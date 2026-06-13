#include "web_server/ws_handler.h"

ws_handler& ws_handler::instance() {
    static ws_handler inst;
    return inst;
}

void ws_handler::on_open(crow::websocket::connection& conn) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_connections.insert(&conn);
}

void ws_handler::on_close(crow::websocket::connection& conn, const std::string&) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_connections.erase(&conn);
}

void ws_handler::on_message(crow::websocket::connection&, const std::string&, bool) {
}

void ws_handler::broadcast(const std::string& message) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto* conn : m_connections) {
        conn->send_text(message);
    }
}
