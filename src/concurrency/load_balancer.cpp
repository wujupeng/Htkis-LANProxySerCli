#include "concurrency/load_balancer.h"
#include "concurrency/ip_affinity_table.h"
#include "concurrency/node_health_checker.h"
#include "v2rayn_manager/vm_node_manager.h"
#include "log_monitor/structured_logger.h"
#include <algorithm>
#include <numeric>
#include <mutex>

load_balancer& load_balancer::instance() {
    static load_balancer inst;
    return inst;
}

void load_balancer::init(schedule_strategy strategy, bool ip_affinity_enabled,
                          int ip_affinity_timeout_sec, int ip_affinity_max_records,
                          int node_capacity_threshold) {
    m_strategy = strategy;
    m_ip_affinity_enabled = ip_affinity_enabled;
    m_node_capacity_threshold = node_capacity_threshold;
    m_affinity.init(ip_affinity_timeout_sec, ip_affinity_max_records);
}

std::vector<node_load_info> load_balancer::get_candidates(const std::vector<std::string>& exclude_tags) {
    auto nodes = vm_node_manager::instance().list_nodes(false);
    std::vector<node_load_info> candidates;

    for (const auto& node : nodes) {
        if (std::find(exclude_tags.begin(), exclude_tags.end(), node.tag) != exclude_tags.end())
            continue;

        node_load_info info;
        info.tag = node.tag;

        auto health_status = node_health_checker::instance().get_status(node.tag);
        info.available = (health_status != node_health_status::unavailable);
        if (!info.available) continue;

        info.latency_ms = node_health_checker::instance().get_latency_ms(node.tag);

        {
            std::shared_lock lock(m_node_conn_mutex);
            auto it = m_node_connections.find(node.tag);
            info.active_connections = (it != m_node_connections.end()) ? it->second->load() : 0;
        }

        if (info.active_connections >= m_node_capacity_threshold.load()) continue;

        {
            std::shared_lock lock(m_node_weight_mutex);
            auto wit = m_node_weights.find(node.tag);
            info.weight = (wit != m_node_weights.end()) ? wit->second : 100;
        }

        candidates.push_back(info);
    }
    return candidates;
}

std::optional<schedule_result> load_balancer::select_node(const std::string& client_ip) {
    auto start = std::chrono::steady_clock::now();

    if (m_ip_affinity_enabled.load()) {
        auto affinity_node = m_affinity.lookup(client_ip);
        if (affinity_node.has_value()) {
            auto status = node_health_checker::instance().get_status(*affinity_node);
            if (status != node_health_status::unavailable) {
                int conns = get_node_connections(*affinity_node);
                if (conns < m_node_capacity_threshold.load()) {
                    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::steady_clock::now() - start).count();
                    return schedule_result{*affinity_node, "ip_affinity", true, elapsed};
                }
            }
            m_affinity.invalidate(client_ip);
        }
    }

    auto candidates = get_candidates();
    if (candidates.empty()) {
        return std::nullopt;
    }

    std::optional<std::string> selected;
    auto strategy = m_strategy.load();
    std::string strategy_name = strategy_to_string(strategy);

    switch (strategy) {
        case schedule_strategy::round_robin:
            selected = do_round_robin(candidates); break;
        case schedule_strategy::weighted_round_robin:
            selected = do_weighted_round_robin(candidates); break;
        case schedule_strategy::least_connections:
            selected = do_least_connections(candidates); break;
        case schedule_strategy::lowest_latency:
            selected = do_lowest_latency(candidates); break;
    }

    if (selected.has_value() && m_ip_affinity_enabled.load()) {
        m_affinity.update(client_ip, *selected);
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - start).count();
    return schedule_result{selected.value_or(""), strategy_name, false, elapsed};
}

std::optional<schedule_result> load_balancer::select_fallback_node(
    const std::string& client_ip, const std::vector<std::string>& exclude_tags) {
    auto start = std::chrono::steady_clock::now();

    auto candidates = get_candidates(exclude_tags);
    if (candidates.empty()) return std::nullopt;

    auto selected = do_least_connections(candidates);

    if (selected.has_value() && m_ip_affinity_enabled.load()) {
        m_affinity.update(client_ip, *selected);
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - start).count();
    return schedule_result{selected.value_or(""), "failover", false, elapsed};
}

