CREATE TABLE IF NOT EXISTS Tiles (
    tile_id SERIAL PRIMARY KEY,
    tile_row INTEGER NOT NULL,
    tile_column INTEGER NOT NULL,
    spectrum TEXT NOT NULL,
    image_id INTEGER NOT NULL,
    tile_url TEXT NOT NULL,
    frequency INTEGER DEFAULT 0
);
