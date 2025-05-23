#ifndef STORAGE_SERVER_H
#define STORAGE_SERVER_H

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include "db_manager.h"

struct storage_server_options {
    uint32_t server_ip;
    uint16_t server_port;
    int workers_count;
    std::string storage_path;  // Путь для хранения файлов
};

// Флаг для остановки сервера
extern volatile bool g_storage_server_stop;

// Основная функция запуска сервера
int storage_server_run(const storage_server_options &opts);

// Функция обработки сокета
int handle_socket(int sock_fd, const std::string &storage_path);

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
std::string process_http_request(const HttpRequest& req, DBManager& db_manager, const std::string& storage_path);

#endif // STORAGE_SERVER_H 