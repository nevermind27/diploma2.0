// Поиск изображений по координатам с использованием geohash
std::vector<ImageInfo> DBManager::search_images(float north, float south, float east, float west) {
    std::vector<ImageInfo> results;
    
    if (!conn) {
        logger.error("Нет соединения с базой данных");
        return results;
    }
    
    if (north <= south || east <= west) {
        logger.error("Некорректные границы области поиска");
        return results;
    }
    
    // Получаем список префиксов geohash для заданной области
    std::vector<std::string> geohash_prefixes = get_geohash_prefixes(north, south, east, west);
    
    std::string query = "SELECT image_id, filename, timestamp, source, geohash "
                       "FROM Images WHERE geohash LIKE ANY ("
                       "SELECT prefix || '%' FROM UNNEST($1::text[]) AS prefix) "
                       "ORDER BY timestamp DESC;";
    
    PGresult* res = PQexecParams(conn, query.c_str(), 1, NULL, 
                                &geohash_prefixes[0], NULL, NULL, 0);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        logger.error("Ошибка выполнения запроса: " + std::string(PQerrorMessage(conn)));
        PQclear(res);
        return results;
    }
    
    int rows = PQntuples(res);
    for (int i = 0; i < rows; i++) {
        ImageInfo info;
        info.image_id = std::stoi(PQgetvalue(res, i, 0));
        info.filename = PQgetvalue(res, i, 1);
        info.timestamp = PQgetvalue(res, i, 2);
        info.source = PQgetvalue(res, i, 3);
        info.geohash = PQgetvalue(res, i, 4);
        results.push_back(info);
    }
    
    PQclear(res);
    return results;
}

// Формирование запроса для вставки нового изображения
// SQL: INSERT INTO Images (filename, source, timestamp, north_lat, south_lat, east_lon, west_lon)
//      VALUES ('filename', 'source', 'timestamp', north_lat, south_lat, east_lon, west_lon)
//      RETURNING image_id;
std::string DBManager::form_insert_image_query(const ImageInsertData& data) {
    std::stringstream query;
    
    query << "INSERT INTO Images ("
          << "filename, source, timestamp, geohash"
          << ") VALUES ('"
          << data.filename << "', '"
          << data.source << "', '"
          << data.timestamp << "', '"
          << data.geohash
          << "') RETURNING image_id;";
    
    return query.str();
}

// Вставка нового изображения в базу данных
// Использует form_insert_image_query для формирования запроса
int DBManager::insert_image(const ImageInsertData& data) {
    if (!conn) {
        logger.error("Нет соединения с базой данных");
        return -1;
    }
    
    if (!validate_coordinates(data.north_lat, data.south_lat, data.east_lon, data.west_lon)) {
        logger.error("Некорректные значения координат");
        return -1;
    }
    
    std::string query = form_insert_image_query(data);
    PGresult* res = PQexec(conn, query.c_str());
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        logger.error("Ошибка выполнения запроса: " + std::string(PQerrorMessage(conn)));
        PQclear(res);
        return -1;
    }
    
    int image_id = std::stoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    return image_id;
}

// Валидация координат для проверки корректности перед вставкой
// Проверяет, что северная широта больше южной, а восточная долгота больше западной
bool DBManager::validate_coordinates(float north, float south, float east, float west) {
    return north > south && east > west;
}

