#include "concurrency/ip_affinity_table.h"
#include <mutex>

void ip_affinity_table::init(int timeout_sec, int max_records) {
    m_timeout_sec = timeout_sec;
    m_max_records = max_records;
}

std::optional<std::string> ip_affinity_table::lookup(const std::string& client_ip) {
    std::shared_lock lock(m_mutex);
    auto it = m_index.find(client_ip);
    if (it == m_index.end()) return std::nullopt;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second->last_access).count();
    if (elapsed > m_timeout_sec) return std::nullopt;

    return it->second->node_tag;
}

void ip_affinity_table::update(const std::string& client_ip, const std::string& node_tag) {
    std::unique_lock lock(m_mutex);

    auto it = m_index.find(client_ip);
    if (it != m_index.end()) {
        it->second->node_tag = node_tag;
        it->second->last_access = std::chrono::steady_clock::now();
        m_lru.splice(m_lru.begin(), m_lru, it->second);
        return;
    }

    while (static_cast<int>(m_lru.size()) >= m_max_records) {
        auto& back = m_lru.back();
        m_index.erase(back.client_ip);
        m_lru.pop_back();
    }

    m_lru.push_front({client_ip, node_tag, std::chrono::steady_clock::now()});
    m_index[client_ip] = m_lru.begin();
}

void ip_affinity_table::invalidate(const std::string& client_ip) {
    std::unique_lock lock(m_mutex);
    auto it = m_index.find(client_ip);
    if (it != m_index.end()) {
        m_lru.erase(it->second);
        m_index.erase(it);
    }
}

void ip_affinity_table::invalidate_node(const std::string& node_tag) {
    std::unique_lock lock(m_mutex);
    for (auto it = m_lru.begin(); it != m_lru.end();) {
        if (it->node_tag == node_tag) {
            m_index.erase(it->client_ip);
            it = m_lru.erase(it);
        } else {
            ++it;
        }
    }
}

void ip_affinity_table::cleanup_expired() {
    std::unique_lock lock(m_mutex);
    auto now = std::chrono::steady_clock::now();
    for (auto it = m_lru.rbegin(); it != m_lru.rend();) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->last_access).count();
        if (elapsed <= m_timeout_sec) break;
        m_index.erase(it->client_ip);
        ++it;
    }
}

void ip_affinity_table::clear() {
    std::unique_lock lock(m_mutex);
    m_index.clear();
    m_lru.clear();
}

int ip_affinity_table::size() const {
    std::shared_lock lock(m_mutex);
    return static_cast<int>(m_lru.size());
}
