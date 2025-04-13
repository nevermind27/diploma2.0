import json
from cryptography.fernet import Fernet
import os

# Генерация ключа шифрования
key = Fernet.generate_key()
fernet = Fernet(key)

# Сохранение ключа в файл (в реальном приложении хранить безопасно)
with open("server_key.key", "wb") as key_file:
    key_file.write(key)

# Загрузка конфигурации
with open("server_config.json", "r") as config_file:
    config = json.load(config_file)

# Шифрование конфигурации
encrypted_config = fernet.encrypt(json.dumps(config).encode())

# Сохранение зашифрованной конфигурации
with open("server_config.encrypted", "wb") as encrypted_file:
    encrypted_file.write(encrypted_config)

print("Конфигурация успешно зашифрована")
print(f"Ключ сохранен в файл: server_key.key")
print(f"Зашифрованная конфигурация сохранена в файл: server_config.encrypted") 