# Используем Ubuntu 22.04 как базовый образ
FROM ubuntu:22.04

# Устанавливаем переменные окружения
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Moscow

# Устанавливаем необходимые пакеты
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    wget \
    unzip \
    libopencv-dev \
    libpq-dev \
    libcpprest-dev \
    python3 \
    python3-pip \
    && rm -rf /var/lib/apt/lists/*

# Устанавливаем PyTorch
RUN pip3 install torch torchvision

# Создаем рабочую директорию
WORKDIR /app

# Копируем исходный код
COPY . /app/

# Создаем директорию для сборки
RUN mkdir build && cd build && \
    cmake .. && \
    make

# Создаем директорию для архивов Sentinel-2
RUN mkdir -p /app/sentinel2_archive

# Открываем порт для HTTP-запросов
EXPOSE 8080

# Настройка сети
ENV POSTGRES_HOST=postgres
ENV POSTGRES_PORT=5432
ENV POSTGRES_DB=router_db
ENV POSTGRES_USER=postgres
ENV POSTGRES_PASSWORD=postgres

# Запускаем приложение
CMD ["./build/min_router"] 