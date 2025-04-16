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
from pathlib import Path
import xml.etree.ElementTree as ET
from client import Client
import aiohttp

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
            async with aiohttp.ClientSession() as session:
                async with session.get(f"{server['url']}{endpoint}", params=params) as response:
                    response.raise_for_status()
                    data = await response.json()
                    
                    # Проверяем, есть ли в ответе новый список серверов
                    if "servers" in data:
                        logger.info("Получен новый список серверов")
                        update_servers(data["servers"])
                    
                    return data
    except (aiohttp.ClientError, asyncio.TimeoutError) as e:
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
        
        # Обрабатываем архив
        client = Client()
        await process_safe_archive(file_path, client)
        
        return {"message": "File uploaded and processed successfully", "filename": filename}
    except Exception as e:
        logger.error(f"Ошибка при обработке файла: {str(e)}")
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

async def process_safe_archive(file_path: str, client: Client):
    """
    Обработка SAFE архива Sentinel-2
    
    Args:
        file_path: путь к SAFE архиву
        client: клиент для отправки запросов
    """
    try:
        # Создаем временную директорию для распаковки
        temp_dir = Path("temp_extract")
        temp_dir.mkdir(exist_ok=True)
        
        # Распаковываем архив
        with zipfile.ZipFile(file_path, 'r') as zip_ref:
            zip_ref.extractall(temp_dir)
        
        # Читаем метаданные из manifest.safe
        manifest_path = temp_dir / "manifest.safe"
        if not manifest_path.exists():
            raise ValueError("manifest.safe не найден в архиве")
        
        # Парсим XML для получения информации о снимке
        tree = ET.parse(manifest_path)
        root = tree.getroot()
        
        # Получаем информацию о снимке
        namespace = {'safe': 'http://www.esa.int/safe/sentinel-1.0'}
        data_object_section = root.find('.//safe:dataObjectSection', namespace)
        
        if data_object_section is None:
            raise ValueError("Не удалось найти информацию о снимке в manifest.safe")
        
        # Обрабатываем каждый спектр
        for data_object in data_object_section.findall('.//safe:dataObject', namespace):
            # Получаем имя файла и тип данных
            file_name = data_object.find('.//safe:fileLocation', namespace).get('href')
            data_type = data_object.find('.//safe:dataType', namespace).text
            
            # Формируем полный путь к файлу
            file_path = temp_dir / file_name
            
            if not file_path.exists():
                logger.warning(f"Файл не найден: {file_path}")
                continue
            
            # Формируем данные для отправки
            data = {
                "snapshot_id": os.path.basename(file_path).split('.')[0],
                "spectrum": data_type,
                "file_name": os.path.basename(file_path)
            }
            
            # Отправляем файл на сервер через клиент
            try:
                with open(file_path, 'rb') as f:
                    files = {'file': (os.path.basename(file_path), f, 'application/octet-stream')}
                    await client.upload_file(data=data, files=files)
                    logger.info(f"Файл {file_path} успешно отправлен")
            except Exception as e:
                logger.error(f"Ошибка при отправке файла {file_path}: {str(e)}")
        
    except Exception as e:
        logger.error(f"Ошибка при обработке архива: {str(e)}")
        raise
    finally:
        # Очищаем временную директорию
        if temp_dir.exists():
            shutil.rmtree(temp_dir)

@app.get("/available-archives")
async def get_available_archives():
    """
    Получение списка доступных архивов
    """
    # Загружаем список серверов из конфигурации
    servers = load_servers()
    logger.info(f"Загружен список серверов: {servers}")
    
    # Пробуем подключиться к каждому серверу по очереди
    for i, server in enumerate(servers):
        logger.info(f"Попытка подключения к серверу {i+1}/{len(servers)}: {server['url']} (приоритет: {server['priority']})")
        result = await try_server_request(server, "/available-archives", {})
        if result is not None:
            logger.info(f"Успешный ответ от сервера {server['url']}")
            return result
    
    logger.error("Все серверы недоступны")
    raise HTTPException(status_code=503, detail="All servers are unavailable")

@app.get("/view-image/{image_id}")
async def view_image(image_id: str):
    """
    Получение информации о снимке для просмотра
    """
    # Загружаем список серверов из конфигурации
    servers = load_servers()
    logger.info(f"Загружен список серверов: {servers}")
    
    # Пробуем подключиться к каждому серверу по очереди
    for i, server in enumerate(servers):
        logger.info(f"Попытка подключения к серверу {i+1}/{len(servers)}: {server['url']}")
        result = await try_server_request(server, f"/view-image/{image_id}", {})
        if result is not None:
            logger.info(f"Успешный ответ от сервера {server['url']}")
            return result
    
    logger.error("Все серверы недоступны")
    raise HTTPException(status_code=503, detail="All servers are unavailable")

@app.get("/tile/{image_id}")
async def get_tile(image_id: str, x1: float, y1: float, x2: float, y2: float):
    """
    Получение тайла снимка по координатам
    """
    # Загружаем список серверов из конфигурации
    servers = load_servers()
    logger.info(f"Загружен список серверов: {servers}")
    
    params = {
        "x1": x1,
        "y1": y1,
        "x2": x2,
        "y2": y2
    }
    
    # Пробуем подключиться к каждому серверу по очереди
    for i, server in enumerate(servers):
        logger.info(f"Попытка подключения к серверу {i+1}/{len(servers)}: {server['url']}")
        result = await try_server_request(server, f"/tile/{image_id}", params)
        if result is not None:
            logger.info(f"Успешный ответ от сервера {server['url']}")
            return result
    
    logger.error("Все серверы недоступны")
    raise HTTPException(status_code=503, detail="All servers are unavailable")

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000) 