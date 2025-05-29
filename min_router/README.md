# Мини-роутер для обработки Sentinel-2 изображений

Мини-роутер представляет собой сервис для обработки изображений Sentinel-2, создания бинарных масок с помощью обученной модели и обмена данными между узлами через gossip протокол.

## Функциональность

- Обработка HTTP-запросов (GET, PUT, POST)
- Работа с базой данных PostgreSQL
- Обработка Sentinel-2 изображений
- Создание бинарных масок с помощью модели UNet-ResNet34
- Обмен данными через gossip протокол

## Требования

- Docker и Docker Compose
- Доступ к портам 8080 (HTTP) и 5432 (PostgreSQL)
- Модель PyTorch (best_unet_resnet34.pth)
- Архив Sentinel-2 изображений

## Структура проекта

```
min_router/
├── Dockerfile              # Конфигурация Docker
├── docker-compose.yml      # Конфигурация Docker Compose
├── init.sql               # Скрипт инициализации БД
├── CMakeLists.txt         # Конфигурация сборки
├── main.cpp              # Основной файл сервера
├── db_manager.h          # Заголовочный файл для работы с БД
├── db_manager.cpp        # Реализация работы с БД
├── sentinel_processor.h  # Заголовочный файл для обработки Sentinel-2
├── sentinel_processor.cpp # Реализация обработки Sentinel-2
├── model_inference.h     # Заголовочный файл для работы с моделью
├── model_inference.cpp   # Реализация работы с моделью
├── sentinel2_archive/    # Директория для архивов Sentinel-2
└── best_unet_resnet34.pth # Обученная модель PyTorch
```

## Сборка и запуск

1. Клонируйте репозиторий:
```bash
git clone <repository-url>
cd min_router
```

2. Поместите модель PyTorch в корневую директорию:
```bash
cp /path/to/best_unet_resnet34.pth .
```

3. Создайте директорию для архивов Sentinel-2:
```bash
mkdir sentinel2_archive
```

4. Запустите сервисы с помощью Docker Compose:
```bash
docker-compose up --build
```

## API Endpoints

### Получение изображения и создание маски
```
GET /?id=<image_id>
```
Возвращает изображение и созданную бинарную маску.

### Сохранение изображения
```
PUT /
Content-Type: application/json

{
    "image_id": "string",
    "image_data": "string"
}
```

### Обработка gossip сообщений
```
POST /gossip
Content-Type: application/json

{
    "neighbor_id": "string",
    "address": "string",
    "message": "string"
}
```

## База данных

База данных содержит следующие таблицы:
- `Images` - хранит информацию об изображениях
- `Spectrums` - хранит информацию о спектрах
- `Neighbors` - хранит информацию о соседних узлах в gossip протоколе

## Мониторинг

- HTTP-сервер доступен на порту 8080
- PostgreSQL доступен на порту 5432
- Логи доступны через `docker-compose logs`

## Остановка

Для остановки сервисов выполните:
```bash
docker-compose down
```

Для полной очистки данных (включая базу данных):
```bash
docker-compose down -v
``` 