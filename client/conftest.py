import pytest
import os
import sys
import logging

# Настройка логирования для тестов
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# Добавляем текущую директорию в путь для импорта
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

# Глобальные фикстуры для всех тестов
@pytest.fixture(autouse=True)
def setup_test_environment():
    """Настройка тестового окружения перед каждым тестом"""
    # Создаем временные директории для тестов
    os.makedirs("test_uploads", exist_ok=True)
    os.makedirs("test_tiles", exist_ok=True)
    
    # Сохраняем оригинальные значения
    original_upload_folder = os.environ.get("UPLOAD_FOLDER", "uploads")
    original_tiles_folder = os.environ.get("TILES_FOLDER", "tiles")
    
    # Устанавливаем тестовые значения
    os.environ["UPLOAD_FOLDER"] = "test_uploads"
    os.environ["TILES_FOLDER"] = "test_tiles"
    
    yield
    
    # Восстанавливаем оригинальные значения
    os.environ["UPLOAD_FOLDER"] = original_upload_folder
    os.environ["TILES_FOLDER"] = original_tiles_folder
    
    # Очищаем временные директории
    if os.path.exists("test_uploads"):
        for file in os.listdir("test_uploads"):
            os.remove(os.path.join("test_uploads", file))
        os.rmdir("test_uploads")
    
    if os.path.exists("test_tiles"):
        for file in os.listdir("test_tiles"):
            os.remove(os.path.join("test_tiles", file))
        os.rmdir("test_tiles")
    
    # Очищаем файл конфигурации
    if os.path.exists("server_config.encrypted"):
        os.remove("server_config.encrypted")
    if os.path.exists("server_key.key"):
        os.remove("server_key.key") 