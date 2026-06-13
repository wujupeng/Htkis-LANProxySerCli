#pragma once
#include <string>
#include <set>
#include <mutex>
#include <functional>
#include <crow.h>

class ws_handler {
public:
    static ws_handler& instance();

    void on_open(crow::websocket::connection& conn);
    void on_close(crow::websocket::connection& conn, const std::string& reason);
    void on_message(crow::websocket::connection& conn, const std::string& msg, bool is_binary);

    void broadcast(const std::string& message);

private:
    ws_handler() = default;

    std::set<crow::websocket::connection*> m_connections;
    std::mutex m_mutex;
};
