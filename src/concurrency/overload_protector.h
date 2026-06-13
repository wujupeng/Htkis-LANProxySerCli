#pragma once
#include <string>
#include <atomic>
#include <chrono>
#include <shared_mutex>
#include <unordered_map>
#include <memory>

struct overload_status {
    bool is_overloaded{false};
    double fd_usage_pct{0.0};
    double mem_usage_pct{0.0};
    int total_connections{0};
    std::string reason;
};

class overload_protector {
public:
    static overload_protector& instance();

    void init(double fd_threshold_pct = 0.9, double mem_threshold_pct = 0.85,
              int max_total_connections = 4000, int max_per_user = 50);

    overload_status check() const;
    bool is_overloaded() const;
    bool check_user_limit(const std::string& username) const;
    void inc_user_connections(const std::string& username);
    void dec_user_connections(const std::string& username);
    int get_user_connections(const std::string& username) const;
    int get_total_connections() const;
    void inc_total_connections();
    void dec_total_connections();

    overload_status get_status() const;

private:
    overload_protector() = default;

    double m_fd_threshold_pct{0.9};
    double m_mem_threshold_pct{0.85};
    int m_max_total_connections{4000};
    int m_max_per_user{50};

    std::atomic<int> m_total_connections{0};
    std::unordered_map<std::string, std::unique_ptr<std::atomic<int>>> m_user_connections;
    mutable std::shared_mutex m_user_mutex;
};
