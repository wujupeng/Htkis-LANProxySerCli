#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <shared_mutex>
#include <atomic>
#include <cstdint>

struct node_metrics {
    std::string tag;
    int active_connections{0};
    int total_connections{0};
    uint64_t bytes_up{0};
    uint64_t bytes_down{0};
    int latency_ms{0};
    std::string health;
};

struct user_metrics {
    std::string username;
    int active_connections{0};
    int total_connections{0};
    uint64_t bytes_up{0};
    uint64_t bytes_down{0};
};

struct history_snapshot {
    std::chrono::steady_clock::time_point timestamp;
    int active_connections{0};
    int queued_connections{0};
    std::unordered_map<std::string, int> node_connections;
};

struct global_stats {
    int active_connections{0};
    int total_connections{0};
    int queued_connections{0};
    int max_connections{0};
    bool is_overloaded{false};
};

class concurrency_metrics {
public:
    static concurrency_metrics& instance();

    void inc_node_connections(const std::string& tag);
    void dec_node_connections(const std::string& tag);
    void add_node_bytes(const std::string& tag, uint64_t up, uint64_t down);
    void update_node_latency(const std::string& tag, int latency_ms);
    void update_node_health(const std::string& tag, const std::string& health);

    void inc_user_connections(const std::string& username);
    void dec_user_connections(const std::string& username);
    void add_user_bytes(const std::string& username, uint64_t up, uint64_t down);

    void set_queued_count(int count);

    void record_history_snapshot();
    global_stats get_global_stats() const;
    node_metrics get_node_metrics(const std::string& tag) const;
    std::vector<node_metrics> get_all_node_metrics() const;
    user_metrics get_user_metrics(const std::string& username) const;
    std::vector<user_metrics> get_all_user_metrics() const;
    std::vector<history_snapshot> get_history(int minutes = 1440) const;
    std::string get_realtime_push_data() const;

    void start_history_collection(int interval_sec = 60);
    void stop_history_collection();

private:
    concurrency_metrics() = default;

    std::unordered_map<std::string, node_metrics> m_node_metrics;
    mutable std::shared_mutex m_node_mutex;

    std::unordered_map<std::string, user_metrics> m_user_metrics;
    mutable std::shared_mutex m_user_mutex;

    std::vector<history_snapshot> m_history;
    mutable std::shared_mutex m_history_mutex;
    static constexpr size_t MAX_HISTORY = 1440;

    std::atomic<int> m_active_connections{0};
    std::atomic<int> m_total_connections{0};
    std::atomic<int> m_queued_connections{0};

    std::atomic<bool> m_history_running{false};
};
