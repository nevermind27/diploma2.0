import pytest
import asyncio
import json
import os
import sys
from unittest.mock import patch, MagicMock, AsyncMock
from aiohttp import web
import aiohttp
import logging
from client import Client
import io
import zipfile

# Добавляем текущую директорию в путь для импорта
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

# Импортируем модули клиента
from client_server import app, process_request, try_server_request, search_images, download_archive
from server_config import load_servers, save_servers, get_default_servers

# Настройка логирования для тестов
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# Тестовые данные
TEST_SNAPSHOT_ID = "test_snapshot_123"
TEST_SPECTRUM = "B04"
TEST_RESPONSE = {
    "data": {
        "tiles": ["B04_1_1.jpg", "B04_1_2.jpg", "B04_2_1.jpg", "B04_2_2.jpg"],
        "metadata": {
            "resolution": "10m",
            "date": "2023-01-01"
        }
    },
    "servers": [
        {"url": "http://localhost:8080", "priority": 1, "name": "Основной сервер"},
        {"url": "http://localhost:8081", "priority": 2, "name": "Резервный сервер 1"}
    ]
}

# Тестовые данные для поиска по координатам
TEST_SEARCH_AREA = {
    "north": 60.0,
    "south": 55.0,
    "east": 40.0,
    "west": 35.0
}

TEST_SEARCH_RESPONSE = {
    "count": 5,
    "images": [
        {"id": "img1", "date": "2023-01-01", "coordinates": {"lat": 57.5, "lon": 37.5}},
        {"id": "img2", "date": "2023-01-02", "coordinates": {"lat": 58.0, "lon": 38.0}},
        {"id": "img3", "date": "2023-01-03", "coordinates": {"lat": 56.5, "lon": 36.5}},
        {"id": "img4", "date": "2023-01-04", "coordinates": {"lat": 59.0, "lon": 39.0}},
        {"id": "img5", "date": "2023-01-05", "coordinates": {"lat": 55.5, "lon": 35.5}}
    ]
}

# Имитаторы серверов
class MockServer:
    def __init__(self, port, delay=0, should_fail=False):
        self.port = port
        self.delay = delay
        self.should_fail = should_fail
        self.app = web.Application()
        self.app.router.add_get('/process', self.handle_process)
        self.app.router.add_post('/search', self.handle_search)
        self.app.router.add_post('/download', self.handle_download)
        self.runner = None
        self.site = None

    async def handle_process(self, request):
        # Имитация задержки
        if self.delay > 0:
            await asyncio.sleep(self.delay)
        
        # Имитация ошибки
        if self.should_fail:
            raise web.HTTPInternalServerError(text="Server error")
        
        return web.json_response(TEST_RESPONSE)
    
    async def handle_search(self, request):
        # Имитация задержки
        if self.delay > 0:
            await asyncio.sleep(self.delay)
        
        # Имитация ошибки
        if self.should_fail:
            raise web.HTTPInternalServerError(text="Server error")
        
        return web.json_response(TEST_SEARCH_RESPONSE)
    
    async def handle_download(self, request):
        # Имитация задержки
        if self.delay > 0:
            await asyncio.sleep(self.delay)
        
        # Имитация ошибки
        if self.should_fail:
            raise web.HTTPInternalServerError(text="Server error")
        
        # Создаем тестовый ZIP-архив
        zip_data = io.BytesIO()
        with zipfile.ZipFile(zip_data, 'w', zipfile.ZIP_DEFLATED) as zf:
            # Добавляем тестовый файл
            zf.writestr("test.txt", "Test data")
        
        zip_data.seek(0)
        return web.Response(
            body=zip_data.getvalue(),
            content_type="application/zip",
            headers={"Content-Disposition": "attachment; filename=test.zip"}
        )

    async def start(self):
        self.runner = web.AppRunner(self.app)
        await self.runner.setup()
        self.site = web.TCPSite(self.runner, 'localhost', self.port)
        await self.site.start()
        logger.info(f"Mock server started on port {self.port}")

    async def stop(self):
        if self.runner:
            await self.runner.cleanup()
            logger.info(f"Mock server on port {self.port} stopped")

# Фикстуры для тестов
@pytest.fixture
async def mock_servers():
    # Создаем три сервера: один с задержкой, один с ошибкой, один нормальный
    server1 = MockServer(8080, delay=0.1, should_fail=False)
    server2 = MockServer(8081, delay=0, should_fail=False)
    server3 = MockServer(8082, delay=0, should_fail=True)
    
    # Запускаем серверы
    await server1.start()
    await server2.start()
    await server3.start()
    
    # Возвращаем серверы для использования в тестах
    yield [server1, server2, server3]
    
    # Останавливаем серверы после тестов
    await server1.stop()
    await server2.stop()
    await server3.stop()

