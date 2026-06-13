#pragma once
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <chrono>

class timeout_scanner {
public:
    static timeout_scanner& instance();

    void init(int auth_timeout_sec = 30, int handshake_timeout_sec = 15,
              int idle_timeout_sec = 300, int max_lifetime_sec = 28800,
              int scan_interval_sec = 5);

    void start();
    void stop();

    void set_timeout_callback(std::function<void(const std::string& session_id, const std::string& reason)> callback);

private:
    timeout_scanner() = default;
    void scan_loop();

    int m_auth_timeout_sec{30};
    int m_handshake_timeout_sec{15};
    int m_idle_timeout_sec{300};
    int m_max_lifetime_sec{28800};
    int m_scan_interval_sec{5};

    std::atomic<bool> m_running{false};
    std::function<void(const std::string&, const std::string&)> m_on_timeout;
};
