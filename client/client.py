import aiohttp
import asyncio
import logging
import zipfile
import io
from pathlib import Path
import tempfile
import shutil
import json
from typing import List, Dict, Optional
import os

# Настройка логирования
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

class Client:
    """
    Клиент для взаимодействия с серверами.
    Поддерживает отказоустойчивость при сбоях серверов.
    """
    
    def __init__(self, base_url: str):
        """
        Инициализация клиента.
        
        Args:
            base_url (str): Базовый URL сервера
        """
        self.base_url = base_url.rstrip('/')
        self.session = None
    
    async def __aenter__(self):
        """Создание сессии при входе в контекстный менеджер"""
        self.session = aiohttp.ClientSession()
        return self
    
    async def __aexit__(self, exc_type, exc_val, exc_tb):
        """Закрытие сессии при выходе из контекстного менеджера"""
        if self.session:
            await self.session.close()
    
    async def get_data(self, endpoint="/process", params=None, timeout=5):
        """
        Получение данных с серверов с поддержкой отказоустойчивости.
        
        Args:
            endpoint (str): Эндпоинт API
            params (dict): Параметры запроса
            timeout (int): Таймаут запроса в секундах
            
        Returns:
            dict: Ответ от сервера
            
        Raises:
            Exception: Если все серверы недоступны
        """
        if not self.session:
            self.session = aiohttp.ClientSession()
        
        last_error = None
        
        for server_url in self.servers:
            try:
                url = f"{server_url}{endpoint}"
                logger.info(f"Запрос к серверу: {url}")
                
                async with self.session.get(url, params=params, timeout=timeout) as response:
                    if response.status == 200:
                        return await response.json()
                    else:
                        logger.warning(f"Сервер {server_url} вернул код {response.status}")
                        last_error = f"Server returned status code {response.status}"
            except asyncio.TimeoutError:
                logger.warning(f"Таймаут при запросе к серверу {server_url}")
                last_error = "Request timeout"
            except aiohttp.ClientError as e:
                logger.warning(f"Ошибка при запросе к серверу {server_url}: {str(e)}")
                last_error = str(e)
            except Exception as e:
                logger.error(f"Неожиданная ошибка при запросе к серверу {server_url}: {str(e)}")
                last_error = str(e)
        
        raise Exception(f"All servers are unavailable. Last error: {last_error}")
    
    async def search_images(self, area):
        """
        Поиск изображений по заданной области.
        
        Args:
            area (dict): Словарь с координатами области (north, south, east, west)
            
        Returns:
            dict: Результаты поиска
        """
        return await self.get_data(endpoint="/search", params=area)
    
    async def get_available_archives(self, search_query: str = None) -> dict:
        """
        Получение списка доступных архивов.
        
        Args:
            search_query (str, optional): Поисковый запрос для фильтрации архивов
            
        Returns:
            dict: Словарь с информацией об архивах
        """
        params = {}
        if search_query:
            params['search'] = search_query
            
        return await self.get_data(endpoint="/available-archives", params=params)
    
    async def get_archive_info(self, archive_id: str) -> dict:
        """
        Получение информации о конкретном архиве.
        
        Args:
            archive_id (str): ID архива
            
        Returns:
            dict: Информация об архиве
        """
        return await self.get_data(endpoint=f"/archive/{archive_id}")
    
    async def download_archive(self, archive_id: str, spectrums: list) -> bytes:
        """
        Скачивание архива с выбранными спектрами.
        
        Args:
            archive_id (str): ID архива
            spectrums (list): Список выбранных спектров
            
        Returns:
            bytes: Данные архива в формате ZIP
        """
        data = {
            "archive_id": archive_id,
            "spectrums": spectrums
        }
        
        response = await self.get_data(endpoint="/download", params=data)
        return response
    
    async def upload_file(self, endpoint="/upload", data=None, files=None, timeout=30):
        """
        Отправка файла на сервер с поддержкой отказоустойчивости.
        
        Args:
            endpoint (str): Эндпоинт API
            data (dict): Данные для отправки
            files (dict): Файлы для отправки
            timeout (int): Таймаут запроса в секундах
            
        Returns:
            dict: Ответ от сервера
            
        Raises:
            Exception: Если все серверы недоступны
        """
        if not self.session:
            self.session = aiohttp.ClientSession()
        
        last_error = None
        
        for server_url in self.servers:
            try:
                url = f"{server_url}{endpoint}"
                logger.info(f"Отправка файла на сервер: {url}")
                
                # Создаем FormData для отправки файлов
                form_data = aiohttp.FormData()
                if data:
                    for key, value in data.items():
                        form_data.add_field(key, str(value))
                if files:
                    for key, (filename, fileobj, content_type) in files.items():
                        form_data.add_field(key, fileobj, filename=filename, content_type=content_type)
                
                async with self.session.post(url, data=form_data, timeout=timeout) as response:
                    if response.status == 200:
                        return await response.json()
                    else:
                        logger.warning(f"Сервер {server_url} вернул код {response.status}")
                        last_error = f"Server returned status code {response.status}"
            except asyncio.TimeoutError:
                logger.warning(f"Таймаут при отправке файла на сервер {server_url}")
                last_error = "Request timeout"
            except aiohttp.ClientError as e:
                logger.warning(f"Ошибка при отправке файла на сервер {server_url}: {str(e)}")
                last_error = str(e)
            except Exception as e:
                logger.error(f"Неожиданная ошибка при отправке файла на сервер {server_url}: {str(e)}")
                last_error = str(e)
        
        raise Exception(f"All servers are unavailable. Last error: {last_error}")
    
    async def get_tile(self, image_id: str, x1: float, y1: float, x2: float, y2: float) -> bytes:
        """
        Получение тайла снимка по координатам
        
        Args:
            image_id (str): ID снимка
            x1 (float): X координата верхнего левого угла
            y1 (float): Y координата верхнего левого угла
            x2 (float): X координата нижнего правого угла
            y2 (float): Y координата нижнего правого угла
            
        Returns:
            bytes: Данные тайла
        """
        params = {
            "x1": x1,
            "y1": y1,
            "x2": x2,
            "y2": y2
        }
        
        response = await self.get_data(endpoint=f"/tile/{image_id}", params=params)
        return response
    
    async def view_image(self, image_id: str) -> dict:
        """
        Получение информации о снимке для просмотра
        
        Args:
            image_id (str): ID снимка
            
        Returns:
            dict: Информация о снимке
        """
        return await self.get_data(endpoint=f"/view-image/{image_id}")
    
    async def get_all_images(self) -> List[Dict]:
        """
        Получение списка всех доступных снимков
        GET /images
        """
        try:
            async with self.session.get(f"{self.base_url}/images") as response:
                if response.status == 200:
                    return await response.json()
                else:
                    logger.error(f"Ошибка при получении списка изображений: {response.status}")
                    return []
        except Exception as e:
            logger.error(f"Ошибка при запросе списка изображений: {str(e)}")
            return []
    
    async def search_images_by_coordinates(self, min_lat: float, max_lat: float, 
                                         min_lon: float, max_lon: float) -> List[Dict]:
        """
        Поиск снимка в определённой области (по координатам)
        GET /images/search?min_lat=55.5&max_lat=56.0&min_lon=37.2&max_lon=38.1
        """
        try:
            params = {
                "min_lat": min_lat,
                "max_lat": max_lat,
                "min_lon": min_lon,
                "max_lon": max_lon
            }
            async with self.session.get(f"{self.base_url}/images/search", params=params) as response:
                if response.status == 200:
                    return await response.json()
                else:
                    logger.error(f"Ошибка при поиске изображений по координатам: {response.status}")
                    return []
        except Exception as e:
            logger.error(f"Ошибка при поиске изображений по координатам: {str(e)}")
            return []
    
    async def search_images_by_name(self, name: str) -> List[Dict]:
        """
        Поиск по названию снимка
        GET /images/search?name=landsat_2024_04_10
        """
        try:
            params = {"name": name}
            async with self.session.get(f"{self.base_url}/images/search", params=params) as response:
                if response.status == 200:
                    return await response.json()
                else:
                    logger.error(f"Ошибка при поиске изображений по имени: {response.status}")
                    return []
        except Exception as e:
            logger.error(f"Ошибка при поиске изображений по имени: {str(e)}")
            return []
    
    async def get_image_with_spectrums(self, image_id: str, bands: List[str]) -> Optional[Dict]:
        """
        Получение снимка с выбранными спектрами
        GET /images/12345?bands=R,G,B
        """
        try:
            params = {"bands": ",".join(bands)}
            async with self.session.get(f"{self.base_url}/images/{image_id}", params=params) as response:
                if response.status == 200:
                    return await response.json()
                else:
                    logger.error(f"Ошибка при получении изображения со спектрами: {response.status}")
                    return None
        except Exception as e:
            logger.error(f"Ошибка при получении изображения со спектрами: {str(e)}")
            return None
    
    async def upload_image(self, file_path: str, metadata: Dict, headers: Dict = None) -> bool:
        """
        Отправляет файл на сервер с метаданными
        
        Args:
            file_path: путь к файлу
            metadata: словарь с метаданными
            headers: дополнительные HTTP-заголовки
            
        Returns:
            bool: True если загрузка успешна, False в противном случае
        """
        try:
            # Загружаем список серверов
            servers = load_servers()
            
            # Подготавливаем данные для отправки
            data = {
                "snapshot_id": metadata.get("snapshot_id", ""),
                "spectrum": metadata.get("band", ""),
                "timestamp": metadata.get("timestamp", ""),
                "coordinates": {
                    "north": metadata.get("north_lat", ""),
                    "south": metadata.get("south_lat", ""),
                    "east": metadata.get("east_lon", ""),
                    "west": metadata.get("west_lon", "")
                }
            }
            
            # Открываем файл
            with open(file_path, 'rb') as f:
                files = {'file': (os.path.basename(file_path), f, 'application/octet-stream')}
                
                # Пробуем отправить на каждый сервер
                for server in servers:
                    try:
                        url = f"{server['url']}/upload"
                        
                        # Добавляем базовые заголовки
                        request_headers = {
                            "X-Spectrum": metadata.get("band", "")
                        }
                        
                        # Добавляем дополнительные заголовки, если они есть
                        if headers:
                            request_headers.update(headers)
                        
                        async with aiohttp.ClientSession() as session:
                            async with session.post(url, data=data, files=files, headers=request_headers) as response:
                                if response.status == 200:
                                    return True
                    except Exception as e:
                        logger.error(f"Ошибка при отправке на сервер {server['url']}: {str(e)}")
                        continue
            
            return False
        except Exception as e:
            logger.error(f"Ошибка при загрузке файла: {str(e)}")
            return False

# Пример использования:
async def main():
    async with Client("http://localhost:8080") as client:
        # Получение списка всех изображений
        images = await client.get_all_images()
        print("Все изображения:", images)
        
        # Поиск по координатам
        search_results = await client.search_images_by_coordinates(55.5, 56.0, 37.2, 38.1)
        print("Результаты поиска по координатам:", search_results)
        
        # Поиск по имени
        name_results = await client.search_images_by_name("landsat_2024_04_10")
        print("Результаты поиска по имени:", name_results)
        
        # Получение изображения со спектрами
        image = await client.get_image_with_spectrums("12345", ["R", "G", "B"])
        print("Изображение со спектрами:", image)
        
        # Загрузка нового изображения
        metadata = {
            "source": "landsat",
            "timestamp": "2024-04-10T12:00:00Z",
            "north_lat": 56.0,
            "south_lat": 55.5,
            "east_lon": 38.1,
            "west_lon": 37.2
        }
        upload_result = await client.upload_image("path/to/image.tif", metadata)
        print("Результат загрузки:", upload_result)

if __name__ == "__main__":
    asyncio.run(main()) 