#pragma once

#include <string>
#include <vector>
#include <memory>
#include <libpq-fe.h>

struct NeighborInfo {
    std::string neighbor_id;
    std::string address;
    std::string last_seen;
    bool is_active;
    int priority;
};

class DBManager {
private:
    PGconn* conn;
    void connect();

public:
    DBManager();
    ~DBManager();

    // Получение изображения по ID
    std::string get_image(const std::string& image_id);
    
    // Сохранение изображения
    bool put_image(const std::string& image_id, const std::string& image_data);
    
    // Получение информации о спектрах
    std::vector<std::string> get_spectrums(const std::string& image_id);

    // Методы для работы с соседями
    bool add_neighbor(const NeighborInfo& neighbor);
    bool update_neighbor(const NeighborInfo& neighbor);
    bool remove_neighbor(const std::string& neighbor_id);
    std::vector<NeighborInfo> get_active_neighbors();
    std::vector<NeighborInfo> get_neighbors_by_priority(int min_priority);
}; 