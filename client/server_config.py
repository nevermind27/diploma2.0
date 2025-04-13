import json
from cryptography.fernet import Fernet
import os
import logging

# Настройка логирования
logger = logging.getLogger(__name__)

# Пути к файлам
CONFIG_FILE = "server_config.encrypted"
KEY_FILE = "server_key.key"

def load_key():
    """Загружает ключ шифрования из файла"""
    try:
        if os.path.exists(KEY_FILE):
            with open(KEY_FILE, "rb") as key_file:
                return key_file.read()
        else:
            logger.warning(f"Файл ключа {KEY_FILE} не найден, генерируем новый ключ")
            key = Fernet.generate_key()
            with open(KEY_FILE, "wb") as key_file:
                key_file.write(key)
            return key
    except Exception as e:
        logger.error(f"Ошибка при загрузке ключа: {str(e)}")
        return Fernet.generate_key()

# Инициализация Fernet с загруженным ключом
fernet = Fernet(load_key())

def load_servers():
    """Загружает список серверов из зашифрованного файла"""
    try:
        if os.path.exists(CONFIG_FILE):
            logger.info(f"Загрузка конфигурации серверов из файла {CONFIG_FILE}")
            with open(CONFIG_FILE, 'rb') as f:
                encrypted_data = f.read()
                decrypted_data = fernet.decrypt(encrypted_data)
                servers = json.loads(decrypted_data)
                logger.info(f"Загружено {len(servers)} серверов")
                return servers
        else:
            logger.info("Файл конфигурации не найден, используется список по умолчанию")
            return get_default_servers()
    except Exception as e:
        logger.error(f"Ошибка при загрузке конфигурации: {str(e)}")
        return get_default_servers()

def save_servers(servers):
    """Сохраняет список серверов в зашифрованный файл"""
    try:
        logger.info(f"Сохранение конфигурации {len(servers)} серверов в файл {CONFIG_FILE}")
        encrypted_data = fernet.encrypt(json.dumps(servers).encode())
        with open(CONFIG_FILE, 'wb') as f:
            f.write(encrypted_data)
        logger.info("Конфигурация успешно сохранена")
        return True
    except Exception as e:
        logger.error(f"Ошибка при сохранении конфигурации: {str(e)}")
        return False

def get_default_servers():
    """Возвращает список серверов по умолчанию"""
    logger.info("Использование списка серверов по умолчанию")
    return [
        {
            "url": "http://localhost:8080",
            "priority": 1,
            "name": "Основной сервер",
            "description": "Основной сервер обработки данных"
        },
        {
            "url": "http://localhost:8081",
            "priority": 2,
            "name": "Резервный сервер 1",
            "description": "Первый резервный сервер"
        },
        {
            "url": "http://localhost:8082",
            "priority": 3,
            "name": "Резервный сервер 2",
            "description": "Второй резервный сервер"
        }
    ]

def update_servers(new_servers):
    """Обновляет список серверов"""
    if isinstance(new_servers, list):
        logger.info(f"Обновление списка серверов: {new_servers}")
        return save_servers(new_servers)
    logger.error(f"Некорректный формат списка серверов: {new_servers}")
    return False 