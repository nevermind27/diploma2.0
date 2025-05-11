#ifndef ROUTING_SERVER_H
#define ROUTING_SERVER_H

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include "db_manager.h"

struct routing_server_options {
    uint32_t server_ip;
    uint16_t server_port;
    int workers_count;
};

// Флаг для остановки сервера
extern volatile bool g_routing_server_stop;

// Основная функция запуска сервера
int routing_server_run(const routing_server_options &opts);

// Функция обработки сокета
int handle_socket(int sock_fd);

// Функция отправки ответа
int send_response(int socket_fd, const std::string &response);

// Структура для хранения HTTP-запроса
struct HttpRequest {
    std::string method;
    std::string path;
    std::map<std::string, std::string> query_params;
    std::map<std::string, std::string> headers;
    std::string body;
};

// Функция парсинга HTTP-запроса
HttpRequest parse_http_request(const char* buffer, size_t length);

// Функция обработки HTTP-запроса
std::string process_http_request(const HttpRequest& req, DBManager& db_manager);

// Определение типов хранилищ
typedef enum {
    HOT_STORAGE,
    COLD_STORAGE
} storage_type_t;

// Функция для определения типа хранилища на основе спектра
storage_type_t determine_storage_type(const char* spectrum);

// Функция для распределения данных в соответствующее хранилище
int distribute_to_storage(storage_type_t storage_type, const char* data, size_t data_size);

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

// Функция для получения списка серверов определенного типа
std::vector<ServerInfo> get_servers_by_type(DBManager& db_manager, const std::string& storage_type);

// Функция для выбора оптимального сервера
ServerInfo select_optimal_server(const std::vector<ServerInfo>& servers, size_t data_size);

// Функция для отправки данных на выбранный сервер
int send_data_to_server(const ServerInfo& server, const char* data, size_t data_size);

#endif // ROUTING_SERVER_H 