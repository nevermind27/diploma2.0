from fastapi import FastAPI, UploadFile, File, Body
from fastapi.responses import FileResponse, JSONResponse
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
import os
import shutil
from datetime import datetime
import requests
import json
from typing import List, Dict, Optional
import asyncio
from fastapi import HTTPException
import logging
from server_config import load_servers, update_servers
import zipfile
import io

# Настройка логирования
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

app = FastAPI()

# Настройка CORS
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Директории для хранения (с поддержкой переменных окружения для тестов)
UPLOAD_FOLDER = os.environ.get("UPLOAD_FOLDER", "uploads")
TILES_FOLDER = os.environ.get("TILES_FOLDER", "tiles")

# Создание необходимых директорий
os.makedirs(UPLOAD_FOLDER, exist_ok=True)
os.makedirs(TILES_FOLDER, exist_ok=True)

# Таймаут для запросов (в секундах)
REQUEST_TIMEOUT = 5

# Модель для запроса поиска по координатам
class SearchArea(BaseModel):
    north: float
    south: float
    east: float
    west: float

# Модель для запроса скачивания архива
class DownloadRequest(BaseModel):
    spectrums: List[str]

async def try_server_request(server: Dict, endpoint: str, params: Dict) -> Dict:
    """Пытается выполнить запрос к серверу с таймаутом"""
    try:
        logger.info(f"Отправка запроса к серверу {server['url']}{endpoint}")
        async with asyncio.timeout(REQUEST_TIMEOUT):
            response = requests.get(f"{server['url']}{endpoint}", params=params)
            response.raise_for_status()
            data = response.json()
            
            # Проверяем, есть ли в ответе новый список серверов
            if "servers" in data:
                logger.info("Получен новый список серверов")
                update_servers(data["servers"])
            
            return data
    except (requests.RequestException, asyncio.TimeoutError) as e:
        logger.error(f"Ошибка при запросе к серверу {server['url']}: {str(e)}")
        return None

@app.get("/")
def get_index():
    """Отдаёт HTML страницу"""
    return FileResponse("index.html")

@app.post("/upload")
async def upload_file(file: UploadFile = File(...)):
    """Обработка загруженного файла"""
    try:
        # Создаем уникальное имя файла
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"{timestamp}_{file.filename}"
        file_path = os.path.join(UPLOAD_FOLDER, filename)
        
        # Сохраняем файл
        with open(file_path, "wb") as buffer:
            shutil.copyfileobj(file.file, buffer)
        
        # TODO: Здесь будет логика обработки загруженного файла
        # Например, распаковка архива и обработка снимков
        
        return {"message": "File uploaded successfully", "filename": filename}
    except Exception as e:
        return {"error": str(e)}

@app.get("/tiles")
def list_tiles(band: str = "B04"):
    """Возвращает список доступных тайлов для указанного канала"""
    if not os.path.exists(TILES_FOLDER):
        return {"tiles": []}
    
    # Получаем список файлов в директории тайлов
    tiles = []
    for file in os.listdir(TILES_FOLDER):
        if file.startswith(f"{band}_"):
            tiles.append(file)
    
    return {"tiles": sorted(tiles)}

@app.get("/tiles/{tile_name}")
def get_tile(tile_name: str):
    """Отдаёт изображение тайла"""
    tile_path = os.path.join(TILES_FOLDER, tile_name)
    if os.path.exists(tile_path):
        return FileResponse(tile_path)
    return {"error": "Tile not found"}

def get_tile_position(tile_name: str):
    """Получает позицию тайла в матрице из его имени"""
    # Формат имени: BAND_row_COL.jpg
    parts = tile_name.split('_')
    if len(parts) != 3:
        return None
    
    try:
        row = int(parts[1])
        col = int(parts[2].split('.')[0])
        return row, col
    except ValueError:
        return None

@app.get("/process")
async def process_request(snapshot_id: str, spectrum: str):
    """
    Обрабатывает запрос к серверам для получения данных по снимку и спектру
    """
    # Загружаем список серверов из конфигурации
    servers = load_servers()
    logger.info(f"Загружен список серверов: {servers}")
    
    params = {
        "snapshot_id": snapshot_id,
        "spectrum": spectrum
    }
    
    logger.info(f"Обработка запроса: snapshot_id={snapshot_id}, spectrum={spectrum}")
    
    # Пробуем подключиться к каждому серверу по очереди
    for i, server in enumerate(servers):
        logger.info(f"Попытка подключения к серверу {i+1}/{len(servers)}: {server['url']} (приоритет: {server['priority']})")
        result = await try_server_request(server, "/process", params)
        if result is not None:
            logger.info(f"Успешный ответ от сервера {server['url']}")
            return result
    
    logger.error("Все серверы недоступны")
    raise HTTPException(status_code=503, detail="All servers are unavailable")

@app.post("/search")
async def search_images(area: SearchArea):
    """
    Поиск снимков по географическим координатам
    """
    # Загружаем список серверов из конфигурации
    servers = load_servers()
    logger.info(f"Загружен список серверов: {servers}")
    
    params = {
        "north": area.north,
        "south": area.south,
        "east": area.east,
        "west": area.west
    }
    
    logger.info(f"Поиск снимков по координатам: {params}")
    
    # Пробуем подключиться к каждому серверу по очереди
    for i, server in enumerate(servers):
        logger.info(f"Попытка подключения к серверу {i+1}/{len(servers)}: {server['url']} (приоритет: {server['priority']})")
        result = await try_server_request(server, "/search", params)
        if result is not None:
            logger.info(f"Успешный ответ от сервера {server['url']}")
            return result
    
    logger.error("Все серверы недоступны")
    raise HTTPException(status_code=503, detail="All servers are unavailable")

@app.post("/download")
async def download_archive(request: DownloadRequest):
    """
    Скачивание архива со снимками выбранных спектров
    """
    # Загружаем список серверов из конфигурации
    servers = load_servers()
    logger.info(f"Загружен список серверов: {servers}")
    
    params = {
        "spectrums": request.spectrums
    }
    
    logger.info(f"Скачивание архива со спектрами: {params}")
    
    # Пробуем подключиться к каждому серверу по очереди
    for i, server in enumerate(servers):
        logger.info(f"Попытка подключения к серверу {i+1}/{len(servers)}: {server['url']} (приоритет: {server['priority']})")
        result = await try_server_request(server, "/download", params)
        if result is not None:
            logger.info(f"Успешный ответ от сервера {server['url']}")
            
            # Создаем ZIP-архив в памяти
            memory_file = io.BytesIO()
            with zipfile.ZipFile(memory_file, 'w', zipfile.ZIP_DEFLATED) as zf:
                # Добавляем файлы в архив
                for spectrum in request.spectrums:
                    # Получаем список тайлов для спектра
                    tiles = list_tiles(spectrum)
                    for tile_name in tiles["tiles"]:
                        tile_path = os.path.join(TILES_FOLDER, tile_name)
                        if os.path.exists(tile_path):
                            # Добавляем файл в архив
                            zf.write(tile_path, tile_name)
            
            # Возвращаем архив
            memory_file.seek(0)
            return FileResponse(
                memory_file,
                media_type="application/zip",
                filename="sentinel2_archive.zip"
            )
    
    logger.error("Все серверы недоступны")
    raise HTTPException(status_code=503, detail="All servers are unavailable")

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000) 