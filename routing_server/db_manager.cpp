// Поиск изображений по координатам
// SQL: SELECT filename, timestamp, source, north_lat, south_lat, east_lon, west_lon
//      FROM Images WHERE (north_lat BETWEEN south AND north OR south_lat BETWEEN south AND north)
//      AND (east_lon BETWEEN west AND east OR west_lon BETWEEN west AND east);
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
    
    std::string query = form_search_query(north, south, east, west);
    PGresult* res = PQexec(conn, query.c_str());
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        logger.error("Ошибка выполнения запроса: " + std::string(PQerrorMessage(conn)));
        PQclear(res);
        return results;
    }
    
    int rows = PQntuples(res);
    for (int i = 0; i < rows; i++) {
        ImageInfo info;
        info.filename = PQgetvalue(res, i, 0);
        info.timestamp = PQgetvalue(res, i, 1);
        info.source = PQgetvalue(res, i, 2);
        info.north_lat = std::stof(PQgetvalue(res, i, 3));
        info.south_lat = std::stof(PQgetvalue(res, i, 4));
        info.east_lon = std::stof(PQgetvalue(res, i, 5));
        info.west_lon = std::stof(PQgetvalue(res, i, 6));
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
          << "filename, source, timestamp, "
          << "north_lat, south_lat, east_lon, west_lon"
          << ") VALUES ('"
          << data.filename << "', '"
          << data.source << "', '"
          << data.timestamp << "', "
          << data.north_lat << ", "
          << data.south_lat << ", "
          << data.east_lon << ", "
          << data.west_lon
          << ") RETURNING image_id;";
    
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

// Поиск спектров по имени изображения
// SQL: SELECT spectrum_name, frequency, bandwidth FROM Spectrums WHERE image_id = (SELECT image_id FROM Images WHERE filename = 'имя_изображения');
std::vector<SpectrumInfo> DBManager::get_spectrums_by_image(const std::string& image_name) {
    std::vector<SpectrumInfo> results;
    
    if (!conn) {
        logger.error("Нет соединения с базой данных");
        return results;
    }
    
    std::string query = "SELECT spectrum_name, frequency, bandwidth FROM Spectrums "
                       "WHERE image_id = (SELECT image_id FROM Images WHERE filename = '" + 
                       image_name + "');";
    
    PGresult* res = PQexec(conn, query.c_str());
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        logger.error("Ошибка выполнения запроса: " + std::string(PQerrorMessage(conn)));
        PQclear(res);
        return results;
    }
    
    int rows = PQntuples(res);
    for (int i = 0; i < rows; i++) {
        SpectrumInfo info;
        info.spectrum_name = PQgetvalue(res, i, 0);
        info.frequency = std::stof(PQgetvalue(res, i, 1));
        info.bandwidth = std::stof(PQgetvalue(res, i, 2));
        results.push_back(info);
    }
    
    PQclear(res);
    return results;
}

// Поиск изображений по частичному совпадению имени
// SQL: SELECT filename, timestamp, source, north_lat, south_lat, east_lon, west_lon 
//      FROM Images WHERE filename LIKE '%часть_имени%';
std::vector<ImageInfo> DBManager::search_images_by_name(const std::string& partial_name) {
    std::vector<ImageInfo> results;
    
    if (!conn) {
        logger.error("Нет соединения с базой данных");
        return results;
    }
    
    std::string query = "SELECT filename, timestamp, source, north_lat, south_lat, east_lon, west_lon "
                       "FROM Images WHERE filename LIKE '%" + partial_name + "%';";
    
    PGresult* res = PQexec(conn, query.c_str());
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        logger.error("Ошибка выполнения запроса: " + std::string(PQerrorMessage(conn)));
        PQclear(res);
        return results;
    }
    
    int rows = PQntuples(res);
    for (int i = 0; i < rows; i++) {
        ImageInfo info;
        info.filename = PQgetvalue(res, i, 0);
        info.timestamp = PQgetvalue(res, i, 1);
        info.source = PQgetvalue(res, i, 2);
        info.north_lat = std::stof(PQgetvalue(res, i, 3));
        info.south_lat = std::stof(PQgetvalue(res, i, 4));
        info.east_lon = std::stof(PQgetvalue(res, i, 5));
        info.west_lon = std::stof(PQgetvalue(res, i, 6));
        results.push_back(info);
    }
    
    PQclear(res);
    return results;
}

// Получение списка всех доступных изображений
// SQL: SELECT filename, timestamp, source, north_lat, south_lat, east_lon, west_lon FROM Images;
std::vector<ImageInfo> DBManager::get_all_images() {
    std::vector<ImageInfo> results;
    
    if (!conn) {
        logger.error("Нет соединения с базой данных");
        return results;
    }
    
    std::string query = "SELECT filename, timestamp, source, north_lat, south_lat, east_lon, west_lon "
                       "FROM Images;";
    
    PGresult* res = PQexec(conn, query.c_str());
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        logger.error("Ошибка выполнения запроса: " + std::string(PQerrorMessage(conn)));
        PQclear(res);
        return results;
    }
    
    int rows = PQntuples(res);
    for (int i = 0; i < rows; i++) {
        ImageInfo info;
        info.filename = PQgetvalue(res, i, 0);
        info.timestamp = PQgetvalue(res, i, 1);
        info.source = PQgetvalue(res, i, 2);
        info.north_lat = std::stof(PQgetvalue(res, i, 3));
        info.south_lat = std::stof(PQgetvalue(res, i, 4));
        info.east_lon = std::stof(PQgetvalue(res, i, 5));
        info.west_lon = std::stof(PQgetvalue(res, i, 6));
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


