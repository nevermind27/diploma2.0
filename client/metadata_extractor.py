import re
from typing import Dict, Optional
from pathlib import Path
import zipfile
import xml.etree.ElementTree as ET
import logging

logger = logging.getLogger(__name__)

def extract_band(filename: str) -> str:
    """
    Извлекает номер канала из имени файла
    
    Args:
        filename (str): Имя файла
        
    Returns:
        str: Номер канала в формате "B<number>" или пустая строка
    """
    pattern = r"(?i)(?:\bB0*(\d{1,2})\b|\bband0*(\d{1,2})\b)"
    match = re.search(pattern, filename)
    if match:
        num = match.group(1) if match.group(1) else match.group(2)
        return f"B{num}"
    return ""

def detect_satellite(filename: str) -> str:
    """
    Определяет источник (спутник/сенсор) по имени файла
    
    Args:
        filename (str): Имя файла
        
    Returns:
        str: Название спутника/сенсора
    """
    patterns = [
        (r"(?i)\bS2[AB][_\-]", "Sentinel-2"),
        (r"(?i)\bS1[AB][_\-]", "Sentinel-1"),
        (r"(?i)\bLC08[_\-]", "Landsat-8"),
        (r"(?i)\bLE07[_\-]", "Landsat-7"),
        (r"(?i)\bLT05[_\-]", "Landsat-5"),
        (r"(?i)\bWV0[1-4]", "WorldView"),
        (r"(?i)\bSPOT[456]", "SPOT"),
        (r"(?i)\bPSScene", "PlanetScope"),
        (r"(?i)\bMOD0[9][A-Z]", "MODIS"),
        (r"(?i)\bQB[_\-]", "QuickBird"),
        (r"(?i)\bGE[_\-]", "GeoEye")
    ]
    
    for pattern, satellite in patterns:
        if re.search(pattern, filename):
            return satellite
    return "Unknown"

def extract_metadata_from_file(file_path: Path) -> Dict:
    """
    Извлекает метаданные из файла
    
    Args:
        file_path (Path): Путь к файлу
        
    Returns:
        Dict: Словарь с метаданными
    """
    metadata = {
        "filename": file_path.name,
        "source": detect_satellite(file_path.name),
        "band": extract_band(file_path.name),
        # Заглушки для координат и времени
        "timestamp": "2024-04-21T10:30:00Z",
        "north_lat": "55.75",
        "south_lat": "55.70",
        "east_lon": "37.65",
        "west_lon": "37.60"
    }
    return metadata

def extract_metadata_from_archive(archive_path: Path) -> list[Dict]:
    """
    Извлекает метаданные из архива
    
    Args:
        archive_path (Path): Путь к архиву
        
    Returns:
        list[Dict]: Список словарей с метаданными
    """
    metadata_list = []
    
    try:
        with zipfile.ZipFile(archive_path, 'r') as zip_ref:
            # Проверяем наличие manifest.safe
            manifest_files = [f for f in zip_ref.namelist() if f.endswith('manifest.safe')]
            
            if manifest_files:
                # Это SAFE архив
                with zip_ref.open(manifest_files[0]) as manifest:
                    tree = ET.parse(manifest)
                    root = tree.getroot()
                    
                    # Парсим XML для получения информации о файлах
                    namespace = {'safe': 'http://www.esa.int/safe/sentinel-1.0'}
                    data_objects = root.findall('.//safe:dataObject', namespace)
                    
                    for data_object in data_objects:
                        file_location = data_object.find('.//safe:fileLocation', namespace)
                        if file_location is not None:
                            file_name = file_location.get('href')
                            metadata = extract_metadata_from_file(Path(file_name))
                            metadata_list.append(metadata)
            else:
                # Обычный архив
                for file_name in zip_ref.namelist():
                    if file_name.endswith(('.tif', '.jp2', '.img')):
                        metadata = extract_metadata_from_file(Path(file_name))
                        metadata_list.append(metadata)
    
    except Exception as e:
        logger.error(f"Ошибка при обработке архива {archive_path}: {str(e)}")
    
    return metadata_list

def process_input(input_path: str) -> list[Dict]:
    """
    Обрабатывает входные данные (файл, папку или архив)
    
    Args:
        input_path (str): Путь к входным данным
        
    Returns:
        list[Dict]: Список словарей с метаданными
    """
    path = Path(input_path)
    metadata_list = []
    
    if not path.exists():
        logger.error(f"Путь не существует: {input_path}")
        return metadata_list
    
    if path.is_file():
        if path.suffix.lower() in ['.zip', '.tar', '.gz']:
            metadata_list.extend(extract_metadata_from_archive(path))
        else:
            metadata_list.append(extract_metadata_from_file(path))
    elif path.is_dir():
        for file_path in path.rglob('*'):
            if file_path.is_file():
                if file_path.suffix.lower() in ['.tif', '.jp2', '.img']:
                    metadata_list.append(extract_metadata_from_file(file_path))
    
    return metadata_list 