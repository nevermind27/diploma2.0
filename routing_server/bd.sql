-- Создание таблицы Images
CREATE TABLE IF NOT EXISTS Images (
    image_id SERIAL PRIMARY KEY,
    filename TEXT NOT NULL,
    source TEXT NOT NULL,
    timestamp TIMESTAMP NOT NULL,
    north_lat FLOAT NOT NULL,  -- северная широта
    south_lat FLOAT NOT NULL,  -- южная широта
    east_lon FLOAT NOT NULL,   -- восточная долгота
    west_lon FLOAT NOT NULL 
);

-- Создание таблицы Servers
CREATE TABLE IF NOT EXISTS Servers (
    server_id SERIAL PRIMARY KEY,
    ssd_fullness INTEGER CHECK (ssd_fullness >= 0 AND ssd_fullness <= 100),
    ssd_volume INTEGER NOT NULL,
    hdd_volume INTEGER NOT NULL,
    hdd_fullness INTEGER CHECK (hdd_fullness >= 0 AND hdd_fullness <= 100),
    location TEXT NOT NULL,
    class TEXT CHECK (class IN ('hot', 'cold', 'mixed')) NOT NULL
);

-- Создание таблицы Spectrums
CREATE TABLE IF NOT EXISTS Spectrums (
    img_spectrum_id SERIAL PRIMARY KEY,
    img_id INTEGER REFERENCES Images(image_id) ON DELETE CASCADE,
    spectrum_name TEXT NOT NULL,
    hot_storage INTEGER REFERENCES Servers(server_id) ON DELETE SET NULL,
    cold_storage INTEGER REFERENCES Servers(server_id) ON DELETE SET NULL,
    default_cold_color TEXT NOT NULL,
    frequency INTEGER DEFAULT 0
);

-- Создание таблицы Routing_Servers
CREATE TABLE IF NOT EXISTS Routing_Servers (
    server_id SERIAL PRIMARY KEY,
    adress TEXT NOT NULL,
    priority INTEGER NOT NULL,
    last_health_check TIMESTAMP,
    is_alive BOOLEAN DEFAULT TRUE
);