// Получение спектров для изображения
std::vector<SpectrumInfo> DBManager::get_spectrums_by_image(const std::string& image_name) {
    std::vector<SpectrumInfo> results;
    
    if (!conn) {
        logger.error("Нет соединения с базой данных");
        return results;
    }
    
    std::string query = "SELECT img_spectrum_id, spectrum_name, segment_storage, "
                       "default_cold_color, frequency, other_data "
                       "FROM Spectrums WHERE img_id = ("
                       "SELECT image_id FROM Images WHERE filename = $1);";
    
    const char* paramValues[1] = {image_name.c_str()};
    PGresult* res = PQexecParams(conn, query.c_str(), 1, NULL, paramValues, NULL, NULL, 0);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        logger.error("Ошибка выполнения запроса: " + std::string(PQerrorMessage(conn)));
        PQclear(res);
        return results;
    }
    
    int rows = PQntuples(res);
    for (int i = 0; i < rows; i++) {
        SpectrumInfo info;
        info.spectrum_id = std::stoi(PQgetvalue(res, i, 0));
        info.spectrum_name = PQgetvalue(res, i, 1);
        info.segment_storage = std::stoi(PQgetvalue(res, i, 2));
        info.default_cold_color = PQgetvalue(res, i, 3);
        info.frequency = std::stoi(PQgetvalue(res, i, 4));
        info.other_data = PQgetvalue(res, i, 5);
        results.push_back(info);
    }
    
    PQclear(res);
    return results;
}

// Обновление частоты обращения к спектру
bool DBManager::increment_spectrum_frequency(const std::string& image_name, const std::string& spectrum_name) {
    if (!conn) {
        logger.error("Нет соединения с базой данных");
        return false;
    }
    
    std::string query = "WITH updated_spectrums AS ("
                       "UPDATE Spectrums SET frequency = frequency + 1 "
                       "WHERE img_id = (SELECT image_id FROM Images WHERE filename = $1) "
                       "AND spectrum_name = $2 "
                       "RETURNING img_spectrum_id, segment_storage, default_cold_color, frequency) "
                       "SELECT * FROM updated_spectrums;";
    
    const char* paramValues[2] = {image_name.c_str(), spectrum_name.c_str()};
    PGresult* res = PQexecParams(conn, query.c_str(), 2, NULL, paramValues, NULL, NULL, 0);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        logger.error("Ошибка выполнения запроса: " + std::string(PQerrorMessage(conn)));
        PQclear(res);
        return false;
    }
    
    PQclear(res);
    return true;
}

// Поиск изображений по частичному совпадению имени
std::vector<ImageInfo> DBManager::search_images_by_name(const std::string& partial_name) {
    std::vector<ImageInfo> results;
    
    if (!conn) {
        logger.error("Нет соединения с базой данных");
        return results;
    }
    
    std::string query = "SELECT image_id, filename, timestamp, source, geohash "
                       "FROM Images WHERE filename ILIKE '%' || $1 || '%' "
                       "ORDER BY timestamp DESC;";
    
    const char* paramValues[1] = {partial_name.c_str()};
    PGresult* res = PQexecParams(conn, query.c_str(), 1, NULL, paramValues, NULL, NULL, 0);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        logger.error("Ошибка выполнения запроса: " + std::string(PQerrorMessage(conn)));
        PQclear(res);
        return results;
    }
    
    int rows = PQntuples(res);
    for (int i = 0; i < rows; i++) {
        ImageInfo info;
        info.image_id = std::stoi(PQgetvalue(res, i, 0));
        info.filename = PQgetvalue(res, i, 1);
        info.timestamp = PQgetvalue(res, i, 2);
        info.source = PQgetvalue(res, i, 3);
        info.geohash = PQgetvalue(res, i, 4);
        results.push_back(info);
    }
    
    PQclear(res);
    return results;
}

// Получение списка всех доступных изображений
std::vector<ImageInfo> DBManager::get_all_images() {
    std::vector<ImageInfo> results;
    
    if (!conn) {
        logger.error("Нет соединения с базой данных");
        return results;
    }
    
    std::string query = "SELECT image_id, filename, timestamp, source, geohash "
                       "FROM Images ORDER BY timestamp DESC;";
    
    PGresult* res = PQexec(conn, query.c_str());
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        logger.error("Ошибка выполнения запроса: " + std::string(PQerrorMessage(conn)));
        PQclear(res);
        return results;
    }
    
    int rows = PQntuples(res);
    for (int i = 0; i < rows; i++) {
        ImageInfo info;
        info.image_id = std::stoi(PQgetvalue(res, i, 0));
        info.filename = PQgetvalue(res, i, 1);
        info.timestamp = PQgetvalue(res, i, 2);
        info.source = PQgetvalue(res, i, 3);
        info.geohash = PQgetvalue(res, i, 4);
        results.push_back(info);
    }
    
    PQclear(res);
    return results;
}

