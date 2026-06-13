#pragma once
#include <string>
#include <vector>
#include <optional>
#include <shared_mutex>
#include <chrono>
#include <atomic>
#include <unordered_map>
#include <memory>
#include "concurrency/ip_affinity_table.h"

enum class schedule_strategy {
    round_robin,
    weighted_round_robin,
    least_connections,
    lowest_latency
};

struct schedule_result {
    std::string node_tag;
    std::string strategy_used;
    bool from_affinity{false};
    int64_t decision_us{0};
};

struct node_load_info {
    std::string tag;
    int active_connections{0};
    int weight{100};
    int latency_ms{0};
    bool available{true};
};

class load_balancer {
public:
    static load_balancer& instance();

    void init(schedule_strategy strategy, bool ip_affinity_enabled,
              int ip_affinity_timeout_sec, int ip_affinity_max_records,
              int node_capacity_threshold);

    std::optional<schedule_result> select_node(const std::string& client_ip);
    std::optional<schedule_result> select_fallback_node(
        const std::string& client_ip,
        const std::vector<std::string>& exclude_tags);

    void set_strategy(schedule_strategy strategy);
    schedule_strategy get_strategy() const;
    void set_ip_affinity_enabled(bool enabled);
    bool is_ip_affinity_enabled() const;
    void set_node_capacity_threshold(int threshold);
    int get_node_capacity_threshold() const;
    bool set_node_weight(const std::string& tag, int weight);

    void inc_node_connections(const std::string& tag);
    void dec_node_connections(const std::string& tag);
    int get_node_connections(const std::string& tag) const;

    static schedule_strategy strategy_from_string(const std::string& name);
    static std::string strategy_to_string(schedule_strategy strategy);

private:
    load_balancer() = default;

    std::optional<std::string> do_round_robin(const std::vector<node_load_info>& candidates);
    std::optional<std::string> do_weighted_round_robin(const std::vector<node_load_info>& candidates);
    std::optional<std::string> do_least_connections(const std::vector<node_load_info>& candidates);
    std::optional<std::string> do_lowest_latency(const std::vector<node_load_info>& candidates);

    std::vector<node_load_info> get_candidates(const std::vector<std::string>& exclude_tags = {});

    std::atomic<size_t> m_rr_index{0};
    std::vector<int> m_wrr_current_weights;
    std::shared_mutex m_wrr_mutex;

    std::atomic<schedule_strategy> m_strategy{schedule_strategy::round_robin};
    std::atomic<bool> m_ip_affinity_enabled{true};
    std::atomic<int> m_node_capacity_threshold{1000};

    ip_affinity_table m_affinity;

    std::unordered_map<std::string, std::unique_ptr<std::atomic<int>>> m_node_connections;
    mutable std::shared_mutex m_node_conn_mutex;

    std::unordered_map<std::string, int> m_node_weights;
    mutable std::shared_mutex m_node_weight_mutex;
};
