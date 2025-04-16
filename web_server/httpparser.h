

#ifndef SIMPLE_WEB_SERVER_HTTPPARSER_H
#define SIMPLE_WEB_SERVER_HTTPPARSER_H

#include <string>
#include <vector>

// Перечисление для HTTP-методов
enum class http_method {
    GET,
    PUT,
    UNKNOWN
};

struct http_response_msg {
    std::string status_line;
    std::vector<std::string> headers;
    FILE *req_file;
    http_method method;  // Метод запроса
    std::string request_body;  // Тело запроса для PUT
    std::string request_path;  // Путь запроса
};

int parse_request(char *msg, http_response_msg &response, const char *webserv_root_dir);
int handle_request(http_response_msg &response, const char *webserv_root_dir);


#endif //SIMPLE_WEB_SERVER_HTTPPARSER_H
