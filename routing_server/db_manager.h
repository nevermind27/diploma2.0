// Структура для хранения информации о сервере
struct ServerInfo {
    int server_id;
    int ssd_fullness;
    int ssd_volume;
    int hdd_volume;
    int hdd_fullness;
    std::string location;
    std::string class_type;
};

class DBManager {
public:
    // Получение списка серверов определенного типа
    std::vector<ServerInfo> get_servers_by_type(const std::string& storage_type);
}; 