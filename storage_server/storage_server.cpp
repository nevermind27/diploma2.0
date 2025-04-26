#include "storage_server.h"
#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <queue>
#include <mutex>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

volatile bool g_storage_server_stop = false;
const int MAX_EVENTS = 32;

// Структура для очереди сокетов
struct {
    std::queue<int> que;
    pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
} g_handle_socks;

pthread_cond_t g_condvar;
pthread_mutex_t g_condvar_mtx = PTHREAD_MUTEX_INITIALIZER;

// Установка неблокирующего режима для сокета
int set_nonblock(int fd) {
    int flags;
    if (-1 == (flags = fcntl(fd, F_GETFL, 0))) {
        flags = 0;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// Создание основного сокета
int create_master_socket(const sockaddr_in &master_addr) {
    int master_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (master_fd < 0) {
        perror("Error creating socket: ");
        return -1;
    }

    if (bind(master_fd, reinterpret_cast<const sockaddr *>(&master_addr), sizeof(master_addr)) < 0) {
        perror("Error binding socket: ");
        return -2;
    }

    set_nonblock(master_fd);
    listen(master_fd, SOMAXCONN);
    return master_fd;
}

// Функция рабочего потока
void *worker_main(void *arg) {
    std::string storage_path = *static_cast<std::string*>(arg);
    int process_fd;
    pthread_mutex_lock(&g_condvar_mtx);
    while (!g_storage_server_stop) {
        pthread_cond_wait(&g_condvar, &g_condvar_mtx);

        if (g_storage_server_stop) {
            break;
        }

        pthread_mutex_lock(&g_handle_socks.mtx);
        if (!g_handle_socks.que.empty()) {
            process_fd = g_handle_socks.que.front();
            g_handle_socks.que.pop();
        } else {
            process_fd = 0;
        }
        pthread_mutex_unlock(&g_handle_socks.mtx);

        if (process_fd != 0) {
            pthread_mutex_unlock(&g_condvar_mtx);
            handle_socket(process_fd, storage_path);
            pthread_mutex_lock(&g_condvar_mtx);
        }
    }
    pthread_mutex_unlock(&g_condvar_mtx);
    return nullptr;
}

// Основная функция запуска сервера
int storage_server_run(const storage_server_options &opts) {
    sockaddr_in master_addr;
    master_addr.sin_family = AF_INET;
    master_addr.sin_addr.s_addr = opts.server_ip;
    master_addr.sin_port = opts.server_port;

    int master_fd = create_master_socket(master_addr);
    if (master_fd < 0) {
        return -1;
    }

    int epl = epoll_create1(0);
    epoll_event ev;
    ev.data.fd = master_fd;
    ev.events = EPOLLIN;
    epoll_ctl(epl, EPOLL_CTL_ADD, master_fd, &ev);

    epoll_event evnts[MAX_EVENTS];

    if (pthread_cond_init(&g_condvar, nullptr) != 0) {
        perror("pthread_cond_init: ");
    }

    std::vector<pthread_t> workers;
    workers.resize(opts.workers_count);
    for (int i = 0; i < opts.workers_count; ++i) {
        pthread_create(&workers[i], nullptr, worker_main, const_cast<std::string*>(&opts.storage_path));
    }

    while (!g_storage_server_stop) {
        int evcnt = epoll_wait(epl, evnts, MAX_EVENTS, -1);
        for (int i = 0; (!g_storage_server_stop) && i < evcnt; ++i) {
            if (evnts[i].data.fd == master_fd) {
                int sock = accept(master_fd, nullptr, nullptr);
                set_nonblock(sock);
                ev.data.fd = sock;
                ev.events = EPOLLIN;
                epoll_ctl(epl, EPOLL_CTL_ADD, sock, &ev);
                continue;
            }

            if ((evnts[i].events & EPOLLERR) || (evnts[i].events & EPOLLHUP)) {
                epoll_ctl(epl, EPOLL_CTL_DEL, evnts[i].data.fd, nullptr);
                shutdown(evnts[i].data.fd, SHUT_RDWR);
                close(evnts[i].data.fd);
            } else if (evnts[i].events & EPOLLIN) {
                pthread_mutex_lock(&g_condvar_mtx);
                pthread_mutex_lock(&g_handle_socks.mtx);
                g_handle_socks.que.push(evnts[i].data.fd);
                epoll_ctl(epl, EPOLL_CTL_DEL, evnts[i].data.fd, nullptr);
                pthread_mutex_unlock(&g_handle_socks.mtx);
                pthread_cond_signal(&g_condvar);
                pthread_mutex_unlock(&g_condvar_mtx);
            }
        }
    }

    printf("Info: Release resources\n");
    for (const auto &wrk : workers) {
        pthread_join(wrk, nullptr);
    }
    return 0;
}

// Парсинг HTTP-запроса
HttpRequest parse_http_request(const char* buffer, size_t length) {
    HttpRequest req;
    std::string request_str(buffer, length);
    std::istringstream iss(request_str);
    
    // Парсинг первой строки (метод и путь)
    std::string line;
    std::getline(iss, line);
    std::istringstream first_line(line);
    first_line >> req.method >> req.path;
    
    // Парсинг заголовков
    while (std::getline(iss, line) && line != "\r") {
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 2); // +2 чтобы пропустить ": "
            req.headers[key] = value;
        }
    }
    
    // Парсинг query параметров
    size_t query_pos = req.path.find('?');
    if (query_pos != std::string::npos) {
        std::string query_str = req.path.substr(query_pos + 1);
        req.path = req.path.substr(0, query_pos);
        
        std::istringstream query_iss(query_str);
        std::string param;
        while (std::getline(query_iss, param, '&')) {
            size_t equal_pos = param.find('=');
            if (equal_pos != std::string::npos) {
                std::string key = param.substr(0, equal_pos);
                std::string value = param.substr(equal_pos + 1);
                req.query_params[key] = value;
            }
        }
    }
    
    // Чтение тела запроса
    std::getline(iss, req.body);
    
    return req;
}

