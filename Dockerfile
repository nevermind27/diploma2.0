# Используем Ubuntu как базовый образ
FROM ubuntu:22.04

# Установка необходимых пакетов
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    python3 \
    python3-pip \
    git \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Установка зависимостей Python
COPY diploma2/client/requirements.txt /tmp/requirements.txt
RUN pip3 install -r /tmp/requirements.txt

# Копирование всего проекта
COPY diploma2 /app
WORKDIR /app

# Сборка C++ веб-сервера
WORKDIR /app/web-server
# Очищаем все файлы сборки
RUN rm -rf CMakeFiles CMakeCache.txt cmake_install.cmake Makefile final
# Собираем заново
RUN cmake . && make

# Возвращаемся в корневую директорию проекта
WORKDIR /app

# Создаем скрипт для запуска обоих серверов
RUN echo '#!/bin/bash\n\
cd /app/web-server && ./final -p 8080 & \
cd /app/client && python3 client_web.py & \
wait' > /app/start_servers.sh \
&& chmod +x /app/start_servers.sh

# Открываем порты
EXPOSE 8000 8080

# Запускаем серверы
CMD ["/app/start_servers.sh"] 