// Добавление нового спектра для изображения
// SQL: INSERT INTO Spectrums (image_id, spectrum_name, frequency, bandwidth)
//      VALUES (image_id, 'имя_спектра', частота, ширина_полосы)
//      RETURNING spectrum_id;
int DBManager::insert_spectrum(int image_id, const SpectrumInsertData& data) {
    if (!conn) {
        logger.error("Нет соединения с базой данных");
        return -1;
    }
    
    std::stringstream query;
    query << "INSERT INTO Spectrums (image_id, spectrum_name, frequency, bandwidth) "
          << "VALUES (" << image_id << ", '"
          << data.spectrum_name << "', "
          << data.frequency << ", "
          << data.bandwidth
          << ") RETURNING spectrum_id;";
    
    PGresult* res = PQexec(conn, query.str().c_str());
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        logger.error("Ошибка выполнения запроса: " + std::string(PQerrorMessage(conn)));
        PQclear(res);
        return -1;
    }
    
    int spectrum_id = std::stoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    return spectrum_id;
}

// Получение списка серверов по типу хранилища
std::vector<ServerInfo> DBManager::get_servers_by_type(const std::string& storage_type) {
    std::vector<ServerInfo> servers;
    
    if (!conn) {
        logger.error("Нет соединения с базой данных");
        return servers;
    }
    
    std::string query = "SELECT server_id, ssd_fullness, ssd_volume, hdd_volume, "
                       "hdd_fullness, location, class "
                       "FROM Servers WHERE class = $1;";
    
    const char* paramValues[1] = {storage_type.c_str()};
    PGresult* res = PQexecParams(conn, query.c_str(), 1, NULL, paramValues, NULL, NULL, 0);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        logger.error("Ошибка выполнения запроса: " + std::string(PQerrorMessage(conn)));
        PQclear(res);
        return servers;
    }
    
    int rows = PQntuples(res);
    for (int i = 0; i < rows; i++) {
        ServerInfo server;
        server.server_id = std::stoi(PQgetvalue(res, i, 0));
        server.ssd_fullness = std::stoi(PQgetvalue(res, i, 1));
        server.ssd_volume = std::stoi(PQgetvalue(res, i, 2));
        server.hdd_volume = std::stoi(PQgetvalue(res, i, 3));
        server.hdd_fullness = std::stoi(PQgetvalue(res, i, 4));
        server.location = PQgetvalue(res, i, 5);
        server.class_type = PQgetvalue(res, i, 6);
        servers.push_back(server);
    }
    
    PQclear(res);
    return servers;
}

// Добавление нового сервера
int DBManager::insert_server(const ServerInsert& data) {
    if (!conn) {
        logger.error("Нет соединения с базой данных");
        return -1;
    }
    
    std::string query = "INSERT INTO Servers (ssd_fullness, ssd_volume, hdd_volume, "
                       "hdd_fullness, location, class) "
                       "VALUES ($1, $2, $3, $4, $5, $6) "
                       "RETURNING server_id;";
    
    const char* paramValues[6] = {
        std::to_string(data.ssd_fullness).c_str(),
        std::to_string(data.ssd_volume).c_str(),
        std::to_string(data.hdd_volume).c_str(),
        std::to_string(data.hdd_fullness).c_str(),
        data.location.c_str(),
        data.class_type.c_str()
    };
    
    PGresult* res = PQexecParams(conn, query.c_str(), 6, NULL, paramValues, NULL, NULL, 0);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        logger.error("Ошибка выполнения запроса: " + std::string(PQerrorMessage(conn)));
        PQclear(res);
        return -1;
    }
    
    int server_id = std::stoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    return server_id;
}

