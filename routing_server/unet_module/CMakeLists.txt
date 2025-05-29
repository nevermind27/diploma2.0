cmake_minimum_required(VERSION 3.0 FATAL_ERROR)
project(UnetTest)

find_package(OpenCV REQUIRED)
find_package(Torch REQUIRED)

add_executable(test_unet test_unet.cpp unet_module.cpp)
target_link_libraries(test_unet "${TORCH_LIBRARIES}" ${OpenCV_LIBS})
set_property(TARGET test_unet PROPERTY CXX_STANDARD 17)

# Добавляем пути включения
target_include_directories(test_unet PRIVATE
    ${OpenCV_INCLUDE_DIRS}
    ${TORCH_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
)
