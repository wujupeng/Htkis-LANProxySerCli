#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <shared_mutex>
#include <atomic>

enum class node_health_status { available, unavailable, unstable };

struct node_health_info {
    std::string tag;
    node_health_status status{node_health_status::available};
    int latency_ms{0};
    int consecutive_failures{0};
    std::chrono::steady_clock::time_point last_check;
    int total_checks{0};
    int failed_checks{0};
};

class node_health_checker {
public:
    static node_health_checker& instance();

    void init(int check_interval_sec = 10, int probe_timeout_sec = 5, int failure_threshold = 3);

    void start();
    void stop();
    void check_now(const std::string& tag);

    node_health_status get_status(const std::string& tag) const;
    int get_latency_ms(const std::string& tag) const;
    node_health_info get_info(const std::string& tag) const;
    std::vector<node_health_info> get_all_info() const;

    void mark_unavailable(const std::string& tag);
    void mark_available(const std::string& tag);

private:
    node_health_checker() = default;
    void do_probe(const std::string& tag);
    void periodic_check();

    int m_check_interval_sec{10};
    int m_probe_timeout_sec{5};
    int m_failure_threshold{3};

    std::unordered_map<std::string, node_health_info> m_health;
    mutable std::shared_mutex m_mutex;
    std::atomic<bool> m_running{false};
};
