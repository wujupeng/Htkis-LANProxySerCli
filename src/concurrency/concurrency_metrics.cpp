#include "concurrency/concurrency_metrics.h"
#include "concurrency/overload_protector.h"
#include "concurrency/connection_admitter.h"
#include <thread>
#include <mutex>
#include <nlohmann/json.hpp>

concurrency_metrics& concurrency_metrics::instance() {
    static concurrency_metrics inst;
    return inst;
}

void concurrency_metrics::inc_node_connections(const std::string& tag) {
    std::unique_lock lock(m_node_mutex);
    auto& m = m_node_metrics[tag];
    m.tag = tag;
    m.active_connections++;
    m.total_connections++;
}

void concurrency_metrics::dec_node_connections(const std::string& tag) {
    std::unique_lock lock(m_node_mutex);
    auto it = m_node_metrics.find(tag);
    if (it != m_node_metrics.end() && it->second.active_connections > 0) {
        it->second.active_connections--;
    }
}

void concurrency_metrics::add_node_bytes(const std::string& tag, uint64_t up, uint64_t down) {
    std::unique_lock lock(m_node_mutex);
    auto& m = m_node_metrics[tag];
    m.tag = tag;
    m.bytes_up += up;
    m.bytes_down += down;
}

void concurrency_metrics::update_node_latency(const std::string& tag, int latency_ms) {
    std::unique_lock lock(m_node_mutex);
    auto& m = m_node_metrics[tag];
    m.tag = tag;
    m.latency_ms = latency_ms;
}

void concurrency_metrics::update_node_health(const std::string& tag, const std::string& health) {
    std::unique_lock lock(m_node_mutex);
    auto& m = m_node_metrics[tag];
    m.tag = tag;
    m.health = health;
}

void concurrency_metrics::inc_user_connections(const std::string& username) {
    std::unique_lock lock(m_user_mutex);
    auto& m = m_user_metrics[username];
    m.username = username;
    m.active_connections++;
    m.total_connections++;
    m_active_connections.fetch_add(1);
    m_total_connections.fetch_add(1);
}

void concurrency_metrics::dec_user_connections(const std::string& username) {
    std::unique_lock lock(m_user_mutex);
    auto it = m_user_metrics.find(username);
    if (it != m_user_metrics.end() && it->second.active_connections > 0) {
        it->second.active_connections--;
    }
    int current = m_active_connections.load();
    while (current > 0) {
        if (m_active_connections.compare_exchange_weak(current, current - 1)) {
            break;
        }
    }
}

void concurrency_metrics::add_user_bytes(const std::string& username, uint64_t up, uint64_t down) {
    std::unique_lock lock(m_user_mutex);
    auto& m = m_user_metrics[username];
    m.username = username;
    m.bytes_up += up;
    m.bytes_down += down;
}

void concurrency_metrics::set_queued_count(int count) {
    m_queued_connections = count;
}

void concurrency_metrics::record_history_snapshot() {
    history_snapshot snap;
    snap.timestamp = std::chrono::steady_clock::now();
    snap.active_connections = m_active_connections.load();
    snap.queued_connections = m_queued_connections.load();

    {
        std::shared_lock lock(m_node_mutex);
        for (const auto& [tag, m] : m_node_metrics) {
            snap.node_connections[tag] = m.active_connections;
        }
    }

    {
        std::unique_lock lock(m_history_mutex);
        m_history.push_back(std::move(snap));
        if (m_history.size() > MAX_HISTORY) {
            m_history.erase(m_history.begin());
        }
    }
}

global_stats concurrency_metrics::get_global_stats() const {
    global_stats stats;
    stats.active_connections = m_active_connections.load();
    stats.total_connections = m_total_connections.load();
    stats.queued_connections = m_queued_connections.load();
    stats.max_connections = connection_admitter::instance().get_max_connections();
    stats.is_overloaded = overload_protector::instance().is_overloaded();
    return stats;
}

node_metrics concurrency_metrics::get_node_metrics(const std::string& tag) const {
    std::shared_lock lock(m_node_mutex);
    auto it = m_node_metrics.find(tag);
    if (it != m_node_metrics.end()) return it->second;
    return {};
}

std::vector<node_metrics> concurrency_metrics::get_all_node_metrics() const {
    std::shared_lock lock(m_node_mutex);
    std::vector<node_metrics> result;
    for (const auto& [_, m] : m_node_metrics) {
        result.push_back(m);
    }
    return result;
}

user_metrics concurrency_metrics::get_user_metrics(const std::string& username) const {
    std::shared_lock lock(m_user_mutex);
    auto it = m_user_metrics.find(username);
    if (it != m_user_metrics.end()) return it->second;
    return {};
}

std::vector<user_metrics> concurrency_metrics::get_all_user_metrics() const {
    std::shared_lock lock(m_user_mutex);
    std::vector<user_metrics> result;
    for (const auto& [_, m] : m_user_metrics) {
        result.push_back(m);
    }
    return result;
}

std::vector<history_snapshot> concurrency_metrics::get_history(int minutes) const {
    std::shared_lock lock(m_history_mutex);
    std::vector<history_snapshot> result;
    auto cutoff = std::chrono::steady_clock::now() - std::chrono::minutes(minutes);
    for (const auto& snap : m_history) {
        if (snap.timestamp >= cutoff) {
            result.push_back(snap);
        }
    }
    return result;
}

std::string concurrency_metrics::get_realtime_push_data() const {
    nlohmann::json j;
    auto stats = get_global_stats();
    j["active_connections"] = stats.active_connections;
    j["total_connections"] = stats.total_connections;
    j["queued_connections"] = stats.queued_connections;
    j["is_overloaded"] = stats.is_overloaded;

    auto nodes = get_all_node_metrics();
    for (const auto& n : nodes) {
        nlohmann::json nj;
        nj["tag"] = n.tag;
        nj["active_connections"] = n.active_connections;
        nj["latency_ms"] = n.latency_ms;
        nj["health"] = n.health;
        j["nodes"].push_back(nj);
    }

    return j.dump();
}

void concurrency_metrics::start_history_collection(int interval_sec) {
    if (m_history_running.exchange(true)) return;
    std::thread([this, interval_sec]() {
        while (m_history_running.load()) {
            record_history_snapshot();
            for (int i = 0; i < interval_sec && m_history_running.load(); ++i) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    }).detach();
}

void concurrency_metrics::stop_history_collection() {
    m_history_running = false;
}
