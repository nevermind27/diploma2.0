#include "routing_server.h"
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
#include <nlohmann/json.hpp>

volatile bool g_routing_server_stop = false;
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
void *worker_main(void *) {
    int process_fd;
    pthread_mutex_lock(&g_condvar_mtx);
    while (!g_routing_server_stop) {
        pthread_cond_wait(&g_condvar, &g_condvar_mtx);

        if (g_routing_server_stop) {
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
            handle_socket(process_fd);
            pthread_mutex_lock(&g_condvar_mtx);
        }
    }
    pthread_mutex_unlock(&g_condvar_mtx);
    return nullptr;
}

// Основная функция запуска сервера
int routing_server_run(const routing_server_options &opts) {
    sockaddr_in master_addr;
    master_addr.sin_family = AF_INET;
    master_addr.sin_addr.s_addr = opts.server_ip;
    master_addr.sin_port = opts.server_port;

    int master_fd = create_master_socket(master_addr);
    if (master_fd < 0) {
        return -1;
    }

    // Отправляем информацию о создании сервера
    nlohmann::json server_info;
    server_info["adress"] = inet_ntoa(master_addr.sin_addr);
    server_info["priority"] = 1; // Приоритет по умолчанию
    gossip_broadcast("POST", "/router/add", server_info.dump());

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
        pthread_create(&workers[i], nullptr, worker_main, nullptr);
    }

    while (!g_routing_server_stop) {
        int evcnt = epoll_wait(epl, evnts, MAX_EVENTS, -1);
        for (int i = 0; (!g_routing_server_stop) && i < evcnt; ++i) {
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
    
    // Отправляем информацию об удалении сервера
    std::string server_address = inet_ntoa(master_addr.sin_addr);
    gossip_broadcast("DELETE", "/router/remove/" + server_address);

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

// Список спектров для холодного хранилища
const char* COLD_STORAGE_SPECTRUMS[] = {
    "B01",  // Coastal/Aerosol
    "B09",  // Water Vapor
    "B10",  // Cirrus
    "B12",  // SWIR-2
    "B05",  // Red-Edge 1
    "B06",  // Red-Edge 2
    "B07",  // Red-Edge 3
    "B8A"   // Red-Edge 4
};
const int COLD_STORAGE_SPECTRUMS_COUNT = sizeof(COLD_STORAGE_SPECTRUMS) / sizeof(COLD_STORAGE_SPECTRUMS[0]);

storage_type_t determine_storage_type(const char* spectrum) {
    // Проверяем, входит ли спектр в список для холодного хранилища
    for (int i = 0; i < COLD_STORAGE_SPECTRUMS_COUNT; i++) {
        if (strcmp(spectrum, COLD_STORAGE_SPECTRUMS[i]) == 0) {
            return COLD_STORAGE;
        }
    }
    return HOT_STORAGE;
}

std::vector<ServerInfo> get_servers_by_type(DBManager& db_manager, const std::string& storage_type) {
    // Получаем список серверов из базы данных
    return db_manager.get_servers_by_type(storage_type);
}

ServerInfo select_optimal_server(const std::vector<ServerInfo>& servers, size_t data_size) {
    ServerInfo best_server;
    double best_score = -1;
    
    for (const auto& server : servers) {
        // Вычисляем свободное место на SSD и HDD
        double free_ssd = server.ssd_volume * (100 - server.ssd_fullness) / 100.0;
        double free_hdd = server.hdd_volume * (100 - server.hdd_fullness) / 100.0;
        
        // Вычисляем общий показатель свободного места
        double score = free_ssd + free_hdd;
        
        if (score > best_score) {
            best_score = score;
            best_server = server;
        }
    }
    
    return best_server;
}

int send_data_to_server(const ServerInfo& server, const char* data, size_t data_size) {
    // Формируем URL сервера
    std::string server_url = "http://" + server.location + ":8080/upload";
    
    // Отправляем данные на сервер
    std::string response = send_request_to_storage("POST", "/upload", std::string(data, data_size));
    
    // Проверяем ответ
    if (response.find("200 OK") != std::string::npos) {
        return 0;
    }
    return -1;
}

int distribute_to_storage(storage_type_t storage_type, const char* data, size_t data_size) {
    // Определяем тип хранилища в строковом формате
    std::string storage_type_str = (storage_type == HOT_STORAGE) ? "hot" : "cold";
    
    // Создаем экземпляр DBManager
    DBManager db_manager;
    
    // Получаем список серверов нужного типа
    std::vector<ServerInfo> servers = get_servers_by_type(db_manager, storage_type_str);
    
    if (servers.empty()) {
        printf("Ошибка: не найдены серверы типа %s\n", storage_type_str.c_str());
        return -1;
    }
    
    // Выбираем оптимальный сервер
    ServerInfo selected_server = select_optimal_server(servers, data_size);
    
    printf("Выбран сервер %d (тип: %s, свободное место: SSD %.1f%%, HDD %.1f%%)\n",
           selected_server.server_id,
           selected_server.class_type.c_str(),
           100.0 - selected_server.ssd_fullness,
           100.0 - selected_server.hdd_fullness);
    
    // Отправляем данные на выбранный сервер
    return send_data_to_server(selected_server, data, data_size);
}

// Функция для отправки HTTP-запроса к storage_server
std::string send_request_to_storage(const std::string& method, const std::string& path, 
                                  const std::string& body = "", 
                                  const std::map<std::string, std::string>& query_params = {}) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return "";
    }

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080); // Порт storage_server
    addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // IP storage_server

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return "";
    }

    // Формируем URL с query параметрами
    std::string full_path = path;
    if (!query_params.empty()) {
        full_path += "?";
        for (const auto& param : query_params) {
            if (full_path.back() != '?') full_path += "&";
            full_path += param.first + "=" + param.second;
        }
    }

    // Формируем HTTP-запрос
    std::string request = method + " " + full_path + " HTTP/1.1\r\n";
    request += "Host: localhost:8080\r\n";
    request += "Content-Type: application/json\r\n";
    if (!body.empty()) {
        request += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    }
    request += "\r\n";
    if (!body.empty()) {
        request += body;
    }

    // Отправляем запрос
    send(sock, request.c_str(), request.size(), 0);

    // Читаем ответ
    char buffer[4096];
    std::string response;
    int bytes_read;
    while ((bytes_read = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
        response.append(buffer, bytes_read);
    }

    close(sock);
    return response;
}

