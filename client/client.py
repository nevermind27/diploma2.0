import aiohttp
import asyncio
import logging
import zipfile
import io
from pathlib import Path
import tempfile
import shutil

# Настройка логирования
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

class Client:
    """
    Клиент для взаимодействия с серверами.
    Поддерживает отказоустойчивость при сбоях серверов.
    """
    
    def __init__(self, servers=None):
        """
        Инициализация клиента.
        
        Args:
            servers (list): Список URL серверов в порядке приоритета
        """
        self.servers = servers or []
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