-- Таблица Images
CREATE TABLE IF NOT EXISTS Images (
    image_id SERIAL PRIMARY KEY,
    filename TEXT NOT NULL,
    source TEXT NOT NULL,
    timestamp TIMESTAMP NOT NULL,
    geohash TEXT NOT NULL
);

-- Таблица Servers
CREATE TABLE IF NOT EXISTS Servers (
    server_id SERIAL PRIMARY KEY,
    ssd_fullness INTEGER CHECK (ssd_fullness >= 0 AND ssd_fullness <= 100),
    ssd_volume INTEGER NOT NULL,
    hdd_volume INTEGER NOT NULL,
    hdd_fullness INTEGER CHECK (hdd_fullness >= 0 AND hdd_fullness <= 100),
    location TEXT NOT NULL,
    class TEXT CHECK (class IN ('hot', 'cold', 'mixed')) NOT NULL
);

-- Таблица segment_staorage (многие-ко-многим: группы сегментов и серверы)
CREATE TABLE IF NOT EXISTS segment_staorage (
    group_of_segmets_id INTEGER NOT NULL,
    server_id INTEGER NOT NULL REFERENCES Servers(server_id) ON DELETE CASCADE,
    storage_type TEXT CHECK (storage_type IN ('hot', 'cold')) NOT NULL,
    PRIMARY KEY (group_of_segmets_id, server_id)
);

-- Таблица Spectrums
CREATE TABLE IF NOT EXISTS Spectrums (
    img_spectrum_id SERIAL PRIMARY KEY,
    img_id INTEGER NOT NULL REFERENCES Images(image_id) ON DELETE CASCADE,
    spectrum_name TEXT NOT NULL,
    segment_storage INTEGER NOT NULL,  -- ссылка на group_of_segmets_id
    default_cold_color TEXT NOT NULL,
    frequency INTEGER DEFAULT 0,
    other_data TEXT
);

-- Таблица Routing_Servers
CREATE TABLE IF NOT EXISTS Routing_Servers (
    server_id INTEGER PRIMARY KEY REFERENCES Servers(server_id) ON DELETE CASCADE,
    adress TEXT NOT NULL,
    priority INTEGER NOT NULL,
    geohash_prefix TEXT NOT NULL
);