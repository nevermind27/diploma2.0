#include "httpparser.h"

#include <cstring>
#include <algorithm>

int parse_request(char *msg, http_response_msg &response, const char *webserv_root_dir) {
    response.req_file = nullptr;
    response.headers.clear();
    response.request_body.clear();

    // Извлечение метода запроса
    char *pos = strtok(msg, " ");
    std::string method_str = pos;
    
    // Определение метода
    if (method_str == "GET") {
        response.method = http_method::GET;
    } else if (method_str == "PUT") {
        response.method = http_method::PUT;
    } else {
        response.method = http_method::UNKNOWN;
    }

    // Извлечение пути
    std::string filepath = strtok(nullptr, " ");
    response.request_path = filepath;
    
    // Обработка GET-параметров
    unsigned long idx = filepath.find('?');
    if (idx != std::string::npos) {
        filepath = filepath.substr(0, idx);
    }

    // Полный путь к файлу
    std::string full_path = webserv_root_dir;
    full_path += filepath;

    // Извлечение версии HTTP
    std::string http_ver = strtok(nullptr, "\n");
    http_ver.pop_back();
    if (http_ver.back() == '\r') {
        http_ver.pop_back();
    }

    // Обработка заголовков и тела запроса
    char *line = strtok(nullptr, "\n");
    bool headers_end = false;
    int content_length = 0;
    
    while (line != nullptr) {
        std::string header_line(line);
        
        // Удаление символов возврата каретки
        if (!header_line.empty() && header_line.back() == '\r') {
            header_line.pop_back();
        }
        
        // Проверка на конец заголовков
        if (header_line.empty()) {
            headers_end = true;
        } else if (!headers_end) {
            // Обработка заголовка Content-Length для PUT
            if (header_line.substr(0, 16) == "Content-Length: ") {
                content_length = std::stoi(header_line.substr(16));
            }
        } else if (headers_end && response.method == http_method::PUT) {
            // Собираем тело запроса для PUT
            response.request_body += header_line;
        }
        
        line = strtok(nullptr, "\n");
    }

    // Сохраняем версию HTTP для ответа
    response.status_line = http_ver;
    response.status_line += " ";

    // Для GET-запросов проверяем существование файла
    if (response.method == http_method::GET) {
        FILE *file = fopen(full_path.c_str(), "r");
        response.req_file = file;

        if (file) {
            response.status_line += "200 Ok";
            fseek(file, 0, SEEK_END);
            long byte_size = ftell(file);
            response.headers.emplace_back("Content-Length: " + std::to_string(byte_size));
            rewind(file);
        } else {
            response.status_line += "404 Not Found";
            response.headers.emplace_back("Content-Length: 0");
        }
    }
    
    return 0;
}

int handle_request(http_response_msg &response, const char *webserv_root_dir) {
    std::string full_path = webserv_root_dir;
    full_path += response.request_path;
    
    // Обработка GET-запросов
    if (response.method == http_method::GET) {
        // GET-запросы уже обработаны в parse_request
        return 0;
    }
    
    // Обработка PUT-запросов
    if (response.method == http_method::PUT) {
        // Открываем файл для записи
        FILE *file = fopen(full_path.c_str(), "w");
        
        if (file) {
            // Записываем тело запроса в файл
            fwrite(response.request_body.c_str(), 1, response.request_body.size(), file);
            fclose(file);
            
            // Формируем ответ
            response.status_line += "200 OK";
            response.headers.emplace_back("Content-Length: 0");
        } else {
            // Ошибка при создании файла
            response.status_line += "500 Internal Server Error";
            response.headers.emplace_back("Content-Length: 0");
        }
        
        return 0;
    }
    
    // Обработка неизвестных методов
    response.status_line += "405 Method Not Allowed";
    response.headers.emplace_back("Content-Length: 0");
    
    return 0;
}
