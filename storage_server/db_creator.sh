#!/bin/bash

DB_NAME="tiles_db"
TABLE_SQL="create_tiles.sql"

# 1. Установка PostgreSQL
install_postgres() {
  if command -v psql > /dev/null; then
    echo "PostgreSQL уже установлен ✅"
  else
    echo "Устанавливаю PostgreSQL..."
    if [[ "$OSTYPE" == "darwin"* ]]; then
      brew install postgresql
      brew services start postgresql
    elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
      sudo apt update
      sudo apt install -y postgresql postgresql-contrib
      sudo service postgresql start
    else
      echo "❌ Неизвестная ОС. Установка не поддерживается автоматически."
      exit 1
    fi
  fi
}

# 2. Создание базы данных и таблицы
create_db_and_table() {
  echo "Создаю базу данных $DB_NAME и таблицу Tiles..."
  psql postgres -c "CREATE DATABASE $DB_NAME;" 2>/dev/null || echo "БД уже существует"
  psql $DB_NAME -f "$TABLE_SQL"
  echo "✅ База и таблица готовы."
}

# 3. Основной запуск
install_postgres
cat > create_tiles.sql <<EOF
$(
  cat <<SQL
CREATE TABLE IF NOT EXISTS Tiles (
    tile_id SERIAL PRIMARY KEY,
    tile_row INTEGER NOT NULL,
    tile_column INTEGER NOT NULL,
    spectrum TEXT NOT NULL,
    image_id INTEGER NOT NULL,
    tile_url TEXT NOT NULL,
    frequency INTEGER DEFAULT 0
);
SQL
)
EOF

create_db_and_table
