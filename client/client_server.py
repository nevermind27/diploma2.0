from fastapi import FastAPI, UploadFile, File
from fastapi.responses import FileResponse
from fastapi.middleware.cors import CORSMiddleware
import os
import shutil
from datetime import datetime

app = FastAPI()

# Настройка CORS
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Директории для хранения
UPLOAD_FOLDER = "uploads"
TILES_FOLDER = "tiles"

# Создание необходимых директорий
os.makedirs(UPLOAD_FOLDER, exist_ok=True)
os.makedirs(TILES_FOLDER, exist_ok=True)

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

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000) 