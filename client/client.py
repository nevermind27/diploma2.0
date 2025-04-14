import aiohttp
import asyncio
import logging

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
    
    async def download_archive(self, spectrums):
        """
        Скачивание архива с выбранными спектрами.
        
        Args:
            spectrums (list): Список выбранных спектров
            
        Returns:
            bytes: Данные архива
        """
        response = await self.get_data(endpoint="/download", params={"spectrums": spectrums})
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