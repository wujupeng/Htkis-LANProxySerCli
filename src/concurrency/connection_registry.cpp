#include "concurrency/connection_registry.h"
#include <mutex>

connection_registry& connection_registry::instance() {
    static connection_registry inst;
    return inst;
}

void connection_registry::register_connection(const std::string& session_id,
                                               const std::string& client_ip,
                                               const std::string& username) {
    std::unique_lock lock(m_mutex);
    auto& rec = m_connections[session_id];
    rec.session_id = session_id;
    rec.client_ip = client_ip;
    rec.username = username;
    rec.state = connection_state::authenticating;
    rec.admit_time = std::chrono::steady_clock::now();
    rec.idle_since = std::chrono::steady_clock::now();
}

void connection_registry::unregister_connection(const std::string& session_id) {
    std::unique_lock lock(m_mutex);
    m_connections.erase(session_id);
}

void connection_registry::update_state(const std::string& session_id, connection_state state) {
    std::unique_lock lock(m_mutex);
    auto it = m_connections.find(session_id);
    if (it != m_connections.end()) {
        it->second.state = state;
        if (state == connection_state::active) {
            it->second.idle_since = std::chrono::steady_clock::now();
        }
    }
}

void connection_registry::update_node(const std::string& session_id, const std::string& node_tag) {
    std::unique_lock lock(m_mutex);
    auto it = m_connections.find(session_id);
    if (it != m_connections.end()) {
        it->second.assigned_node_tag = node_tag;
    }
}

void connection_registry::mark_idle(const std::string& session_id) {
    std::unique_lock lock(m_mutex);
    auto it = m_connections.find(session_id);
    if (it != m_connections.end()) {
        it->second.state = connection_state::idle;
        it->second.idle_since = std::chrono::steady_clock::now();
    }
}

void connection_registry::mark_active(const std::string& session_id) {
    std::unique_lock lock(m_mutex);
    auto it = m_connections.find(session_id);
    if (it != m_connections.end()) {
        it->second.state = connection_state::active;
        it->second.idle_since = std::chrono::steady_clock::now();
    }
}

void connection_registry::set_exempt_idle(const std::string& session_id, bool exempt) {
    std::unique_lock lock(m_mutex);
    auto it = m_connections.find(session_id);
    if (it != m_connections.end()) {
        it->second.exempt_idle = exempt;
    }
}

std::optional<connection_record> connection_registry::get(const std::string& session_id) const {
    std::shared_lock lock(m_mutex);
    auto it = m_connections.find(session_id);
    if (it == m_connections.end()) return std::nullopt;
    return it->second;
}

std::vector<connection_record> connection_registry::get_all() const {
    std::shared_lock lock(m_mutex);
    std::vector<connection_record> result;
    result.reserve(m_connections.size());
    for (const auto& [_, rec] : m_connections) {
        result.push_back(rec);
    }
    return result;
}

std::vector<connection_record> connection_registry::get_by_user(const std::string& username) const {
    std::shared_lock lock(m_mutex);
    std::vector<connection_record> result;
    for (const auto& [_, rec] : m_connections) {
        if (rec.username == username) result.push_back(rec);
    }
    return result;
}

std::vector<connection_record> connection_registry::get_by_node(const std::string& node_tag) const {
    std::shared_lock lock(m_mutex);
    std::vector<connection_record> result;
    for (const auto& [_, rec] : m_connections) {
        if (rec.assigned_node_tag == node_tag) result.push_back(rec);
    }
    return result;
}

int connection_registry::get_node_connection_count(const std::string& node_tag) const {
    std::shared_lock lock(m_mutex);
    int count = 0;
    for (const auto& [_, rec] : m_connections) {
        if (rec.assigned_node_tag == node_tag &&
            (rec.state == connection_state::active || rec.state == connection_state::idle)) {
            count++;
        }
    }
    return count;
}

int connection_registry::get_user_connection_count(const std::string& username) const {
    std::shared_lock lock(m_mutex);
    int count = 0;
    for (const auto& [_, rec] : m_connections) {
        if (rec.username == username &&
            (rec.state == connection_state::active || rec.state == connection_state::idle)) {
            count++;
        }
    }
    return count;
}

std::vector<connection_record> connection_registry::get_timeout_candidates() const {
    std::shared_lock lock(m_mutex);
    std::vector<connection_record> result;
    auto now = std::chrono::steady_clock::now();
    for (const auto& [_, rec] : m_connections) {
        if (rec.state == connection_state::authenticating ||
            rec.state == connection_state::handshaking ||
            rec.state == connection_state::idle) {
            result.push_back(rec);
        }
        if (rec.state == connection_state::active || rec.state == connection_state::idle) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - rec.admit_time).count();
            if (elapsed > 28800) {
                result.push_back(rec);
            }
        }
    }
    return result;
}

int connection_registry::size() const {
    std::shared_lock lock(m_mutex);
    return static_cast<int>(m_connections.size());
}

void connection_registry::clear() {
    std::unique_lock lock(m_mutex);
    m_connections.clear();
}
