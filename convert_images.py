from PIL import Image
import os

def convert_tiff_to_png(tiff_path, png_path):
    try:
        # Открываем TIFF файл
        with Image.open(tiff_path) as img:
            # Конвертируем в PNG
            img.save(png_path, 'PNG')
        print(f"Успешно конвертировано: {tiff_path} -> {png_path}")
    except Exception as e:
        print(f"Ошибка при конвертации {tiff_path}: {str(e)}")

# Пути к исходным файлам
tiff_image = "/Users/elizavetaomelcenko/AerialImageDataset/train/images/tyrol-w34.tif"
tiff_mask = "/Users/elizavetaomelcenko/AerialImageDataset/train/gt/tyrol-w34.tif"

# Пути для сохранения PNG файлов
png_image = "tyrol-w34.png"
png_mask = "tyrol-w34-mask.png"

# Конвертируем файлы
convert_tiff_to_png(tiff_image, png_image)
convert_tiff_to_png(tiff_mask, png_mask) 