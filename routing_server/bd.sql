-- Таблица изображений
CREATE TABLE IF NOT EXISTS Images (
    image_id SERIAL PRIMARY KEY,
    filename TEXT NOT NULL,
    source TEXT NOT NULL,
    timestamp TIMESTAMP NOT NULL,
    north_lat FLOAT NOT NULL,
    south_lat FLOAT NOT NULL,
    east_lon  FLOAT NOT NULL,
    west_lon  FLOAT NOT NULL
);
-- Индексы для географического поиска
CREATE INDEX IF NOT EXISTS idx_images_bbox ON Images (north_lat, south_lat, east_lon, west_lon);
CREATE INDEX IF NOT EXISTS idx_images_timestamp ON Images (timestamp);

-- Таблица серверов хранения
CREATE TABLE IF NOT EXISTS Servers (
    server_id SERIAL PRIMARY KEY,
    ssd_fullness INTEGER CHECK (ssd_fullness BETWEEN 0 AND 100),
    ssd_volume INTEGER NOT NULL,
    hdd_volume INTEGER NOT NULL,
    hdd_fullness INTEGER CHECK (hdd_fullness BETWEEN 0 AND 100),
    location TEXT NOT NULL,
    class TEXT CHECK (class IN ('hot', 'cold', 'mixed')) NOT NULL
);

-- Таблица групп хранилищ для спектров
CREATE TABLE IF NOT EXISTS segment_storage (
    group_of_segments_id SERIAL PRIMARY KEY
);
-- Сопоставление группы сегментов и серверов
CREATE TABLE IF NOT EXISTS segment_storage_mapping (
    group_of_segments_id INTEGER REFERENCES segment_storage(group_of_segments_id) ON DELETE CASCADE,
    server_id INTEGER REFERENCES Servers(server_id) ON DELETE CASCADE,
    storage_type TEXT CHECK (storage_type IN ('hot', 'cold')) NOT NULL,
    PRIMARY KEY (group_of_segments_id, server_id, storage_type)
);

-- Таблица спектров
CREATE TABLE IF NOT EXISTS Spectrums (
    img_spectrum_id SERIAL PRIMARY KEY,
    img_id INTEGER REFERENCES Images(image_id) ON DELETE CASCADE,
    spectrum_name TEXT NOT NULL,
    segment_storage INTEGER REFERENCES segment_storage(group_of_segments_id) ON DELETE SET NULL,
    default_cold_color TEXT NOT NULL,
    frequency INTEGER DEFAULT 0
);
CREATE INDEX IF NOT EXISTS idx_spectrums_img_id ON Spectrums (img_id);

-- Таблица серверов маршрутизации
CREATE TABLE IF NOT EXISTS Routing_Servers (
    server_id SERIAL PRIMARY KEY,
    adress TEXT NOT NULL,
    priority INTEGER NOT NULL,
    geohash_prefix TEXT NOT NULL,
    last_health_check TIMESTAMP,
    is_alive BOOLEAN DEFAULT TRUE
);
-- Индексы по префиксу и приоритету
CREATE INDEX IF NOT EXISTS idx_routing_prefix ON Routing_Servers (geohash_prefix);
CREATE INDEX IF NOT EXISTS idx_routing_priority ON Routing_Servers (priority);

-- Таблица тайлов (в БД хранения)
CREATE TABLE IF NOT EXISTS Tiles (
    tile_id SERIAL PRIMARY KEY,
    row INTEGER NOT NULL,
    column INTEGER NOT NULL,
    spectrum INTEGER NOT NULL,
    image_id INTEGER NOT NULL REFERENCES Images(image_id) ON DELETE CASCADE,
    tile_bin BYTEA,
    frequency INTEGER DEFAULT 0
);
CREATE INDEX IF NOT EXISTS idx_tiles_spectrum ON Tiles (spectrum);
CREATE INDEX IF NOT EXISTS idx_tiles_image_id ON Tiles (image_id);