std::optional<std::string> load_balancer::do_round_robin(const std::vector<node_load_info>& candidates) {
    size_t idx = m_rr_index.fetch_add(1) % candidates.size();
    return candidates[idx].tag;
}

std::optional<std::string> load_balancer::do_weighted_round_robin(const std::vector<node_load_info>& candidates) {
    std::unique_lock lock(m_wrr_mutex);

    if (m_wrr_current_weights.size() != candidates.size()) {
        m_wrr_current_weights.resize(candidates.size(), 0);
    }

    int total_weight = 0;
    for (const auto& c : candidates) total_weight += c.weight;
    if (total_weight == 0) return candidates.empty() ? std::nullopt : std::optional<std::string>(candidates[0].tag);

    for (size_t i = 0; i < candidates.size(); ++i) {
        m_wrr_current_weights[i] += candidates[i].weight;
    }

    size_t best = 0;
    for (size_t i = 1; i < candidates.size(); ++i) {
        if (m_wrr_current_weights[i] > m_wrr_current_weights[best]) best = i;
    }
    m_wrr_current_weights[best] -= total_weight;

    return candidates[best].tag;
}

std::optional<std::string> load_balancer::do_least_connections(const std::vector<node_load_info>& candidates) {
    auto best = std::min_element(candidates.begin(), candidates.end(),
        [](const node_load_info& a, const node_load_info& b) {
            return a.active_connections < b.active_connections;
        });
    return best->tag;
}

std::optional<std::string> load_balancer::do_lowest_latency(const std::vector<node_load_info>& candidates) {
    int min_latency = std::numeric_limits<int>::max();
    for (const auto& c : candidates) {
        if (c.latency_ms > 0 && c.latency_ms < min_latency) min_latency = c.latency_ms;
    }

    std::vector<node_load_info> low_latency;
    for (const auto& c : candidates) {
        if (c.latency_ms <= min_latency + 10) low_latency.push_back(c);
    }

    if (low_latency.empty()) return do_least_connections(candidates);
    return do_least_connections(low_latency);
}

void load_balancer::set_strategy(schedule_strategy strategy) { m_strategy = strategy; }
schedule_strategy load_balancer::get_strategy() const { return m_strategy.load(); }
void load_balancer::set_ip_affinity_enabled(bool enabled) { m_ip_affinity_enabled = enabled; }
bool load_balancer::is_ip_affinity_enabled() const { return m_ip_affinity_enabled.load(); }
void load_balancer::set_node_capacity_threshold(int threshold) { m_node_capacity_threshold = threshold; }
int load_balancer::get_node_capacity_threshold() const { return m_node_capacity_threshold.load(); }

bool load_balancer::set_node_weight(const std::string& tag, int weight) {
    std::unique_lock lock(m_node_weight_mutex);
    m_node_weights[tag] = weight;
    return true;
}

void load_balancer::inc_node_connections(const std::string& tag) {
    std::unique_lock lock(m_node_conn_mutex);
    auto it = m_node_connections.find(tag);
    if (it != m_node_connections.end()) {
        it->second->fetch_add(1);
    } else {
        m_node_connections[tag] = std::make_unique<std::atomic<int>>(1);
    }
}

void load_balancer::dec_node_connections(const std::string& tag) {
    std::unique_lock lock(m_node_conn_mutex);
    auto it = m_node_connections.find(tag);
    if (it != m_node_connections.end()) {
        int prev = it->second->fetch_sub(1);
        if (prev <= 1) m_node_connections.erase(it);
    }
}

int load_balancer::get_node_connections(const std::string& tag) const {
    std::shared_lock lock(m_node_conn_mutex);
    auto it = m_node_connections.find(tag);
    if (it != m_node_connections.end()) return it->second->load();
    return 0;
}

schedule_strategy load_balancer::strategy_from_string(const std::string& name) {
    if (name == "weighted_round_robin") return schedule_strategy::weighted_round_robin;
    if (name == "least_connections") return schedule_strategy::least_connections;
    if (name == "lowest_latency") return schedule_strategy::lowest_latency;
    return schedule_strategy::round_robin;
}

std::string load_balancer::strategy_to_string(schedule_strategy strategy) {
    switch (strategy) {
        case schedule_strategy::round_robin: return "round_robin";
        case schedule_strategy::weighted_round_robin: return "weighted_round_robin";
        case schedule_strategy::least_connections: return "least_connections";
        case schedule_strategy::lowest_latency: return "lowest_latency";
    }
    return "round_robin";
}
