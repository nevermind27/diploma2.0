#include <iostream>
#include <string>
#include <vector>
#include <libpq-fe.h>
#include <stdexcept>

class DBManager {
private:
    PGconn* conn;
    std::string dbname;
    std::string user;
    std::string password;
    std::string host;
    std::string port;

    void executeQuery(const std::string& query) {
        PGresult* res = PQexec(conn, query.c_str());
        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            std::string error = PQerrorMessage(conn);
            PQclear(res);
            throw std::runtime_error("Ошибка выполнения запроса: " + error);
        }
        PQclear(res);
    }

public:
    DBManager(const std::string& dbname, 
              const std::string& user = "postgres",
              const std::string& password = "",
              const std::string& host = "localhost",
              const std::string& port = "5432")
        : dbname(dbname), user(user), password(password), host(host), port(port) {
        
        std::string conninfo = "dbname=" + dbname + 
                              " user=" + user + 
                              " password=" + password + 
                              " host=" + host + 
                              " port=" + port;
        
        conn = PQconnectdb(conninfo.c_str());
        if (PQstatus(conn) != CONNECTION_OK) {
            std::string error = PQerrorMessage(conn);
            PQfinish(conn);
            throw std::runtime_error("Ошибка подключения к базе данных: " + error);
        }
    }

    ~DBManager() {
        PQfinish(conn);
    }

    // Получить все тайлы определенного снимка по полю frequency
    std::vector<std::string> getTilesByFrequency(int image_id, int frequency) {
        std::string query = "SELECT tile_url FROM Tiles WHERE image_id = " + 
                           std::to_string(image_id) + 
                           " AND frequency = " + 
                           std::to_string(frequency);
        
        PGresult* res = PQexec(conn, query.c_str());
        if (PQresultStatus(res) != PGRES_TUPLES_OK) {
            std::string error = PQerrorMessage(conn);
            PQclear(res);
            throw std::runtime_error("Ошибка выполнения запроса: " + error);
        }

        std::vector<std::string> tiles;
        int rows = PQntuples(res);
        for (int i = 0; i < rows; i++) {
            tiles.push_back(PQgetvalue(res, i, 0));
        }

        PQclear(res);
        return tiles;
    }

    // Добавить тайлы в базу данных
    void insertTiles(const std::vector<std::tuple<int, int, std::string, int, std::string>>& tiles) {
        std::string query = "INSERT INTO Tiles (tile_row, tile_column, spectrum, image_id, tile_url) VALUES ";
        
        for (size_t i = 0; i < tiles.size(); i++) {
            const auto& [row, col, spectrum, image_id, url] = tiles[i];
            query += "(" + std::to_string(row) + ", " + 
                    std::to_string(col) + ", '" + 
                    spectrum + "', " + 
                    std::to_string(image_id) + ", '" + 
                    url + "')";
            
            if (i < tiles.size() - 1) {
                query += ", ";
            }
        }

        executeQuery(query);
    }

    // Обновить частотность тайла
    void updateTileFrequency(int tile_id, int new_frequency) {
        std::string query = "UPDATE Tiles SET frequency = " + 
                           std::to_string(new_frequency) + 
                           " WHERE tile_id = " + 
                           std::to_string(tile_id);
        
        executeQuery(query);
    }

    // Получить все данные из таблицы Tiles
    std::vector<std::tuple<int, int, int, std::string, int, std::string, int>> getAllTiles() {
        std::string query = "SELECT * FROM Tiles";
        
        PGresult* res = PQexec(conn, query.c_str());
        if (PQresultStatus(res) != PGRES_TUPLES_OK) {
            std::string error = PQerrorMessage(conn);
            PQclear(res);
            throw std::runtime_error("Ошибка выполнения запроса: " + error);
        }

        std::vector<std::tuple<int, int, int, std::string, int, std::string, int>> tiles;
        int rows = PQntuples(res);
        for (int i = 0; i < rows; i++) {
            tiles.emplace_back(
                std::stoi(PQgetvalue(res, i, 0)), // tile_id
                std::stoi(PQgetvalue(res, i, 1)), // tile_row
                std::stoi(PQgetvalue(res, i, 2)), // tile_column
                PQgetvalue(res, i, 3),            // spectrum
                std::stoi(PQgetvalue(res, i, 4)), // image_id
                PQgetvalue(res, i, 5),            // tile_url
                std::stoi(PQgetvalue(res, i, 6))  // frequency
            );
        }

        PQclear(res);
        return tiles;
    }
}; 