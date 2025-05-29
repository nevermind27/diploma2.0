cmake_minimum_required(VERSION 3.10)
project(min_router)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Находим необходимые пакеты
find_package(OpenCV REQUIRED)
find_package(Torch REQUIRED)
find_package(Casablanca REQUIRED)
find_package(PostgreSQL REQUIRED)

# Добавляем исполняемый файл
add_executable(min_router
    main.cpp
    db_manager.cpp
    sentinel_processor.cpp
    model_inference.cpp
)

# Подключаем заголовочные файлы
target_include_directories(min_router PRIVATE
    ${OpenCV_INCLUDE_DIRS}
    ${TORCH_INCLUDE_DIRS}
    ${Casablanca_INCLUDE_DIRS}
    ${PostgreSQL_INCLUDE_DIRS}
)

# Линкуем библиотеки
target_link_libraries(min_router PRIVATE
    ${OpenCV_LIBS}
    ${TORCH_LIBRARIES}
    ${Casablanca_LIBRARIES}
    ${PostgreSQL_LIBRARIES}
)

# Копируем модель в директорию сборки
configure_file(${CMAKE_SOURCE_DIR}/../best_unet_resnet34.pth
               ${CMAKE_BINARY_DIR}/best_unet_resnet34.pth COPYONLY) 