std::string process_http_request(const HttpRequest& req, DBManager& db_manager) {
    if (req.method == "POST" && req.path == "/router/add") {
        nlohmann::json data = nlohmann::json::parse(req.body);
        RoutingServerInsert rs;
        rs.adress = data["adress"];
        rs.priority = data["priority"];
        db_manager.insert_routing_server(rs);
        gossip_broadcast("POST", "/router/add", req.body);
        return "HTTP/1.1 201 Created\r\nContent-Length: 0\r\n\r\n";
    }
    if (req.method == "DELETE" && req.path.find("/router/remove/") == 0) {
        int id = std::stoi(req.path.substr(strlen("/router/remove/")));
        db_manager.delete_routing_server(id);
        gossip_broadcast("DELETE", req.path);
        return "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
    }
    if (req.method == "POST" && req.path == "/server/add") {
        nlohmann::json data = nlohmann::json::parse(req.body);
        ServerInsert s;
        s.ssd_fullness = data["ssd_fullness"];
        s.ssd_volume = data["ssd_volume"];
        s.hdd_volume = data["hdd_volume"];
        s.hdd_fullness = data["hdd_fullness"];
        s.location = data["location"];
        s.class_type = data["class"];
        db_manager.insert_server(s);
        gossip_broadcast("POST", "/server/add", req.body);
        return "HTTP/1.1 201 Created\r\nContent-Length: 0\r\n\r\n";
    }
    if (req.method == "DELETE" && req.path.find("/server/remove/") == 0) {
        int id = std::stoi(req.path.substr(strlen("/server/remove/")));
        db_manager.delete_server(id);
        gossip_broadcast("DELETE", req.path);
        return "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
    }

    if (req.method == "POST" && req.path == "/upload") {
        // Получаем спектр из заголовков
        const char* spectrum = nullptr;
        for (const auto& header : req.headers) {
            if (header.first == "X-Spectrum") {
                spectrum = header.second.c_str();
                break;
            }
        }

        if (!spectrum) {
            return "HTTP/1.1 400 Bad Request\r\n"
                   "Content-Type: application/json\r\n"
                   "Content-Length: 45\r\n\r\n"
                   "{\"error\": \"Spectrum header is required\"}";
        }

        // Определяем тип хранилища
        storage_type_t storage_type = determine_storage_type(spectrum);

        // Распределяем данные
        if (distribute_to_storage(storage_type, req.body.c_str(), req.body.length()) != 0) {
            return "HTTP/1.1 500 Internal Server Error\r\n"
                   "Content-Type: application/json\r\n"
                   "Content-Length: 35\r\n\r\n"
                   "{\"error\": \"Storage distribution failed\"}";
        }

        // Возвращаем успешный ответ
        return "HTTP/1.1 200 OK\r\n"
               "Content-Type: application/json\r\n"
               "Content-Length: 35\r\n\r\n"
               "{\"message\": \"File uploaded successfully\"}";
    }

    std::string response;
    
    // Обработка запросов для работы с изображениями
    if (req.path == "/images") {
        if (req.method == "GET") {
            // Проверяем параметры поиска
            if (req.query_params.find("north") != req.query_params.end() &&
                req.query_params.find("south") != req.query_params.end() &&
                req.query_params.find("east") != req.query_params.end() &&
                req.query_params.find("west") != req.query_params.end()) {
                
                // Получаем координаты из параметров
                float north = std::stof(req.query_params["north"]);
                float south = std::stof(req.query_params["south"]);
                float east = std::stof(req.query_params["east"]);
                float west = std::stof(req.query_params["west"]);
                
                // Ищем изображения в БД
                std::vector<ImageInfo> images = db_manager.search_images(north, south, east, west);
                
                // Формируем JSON-ответ
                std::string json_response = "{\"images\":[";
                for (size_t i = 0; i < images.size(); ++i) {
                    if (i > 0) json_response += ",";
                    json_response += "{";
                    json_response += "\"filename\":\"" + images[i].filename + "\",";
                    json_response += "\"timestamp\":\"" + images[i].timestamp + "\",";
                    json_response += "\"source\":\"" + images[i].source + "\",";
                    json_response += "\"north_lat\":" + std::to_string(images[i].north_lat) + ",";
                    json_response += "\"south_lat\":" + std::to_string(images[i].south_lat) + ",";
                    json_response += "\"east_lon\":" + std::to_string(images[i].east_lon) + ",";
                    json_response += "\"west_lon\":" + std::to_string(images[i].west_lon);
                    json_response += "}";
                }
                json_response += "]}";
                
                response = "HTTP/1.1 200 OK\r\n";
                response += "Content-Type: application/json\r\n";
                response += "Content-Length: " + std::to_string(json_response.size()) + "\r\n\r\n";
                response += json_response;
            } else {
                response = "HTTP/1.1 400 Bad Request\r\n\r\n";
            }
        }
        else if (req.method == "POST") {
            try {
                // Парсим JSON из тела запроса
                nlohmann::json json_data = nlohmann::json::parse(req.body);
                
                // Создаем структуру для вставки
                ImageInsertData data;
                data.filename = json_data["filename"];
                data.source = json_data["source"];
                data.timestamp = json_data["timestamp"];
                data.north_lat = json_data["north_lat"];
                data.south_lat = json_data["south_lat"];
                data.east_lon = json_data["east_lon"];
                data.west_lon = json_data["west_lon"];
                
                // Вставляем изображение в БД
                int image_id = db_manager.insert_image(data);
                if (image_id > 0) {
                    // Формируем JSON-ответ
                    std::string json_response = "{\"image_id\":" + std::to_string(image_id) + "}";
                    
                    response = "HTTP/1.1 201 Created\r\n";
                    response += "Content-Type: application/json\r\n";
                    response += "Content-Length: " + std::to_string(json_response.size()) + "\r\n\r\n";
                    response += json_response;
                } else {
                    response = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
                }
            } catch (...) {
                response = "HTTP/1.1 400 Bad Request\r\n\r\n";
            }
        }
    }
    // Обработка запросов для работы с тайлами
    else if (req.path == "/tiles") {
        if (req.method == "GET") {
            if (req.query_params.find("image_id") != req.query_params.end()) {
                int image_id = std::stoi(req.query_params["image_id"]);
                
                // Формируем параметры для запроса к storage_server
                std::map<std::string, std::string> params;
                params["image_id"] = std::to_string(image_id);
                
                if (req.query_params.find("sort") != req.query_params.end() && 
                    req.query_params["sort"] == "frequency") {
                    params["sort"] = "frequency";
                }
                
                // Отправляем запрос к storage_server
                std::string storage_response = send_request_to_storage("GET", "/tiles", "", params);
                
                if (!storage_response.empty()) {
                    response = storage_response;
                } else {
                    response = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
                }
            } else {
                response = "HTTP/1.1 400 Bad Request\r\n\r\n";
            }
        }
        else if (req.method == "POST") {
            try {
                // Парсим JSON из тела запроса
                nlohmann::json json_data = nlohmann::json::parse(req.body);
                
                // Отправляем запрос к storage_server
                std::string storage_response = send_request_to_storage("POST", "/tiles", req.body);
                
                if (!storage_response.empty()) {
                    response = storage_response;
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
            // Отправляем запрос к storage_server
            std::string storage_response = send_request_to_storage("POST", req.path);
            
            if (!storage_response.empty()) {
                response = storage_response;
            } else {
                response = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
            }
        }
    }
    else {
        response = "HTTP/1.1 404 Not Found\r\n\r\n";
    }
    
    return response;
}

// Модифицируем функцию handle_socket
int handle_socket(int sock_fd) {
    char buf[4096];
    ssize_t nbytes = recv(sock_fd, &buf, 4096, 0);
    if ((nbytes <= 0) && (errno != EAGAIN)) {
        shutdown(sock_fd, SHUT_RDWR);
        close(sock_fd);
        return -1;
    } else if (nbytes > 0) {
        HttpRequest req = parse_http_request(buf, nbytes);
        DBManager db_manager;
        std::string response = process_http_request(req, db_manager);
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
}