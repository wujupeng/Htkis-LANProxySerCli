#pragma once
#include <string>
#include <unordered_map>
#include <atomic>
#include <shared_mutex>
#include <chrono>
#include <memory>

class rate_limiter {
public:
    static rate_limiter& instance();

    void init(int max_conn_per_ip_per_sec = 10, int max_auth_fails_per_min = 5, int ban_duration_sec = 60);

    bool check_connection_rate(const std::string& client_ip);
    bool check_auth_rate(const std::string& client_ip);
    void record_auth_failure(const std::string& client_ip);

private:
    rate_limiter() = default;

    struct ip_rate_info {
        std::atomic<int> conn_count{0};
        std::chrono::steady_clock::time_point conn_window_start;
        std::atomic<int> auth_fail_count{0};
        std::chrono::steady_clock::time_point auth_window_start;
        std::chrono::steady_clock::time_point banned_until;
    };

    int m_max_conn_per_ip_per_sec{10};
    int m_max_auth_fails_per_min{5};
    int m_ban_duration_sec{60};

    std::unordered_map<std::string, std::unique_ptr<ip_rate_info>> m_rates;
    mutable std::shared_mutex m_mutex;
};