@pytest.fixture
def test_config():
    # Создаем тестовую конфигурацию
    config = [
        {"url": "http://localhost:8080", "priority": 1, "name": "Сервер 1"},
        {"url": "http://localhost:8081", "priority": 2, "name": "Сервер 2"},
        {"url": "http://localhost:8082", "priority": 3, "name": "Сервер 3"}
    ]
    
    # Сохраняем конфигурацию
    save_servers(config)
    
    yield config
    
    # Очищаем конфигурацию после тестов
    if os.path.exists("server_config.encrypted"):
        os.remove("server_config.encrypted")

@pytest.fixture
def client():
    return Client(servers=["http://localhost:8001", "http://localhost:8002", "http://localhost:8003"])

# Тесты
@pytest.mark.asyncio
async def test_all_servers_working(client):
    with patch('aiohttp.ClientSession.get') as mock_get:
        mock_get.return_value.__aenter__.return_value.status = 200
        mock_get.return_value.__aenter__.return_value.json = AsyncMock(return_value={"status": "ok"})
        
        result = await client.get_data()
        assert result == {"status": "ok"}
        assert mock_get.call_count == 1

@pytest.mark.asyncio
async def test_first_server_timeout(client):
    with patch('aiohttp.ClientSession.get') as mock_get:
        mock_get.return_value.__aenter__.side_effect = asyncio.TimeoutError()
        
        result = await client.get_data()
        assert result == {"status": "ok"}
        assert mock_get.call_count == 2

@pytest.mark.asyncio
async def test_first_server_error(client):
    with patch('aiohttp.ClientSession.get') as mock_get:
        mock_get.return_value.__aenter__.side_effect = aiohttp.ClientError()
        
        result = await client.get_data()
        assert result == {"status": "ok"}
        assert mock_get.call_count == 2

@pytest.mark.asyncio
async def test_all_servers_fail(client):
    with patch('aiohttp.ClientSession.get') as mock_get:
        mock_get.return_value.__aenter__.side_effect = aiohttp.ClientError()
        
        with pytest.raises(Exception) as exc_info:
            await client.get_data()
        assert "All servers are unavailable" in str(exc_info.value)
        assert mock_get.call_count == 3

# Тесты для поиска по координатам
@pytest.mark.asyncio
async def test_search_images_success(mock_servers, test_config):
    """Тест: успешный поиск снимков по координатам"""
    # Патчим функцию try_server_request, чтобы использовать наш клиент
    with patch('client_server.try_server_request', side_effect=try_server_request):
        # Вызываем функцию search_images
        response = await search_images(TEST_SEARCH_AREA)
        
        # Проверяем, что ответ соответствует ожидаемому
        assert response == TEST_SEARCH_RESPONSE

@pytest.mark.asyncio
async def test_search_images_all_servers_fail(mock_servers, test_config):
    """Тест: все серверы не отвечают при поиске снимков"""
    # Изменяем все серверы, чтобы они не отвечали
    for server in mock_servers:
        server.should_fail = True
    
    # Патчим функцию try_server_request, чтобы использовать наш клиент
    with patch('client_server.try_server_request', side_effect=try_server_request):
        # Вызываем функцию search_images и ожидаем исключение
        with pytest.raises(Exception) as excinfo:
            await search_images(TEST_SEARCH_AREA)
        
        # Проверяем, что исключение содержит правильное сообщение
        assert "All servers are unavailable" in str(excinfo.value)

# Тесты для скачивания архивов
@pytest.mark.asyncio
async def test_download_archive_success(mock_servers, test_config):
    """Тест: успешное скачивание архива"""
    # Патчим функцию try_server_request, чтобы использовать наш клиент
    with patch('client_server.try_server_request', side_effect=try_server_request):
        # Вызываем функцию download_archive
        response = await download_archive({"spectrums": ["B04", "B08"]})
        
        # Проверяем, что ответ содержит ZIP-архив
        assert response.headers["content-type"] == "application/zip"
        assert "attachment" in response.headers["content-disposition"]

@pytest.mark.asyncio
async def test_download_archive_all_servers_fail(mock_servers, test_config):
    """Тест: все серверы не отвечают при скачивании архива"""
    # Изменяем все серверы, чтобы они не отвечали
    for server in mock_servers:
        server.should_fail = True
    
    # Патчим функцию try_server_request, чтобы использовать наш клиент
    with patch('client_server.try_server_request', side_effect=try_server_request):
        # Вызываем функцию download_archive и ожидаем исключение
        with pytest.raises(Exception) as excinfo:
            await download_archive({"spectrums": ["B04", "B08"]})
        
        # Проверяем, что исключение содержит правильное сообщение
        assert "All servers are unavailable" in str(excinfo.value)

if __name__ == "__main__":
    pytest.main(["-v", "test_client.py"]) 