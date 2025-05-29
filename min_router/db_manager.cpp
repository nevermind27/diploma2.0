#include "db_manager.h"
#include <iostream>
#include <sstream>

DBManager::DBManager() {
    connect();
}

DBManager::~DBManager() {
    if (conn) {
        PQfinish(conn);
    }
}

void DBManager::connect() {
    conn = PQconnectdb("dbname=router_db user=postgres password=postgres host=localhost port=5432");
    if (PQstatus(conn) != CONNECTION_OK) {
        std::cerr << "Connection to database failed: " << PQerrorMessage(conn) << std::endl;
        PQfinish(conn);
        conn = nullptr;
    }
}

std::string DBManager::get_image(const std::string& image_id) {
    if (!conn) {
        std::cerr << "No database connection" << std::endl;
        return "";
    }

    std::stringstream query;
    query << "SELECT image_data FROM Images WHERE image_id = '" << image_id << "';";
    
    PGresult* res = PQexec(conn, query.str().c_str());
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        std::cerr << "Query failed: " << PQerrorMessage(conn) << std::endl;
        PQclear(res);
        return "";
    }

    std::string result;
    if (PQntuples(res) > 0) {
        result = PQgetvalue(res, 0, 0);
    }

    PQclear(res);
    return result;
}

bool DBManager::put_image(const std::string& image_id, const std::string& image_data) {
    if (!conn) {
        std::cerr << "No database connection" << std::endl;
        return false;
    }

    std::stringstream query;
    query << "INSERT INTO Images (image_id, image_data) VALUES ('" 
          << image_id << "', '" << image_data << "') "
          << "ON CONFLICT (image_id) DO UPDATE SET image_data = EXCLUDED.image_data;";
    
    PGresult* res = PQexec(conn, query.str().c_str());
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        std::cerr << "Query failed: " << PQerrorMessage(conn) << std::endl;
        PQclear(res);
        return false;
    }

    PQclear(res);
    return true;
}

std::vector<std::string> DBManager::get_spectrums(const std::string& image_id) {
    std::vector<std::string> spectrums;
    if (!conn) {
        std::cerr << "No database connection" << std::endl;
        return spectrums;
    }

    std::stringstream query;
    query << "SELECT spectrum_name FROM Spectrums WHERE image_id = '" << image_id << "';";
    
    PGresult* res = PQexec(conn, query.str().c_str());
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        std::cerr << "Query failed: " << PQerrorMessage(conn) << std::endl;
        PQclear(res);
        return spectrums;
    }

    int rows = PQntuples(res);
    for (int i = 0; i < rows; i++) {
        spectrums.push_back(PQgetvalue(res, i, 0));
    }

    PQclear(res);
    return spectrums;
}

bool DBManager::add_neighbor(const NeighborInfo& neighbor) {
    if (!conn) {
        std::cerr << "No database connection" << std::endl;
        return false;
    }

    std::stringstream query;
    query << "INSERT INTO Neighbors (neighbor_id, address, last_seen, is_active, priority) "
          << "VALUES ('" << neighbor.neighbor_id << "', '" << neighbor.address << "', "
          << "CURRENT_TIMESTAMP, " << (neighbor.is_active ? "true" : "false") << ", "
          << neighbor.priority << ") "
          << "ON CONFLICT (neighbor_id) DO UPDATE SET "
          << "address = EXCLUDED.address, "
          << "last_seen = CURRENT_TIMESTAMP, "
          << "is_active = EXCLUDED.is_active, "
          << "priority = EXCLUDED.priority;";

    PGresult* res = PQexec(conn, query.str().c_str());
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        std::cerr << "Query failed: " << PQerrorMessage(conn) << std::endl;
        PQclear(res);
        return false;
    }

    PQclear(res);
    return true;
}

bool DBManager::update_neighbor(const NeighborInfo& neighbor) {
    if (!conn) {
        std::cerr << "No database connection" << std::endl;
        return false;
    }

    std::stringstream query;
    query << "UPDATE Neighbors SET "
          << "address = '" << neighbor.address << "', "
          << "last_seen = CURRENT_TIMESTAMP, "
          << "is_active = " << (neighbor.is_active ? "true" : "false") << ", "
          << "priority = " << neighbor.priority << " "
          << "WHERE neighbor_id = '" << neighbor.neighbor_id << "';";

    PGresult* res = PQexec(conn, query.str().c_str());
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        std::cerr << "Query failed: " << PQerrorMessage(conn) << std::endl;
        PQclear(res);
        return false;
    }

    PQclear(res);
    return true;
}

bool DBManager::remove_neighbor(const std::string& neighbor_id) {
    if (!conn) {
        std::cerr << "No database connection" << std::endl;
        return false;
    }

    std::stringstream query;
    query << "DELETE FROM Neighbors WHERE neighbor_id = '" << neighbor_id << "';";

    PGresult* res = PQexec(conn, query.str().c_str());
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        std::cerr << "Query failed: " << PQerrorMessage(conn) << std::endl;
        PQclear(res);
        return false;
    }

    PQclear(res);
    return true;
}

std::vector<NeighborInfo> DBManager::get_active_neighbors() {
    std::vector<NeighborInfo> neighbors;
    if (!conn) {
        std::cerr << "No database connection" << std::endl;
        return neighbors;
    }

    std::string query = "SELECT neighbor_id, address, last_seen, is_active, priority "
                       "FROM Neighbors WHERE is_active = true "
                       "ORDER BY priority DESC, last_seen DESC;";

    PGresult* res = PQexec(conn, query.c_str());
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        std::cerr << "Query failed: " << PQerrorMessage(conn) << std::endl;
        PQclear(res);
        return neighbors;
    }

    int rows = PQntuples(res);
    for (int i = 0; i < rows; i++) {
        NeighborInfo neighbor;
        neighbor.neighbor_id = PQgetvalue(res, i, 0);
        neighbor.address = PQgetvalue(res, i, 1);
        neighbor.last_seen = PQgetvalue(res, i, 2);
        neighbor.is_active = (PQgetvalue(res, i, 3)[0] == 't');
        neighbor.priority = std::stoi(PQgetvalue(res, i, 4));
        neighbors.push_back(neighbor);
    }

    PQclear(res);
    return neighbors;
}

std::vector<NeighborInfo> DBManager::get_neighbors_by_priority(int min_priority) {
    std::vector<NeighborInfo> neighbors;
    if (!conn) {
        std::cerr << "No database connection" << std::endl;
        return neighbors;
    }

    std::stringstream query;
    query << "SELECT neighbor_id, address, last_seen, is_active, priority "
          << "FROM Neighbors WHERE priority >= " << min_priority << " "
          << "ORDER BY priority DESC, last_seen DESC;";

    PGresult* res = PQexec(conn, query.str().c_str());
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        std::cerr << "Query failed: " << PQerrorMessage(conn) << std::endl;
        PQclear(res);
        return neighbors;
    }

    int rows = PQntuples(res);
    for (int i = 0; i < rows; i++) {
        NeighborInfo neighbor;
        neighbor.neighbor_id = PQgetvalue(res, i, 0);
        neighbor.address = PQgetvalue(res, i, 1);
        neighbor.last_seen = PQgetvalue(res, i, 2);
        neighbor.is_active = (PQgetvalue(res, i, 3)[0] == 't');
        neighbor.priority = std::stoi(PQgetvalue(res, i, 4));
        neighbors.push_back(neighbor);
    }

    PQclear(res);
    return neighbors;
} 