// Обработка HTTP-запроса
std::string process_http_request(const HttpRequest& req, DBManager& db_manager, const std::string& storage_path) {
    std::string response;
    
    // Обработка запросов для работы с тайлами
    if (req.path == "/tiles") {
        if (req.method == "GET") {
            // Получение тайлов снимка
            if (req.query_params.find("image_id") != req.query_params.end()) {
                int image_id = std::stoi(req.query_params["image_id"]);
                
                // Проверяем параметр сортировки
                if (req.query_params.find("sort") != req.query_params.end() && 
                    req.query_params["sort"] == "frequency") {
                    // Получение тайлов с сортировкой по частоте
                    std::vector<std::string> tile_urls = db_manager.get_tiles_by_frequency(image_id);
                    
                    // Формируем JSON-ответ
                    std::string json_response = "{\"tiles\":[";
                    for (size_t i = 0; i < tile_urls.size(); ++i) {
                        if (i > 0) json_response += ",";
                        json_response += "\"" + tile_urls[i] + "\"";
                    }
                    json_response += "]}";
                    
                    response = "HTTP/1.1 200 OK\r\n";
                    response += "Content-Type: application/json\r\n";
                    response += "Content-Length: " + std::to_string(json_response.size()) + "\r\n\r\n";
                    response += json_response;
                } else {
                    // Получение всех тайлов снимка
                    std::vector<std::string> tile_urls = db_manager.get_tiles_by_image(image_id);
                    
                    // Формируем JSON-ответ
                    std::string json_response = "{\"tiles\":[";
                    for (size_t i = 0; i < tile_urls.size(); ++i) {
                        if (i > 0) json_response += ",";
                        json_response += "\"" + tile_urls[i] + "\"";
                    }
                    json_response += "]}";
                    
                    response = "HTTP/1.1 200 OK\r\n";
                    response += "Content-Type: application/json\r\n";
                    response += "Content-Length: " + std::to_string(json_response.size()) + "\r\n\r\n";
                    response += json_response;
                }
            } else {
                response = "HTTP/1.1 400 Bad Request\r\n\r\n";
            }
        }
        else if (req.method == "POST") {
            // Добавление нового тайла
            try {
                // Парсим JSON из тела запроса
                nlohmann::json json_data = nlohmann::json::parse(req.body);
                
                int image_id = json_data["image_id"];
                int tile_row = json_data["tile_row"];
                int tile_column = json_data["tile_column"];
                
                // Формируем URL тайла
                std::string tile_url = "tile_" + std::to_string(image_id) + "_" + 
                                     std::to_string(tile_row) + "_" + 
                                     std::to_string(tile_column) + ".bin";
                
                // Добавляем тайл в БД
                if (db_manager.insert_tile(image_id, tile_row, tile_column, tile_url)) {
                    response = "HTTP/1.1 201 Created\r\n\r\n";
                } else {
                    response = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
                }
            } catch (...) {
                response = "HTTP/1.1 400 Bad Request\r\n\r\n";
            }
        }
    }
    // Обработка запроса на инкремент частоты обращения к тайлу
    else if (req.path.find("/tiles/") == 0 && req.path.find("/increment") != std::string::npos) {
        if (req.method == "POST") {
            // Извлекаем tile_row и tile_column из пути
            std::string path_part = req.path.substr(7); // Убираем "/tiles/"
            path_part = path_part.substr(0, path_part.find("/increment")); // Убираем "/increment"
            
            size_t slash_pos = path_part.find('/');
            if (slash_pos != std::string::npos) {
                int tile_row = std::stoi(path_part.substr(0, slash_pos));
                int tile_column = std::stoi(path_part.substr(slash_pos + 1));
                
                // Обновляем частоту обращения к тайлу
                if (db_manager.increment_tile_frequency(tile_row, tile_column)) {
                    response = "HTTP/1.1 200 OK\r\n\r\n";
                } else {
                    response = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
                }
            } else {
                response = "HTTP/1.1 400 Bad Request\r\n\r\n";
            }
        }
    }
    else {
        response = "HTTP/1.1 404 Not Found\r\n\r\n";
    }
    
    return response;
}

// Функция обработки сокета
int handle_socket(int sock_fd, const std::string &storage_path) {
    char buf[4096];
    ssize_t nbytes = recv(sock_fd, &buf, 4096, 0);
    if ((nbytes <= 0) && (errno != EAGAIN)) {
        shutdown(sock_fd, SHUT_RDWR);
        close(sock_fd);
        return -1;
    } else if (nbytes > 0) {
        // Парсим HTTP-запрос
        HttpRequest req = parse_http_request(buf, nbytes);
        
        // Создаем экземпляр DBManager
        DBManager db_manager;
        
        // Обрабатываем запрос
        std::string response = process_http_request(req, db_manager, storage_path);
        
        // Отправляем ответ
        send_response(sock_fd, response);
        shutdown(sock_fd, SHUT_RDWR);
        close(sock_fd);
    }
    return 0;
}

// Функция отправки ответа
int send_response(int socket_fd, const std::string &response) {
    if (send(socket_fd, response.c_str(), response.size(), 0) < 0) {
        perror("Cannot send to socket in send_response: ");
        return -1;
    }
    return 0; 