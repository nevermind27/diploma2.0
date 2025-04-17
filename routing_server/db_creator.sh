#!/bin/bash

DB_NAME="routing_db"
TABLE_SQL="bd.sql"

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

# 2. Создание базы данных и таблиц
create_db_and_tables() {
  echo "Создаю базу данных $DB_NAME и таблицы..."
  psql postgres -c "CREATE DATABASE $DB_NAME;" 2>/dev/null || echo "БД уже существует"
  psql $DB_NAME -f "$TABLE_SQL"
  echo "✅ База и таблицы готовы."
}

# 3. Основной запуск
install_postgres
create_db_and_tables 