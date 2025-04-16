#ifndef DB_MANAGER_H
#define DB_MANAGER_H

#include <string>
#include <vector>
#include <tuple>

class DBManager {
private:
    void* conn; // PGconn* скрыт для инкапсуляции
    std::string dbname;
    std::string user;
    std::string password;
    std::string host;
    std::string port;

    void executeQuery(const std::string& query);

public:
    DBManager(const std::string& dbname, 
              const std::string& user = "postgres",
              const std::string& password = "",
              const std::string& host = "localhost",
              const std::string& port = "5432");
    
    ~DBManager();

    // Получить все тайлы определенного снимка по полю frequency
    std::vector<std::string> getTilesByFrequency(int image_id, int frequency);

    // Добавить тайлы в базу данных
    void insertTiles(const std::vector<std::tuple<int, int, std::string, int, std::string>>& tiles);

    // Обновить частотность тайла
    void updateTileFrequency(int tile_id, int new_frequency);

    // Получить все данные из таблицы Tiles
    std::vector<std::tuple<int, int, int, std::string, int, std::string, int>> getAllTiles();
};

#endif // DB_MANAGER_H 