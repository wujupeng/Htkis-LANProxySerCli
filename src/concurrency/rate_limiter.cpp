#include "concurrency/rate_limiter.h"
#include <mutex>

rate_limiter& rate_limiter::instance() {
    static rate_limiter inst;
    return inst;
}

void rate_limiter::init(int max_conn_per_ip_per_sec, int max_auth_fails_per_min, int ban_duration_sec) {
    m_max_conn_per_ip_per_sec = max_conn_per_ip_per_sec;
    m_max_auth_fails_per_min = max_auth_fails_per_min;
    m_ban_duration_sec = ban_duration_sec;
}

bool rate_limiter::check_connection_rate(const std::string& client_ip) {
    std::unique_lock lock(m_mutex);
    auto it = m_rates.find(client_ip);
    if (it == m_rates.end()) {
        auto info = std::make_unique<ip_rate_info>();
        info->conn_count = 1;
        info->conn_window_start = std::chrono::steady_clock::now();
        m_rates[client_ip] = std::move(info);
        return true;
    }

    auto& info = it->second;
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - info->conn_window_start).count();

    if (elapsed >= 1) {
        info->conn_count = 1;
        info->conn_window_start = now;
        return true;
    }

    info->conn_count++;
    return info->conn_count.load() <= m_max_conn_per_ip_per_sec;
}

bool rate_limiter::check_auth_rate(const std::string& client_ip) {
    std::shared_lock lock(m_mutex);
    auto it = m_rates.find(client_ip);
    if (it == m_rates.end()) return true;

    auto& info = it->second;
    auto now = std::chrono::steady_clock::now();

    if (now < info->banned_until) return false;

    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - info->auth_window_start).count();
    if (elapsed >= 60) return true;

    return info->auth_fail_count.load() < m_max_auth_fails_per_min;
}

void rate_limiter::record_auth_failure(const std::string& client_ip) {
    std::unique_lock lock(m_mutex);
    auto it = m_rates.find(client_ip);
    if (it == m_rates.end()) {
        auto info = std::make_unique<ip_rate_info>();
        info->auth_fail_count = 1;
        info->auth_window_start = std::chrono::steady_clock::now();
        m_rates[client_ip] = std::move(info);
        return;
    }

    auto& info = it->second;
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - info->auth_window_start).count();

    if (elapsed >= 60) {
        info->auth_fail_count = 1;
        info->auth_window_start = now;
    } else {
        info->auth_fail_count++;
    }

    if (info->auth_fail_count.load() >= m_max_auth_fails_per_min) {
        info->banned_until = now + std::chrono::seconds(m_ban_duration_sec);
    }
}
