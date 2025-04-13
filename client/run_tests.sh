#!/bin/bash

# Переходим в директорию клиента
cd "$(dirname "$0")"

# Создаем и активируем виртуальное окружение
python3 -m venv venv
source venv/bin/activate

# Установка зависимостей
pip3 install -r requirements.txt

# Запуск тестов
pytest -v test_client.py

# Деактивируем виртуальное окружение
deactivate 