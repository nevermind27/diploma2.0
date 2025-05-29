
#include <random>  


struct RouterInfo {
    std::string ip;
    int port;
    uint64_t hash_start;
    uint64_t hash_end;
    std::vector<std::string> neighbors;  // ip:port
};

void gossip_broadcast(const std::string& method, const std::string& path, const std::string& body = "");

// Вспомогательная функция для получения случайных соседей
std::vector<RoutingServerInfo> select_random_neighbors(DBManager& db_manager, int count = 2) {
    std::vector<RoutingServerInfo> all = db_manager.get_all_routing_servers();
    std::shuffle(all.begin(), all.end(), std::mt19937(std::random_device()()));
    return std::vector<RoutingServerInfo>(all.begin(), std::min(all.begin() + count, all.end()));
}

// Функция отправки запроса другим маршрутизаторам
void gossip_broadcast(const std::string& method, const std::string& path, const std::string& body) {
    DBManager db_manager;
    auto neighbors = select_random_neighbors(db_manager);

    for (const auto& neighbor : neighbors) {
        std::string full_path = path;
        std::string host = neighbor.adress;
        int port = 8080;

        std::string request = method + " " + full_path + " HTTP/1.1\r\n";
        request += "Host: " + host + "\r\n";
        request += "Content-Type: application/json\r\n";
        request += "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;

        // отправка запроса как TCP-соединение
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) continue;

        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

        if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == 0) {
            send(sock, request.c_str(), request.size(), 0);
        }
        close(sock);
    }
}


std::vector<RouterInfo> dht_table;  // локальный фрагмент DHT (обновляется через gossip)

// Hash-функция (например, по имени снимка)
uint64_t hash_key(const std::string& key) {
    std::hash<std::string> hasher;
    return hasher(key);
}

// Поиск маршрутизатора, ответственного за ключ
RouterInfo find_responsible_router(const std::string& key) {
    uint64_t h = hash_key(key);
    for (const auto& r : dht_table) {  // нет, здесь нужно по соседям искать, мы не знаем всю топологию
        if (r.hash_start <= h && h <= r.hash_end)
            return r;
    }
    // fallback: вернуть соседа с ближайшим hash_start
    return *std::min_element(dht_table.begin(), dht_table.end(), [h](auto& a, auto& b) {
        return std::abs((int64_t)(a.hash_start - h)) < std::abs((int64_t)(b.hash_start - h));
    });
}

// Gossip-обновление от соседа
void receive_gossip_update(const std::vector<RouterInfo>& updated) {
    for (const auto& r : updated) {
        auto it = std::find_if(dht_table.begin(), dht_table.end(), [&r](const RouterInfo& x) {
            return x.ip == r.ip && x.port == r.port;
        });
        if (it == dht_table.end()) {
            dht_table.push_back(r);  // добавляем нового соседа
        } else {
            *it = r;  // обновляем информацию
        }
    }
}


// кольцо DHT с перерасчетом границ хешей при добавлении/удалении маршрутизаторов
using namespace std;

using Hash = size_t;

// Узел в кольце DHT
struct Node {
    string address;          // IP:PORT
    Hash id;                 // Хэш от address
    Node* successor = nullptr;
    Node* predecessor = nullptr;
    map<Hash, string> metadata; // Хэш снимка -> имя файла

    explicit Node(string addr) : address(std::move(addr)) {
        id = hash<string>{}(address);
    }

    // Вставка снимка в DHT
    void store(Hash key, const string& filename) {
        metadata[key] = filename;
    }

    // Поиск владельца ключа в кольце
    Node* find_successor(Hash key) {
        if (in_range(key, id, successor->id)) {
            return successor;
        } else {
            return successor->find_successor(key); // Простой hop-by-hop
        }
    }

    static bool in_range(Hash key, Hash start, Hash end) {
        if (start < end) return key > start && key <= end;
        else return key > start || key <= end; // Обход через 0
    }
};

// Симуляция кольца
struct DHTRing {
    vector<Node*> nodes;

    void add_node(Node* new_node) {
        nodes.push_back(new_node);
        sort(nodes.begin(), nodes.end(), [](Node* a, Node* b) { return a->id < b->id; });
        update_neighbors();
    }

    void update_neighbors() {
        int n = nodes.size();
        for (int i = 0; i < n; ++i) {
            nodes[i]->successor = nodes[(i + 1) % n];
            nodes[i]->predecessor = nodes[(i - 1 + n) % n];
        }
    }

    void print_ring() {
        cout << "=== DHT Ring ===\n";
        for (auto node : nodes) {
            cout << "Node: " << node->address << " ID: " << node->id
                 << " Succ: " << node->successor->address << endl;
        }
    }
};
