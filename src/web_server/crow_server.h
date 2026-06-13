#pragma once
#include <string>
#include <crow.h>

class crow_server {
public:
    static crow_server& instance();

    void init(const std::string& bind_addr, int port, bool https_enabled,
              const std::string& cert_path, const std::string& key_path);
    void run();
    void stop();

    crow::SimpleApp& app() { return m_app; }

private:
    crow_server() = default;
    crow::SimpleApp m_app;
    std::string m_bind_addr{"0.0.0.0"};
    int m_port{8080};
};
