#include "web_server/crow_server.h"
#include "web_server/auth_middleware.h"
#include "log_monitor/structured_logger.h"

crow_server& crow_server::instance() {
    static crow_server inst;
    return inst;
}

void crow_server::init(const std::string& bind_addr, int port, bool /*https_enabled*/,
                         const std::string& /*cert_path*/, const std::string& /*key_path*/) {
    m_bind_addr = bind_addr;
    m_port = port;

    structured_logger::instance().info("crow_server",
        "Web server configured on " + bind_addr + ":" + std::to_string(port));
}

void crow_server::run() {
    structured_logger::instance().info("crow_server",
        "Web server starting on " + m_bind_addr + ":" + std::to_string(m_port));
    m_app.bindaddr(m_bind_addr).port(m_port).multithreaded().run();
}

void crow_server::stop() {
    m_app.stop();
    structured_logger::instance().info("crow_server", "Web server stopped");
}