// Добавление нового маршрутизатора
int DBManager::insert_routing_server(const RoutingServerInsert& data) {
    if (!conn) {
        logger.error("Нет соединения с базой данных");
        return -1;
    }
    
    std::string query = "INSERT INTO Routing_Servers (server_id, adress, priority, geohash_prefix) "
                       "VALUES ($1, $2, $3, $4) "
                       "RETURNING server_id;";
    
    const char* paramValues[4] = {
        std::to_string(data.server_id).c_str(),
        data.adress.c_str(),
        std::to_string(data.priority).c_str(),
        data.geohash_prefix.c_str()
    };
    
    PGresult* res = PQexecParams(conn, query.c_str(), 4, NULL, paramValues, NULL, NULL, 0);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        logger.error("Ошибка выполнения запроса: " + std::string(PQerrorMessage(conn)));
        PQclear(res);
        return -1;
    }
    
    int router_id = std::stoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    return router_id;
}

// Удаление сервера
bool DBManager::delete_server(int id) {
    if (!conn) {
        logger.error("Нет соединения с базой данных");
        return false;
    }
    
    std::string query = "DELETE FROM Servers WHERE server_id = $1;";
    const char* paramValues[1] = {std::to_string(id).c_str()};
    
    PGresult* res = PQexecParams(conn, query.c_str(), 1, NULL, paramValues, NULL, NULL, 0);
    
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        logger.error("Ошибка выполнения запроса: " + std::string(PQerrorMessage(conn)));
        PQclear(res);
        return false;
    }
    
    PQclear(res);
    return true;
}

// Удаление маршрутизатора
bool DBManager::delete_routing_server(int id) {
    if (!conn) {
        logger.error("Нет соединения с базой данных");
        return false;
    }
    
    std::string query = "DELETE FROM Routing_Servers WHERE server_id = $1;";
    const char* paramValues[1] = {std::to_string(id).c_str()};
    
    PGresult* res = PQexecParams(conn, query.c_str(), 1, NULL, paramValues, NULL, NULL, 0);
    
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        logger.error("Ошибка выполнения запроса: " + std::string(PQerrorMessage(conn)));
        PQclear(res);
        return false;
    }
    
    PQclear(res);
    return true;
}

// Добавление нового тайла
int DBManager::insert_tile(const TileInsertData& data) {
    if (!conn) {
        logger.error("Нет соединения с базой данных");
        return -1;
    }
    
    std::stringstream query;
    query << "INSERT INTO Tiles (tile_row, tile_column, spectrum, image_id, tile_url) "
          << "VALUES (" << data.tile_row << ", " 
          << data.tile_column << ", '"
          << data.spectrum << "', "
          << data.image_id << ", '"
          << data.tile_url << "') "
          << "RETURNING tile_id;";
    
    PGresult* res = PQexec(conn, query.str().c_str());
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        logger.error("Ошибка выполнения запроса: " + std::string(PQerrorMessage(conn)));
        PQclear(res);
        return -1;
    }
    
    int tile_id = std::stoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    return tile_id;
}

// Обновление частоты обращения к тайлу
bool DBManager::increment_tile_frequency(int tile_row, int tile_column) {
    if (!conn) {
        logger.error("Нет соединения с базой данных");
        return false;
    }
    
    std::stringstream query;
    query << "UPDATE Tiles SET frequency = frequency + 1 "
          << "WHERE tile_row = " << tile_row 
          << " AND tile_column = " << tile_column << ";";
    
    PGresult* res = PQexec(conn, query.str().c_str());
    
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        logger.error("Ошибка выполнения запроса: " + std::string(PQerrorMessage(conn)));
        PQclear(res);
        return false;
    }
    
    PQclear(res);
    return true;
}


