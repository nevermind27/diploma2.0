-- Создаем таблицу для изображений
CREATE TABLE IF NOT EXISTS Images (
    image_id TEXT PRIMARY KEY,
    image_data TEXT NOT NULL,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Создаем таблицу для спектров
CREATE TABLE IF NOT EXISTS Spectrums (
    image_id TEXT REFERENCES Images(image_id) ON DELETE CASCADE,
    spectrum_name TEXT NOT NULL,
    PRIMARY KEY (image_id, spectrum_name)
);

-- Создаем таблицу для соседей в gossip протоколе
CREATE TABLE IF NOT EXISTS Neighbors (
    neighbor_id TEXT PRIMARY KEY,
    address TEXT NOT NULL,
    last_seen TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    is_active BOOLEAN DEFAULT true,
    priority INTEGER DEFAULT 0
);

-- Создаем индексы для оптимизации запросов
CREATE INDEX IF NOT EXISTS idx_images_timestamp ON Images(timestamp);
CREATE INDEX IF NOT EXISTS idx_spectrums_image_id ON Spectrums(image_id);
CREATE INDEX IF NOT EXISTS idx_neighbors_last_seen ON Neighbors(last_seen);
CREATE INDEX IF NOT EXISTS idx_neighbors_is_active ON Neighbors(is_active); 