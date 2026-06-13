#include "concurrency/connection_admitter.h"
#include "concurrency/overload_protector.h"
#include "concurrency/rate_limiter.h"
#include "log_monitor/structured_logger.h"
#include <mutex>

connection_admitter& connection_admitter::instance() {
    static connection_admitter inst;
    return inst;
}

void connection_admitter::init(int max_connections, int max_connections_per_user,
                                bool queue_enabled, int max_queue_size,
                                int queue_timeout_seconds) {
    m_max_connections = max_connections;
    m_max_connections_per_user = max_connections_per_user;
    m_queue_enabled = queue_enabled;
    m_max_queue_size = max_queue_size;
    m_queue_timeout_seconds = queue_timeout_seconds;
}

admit_response connection_admitter::request_admit(const std::string& session_id,
                                                    const std::string& client_ip,
                                                    const std::string& username,
                                                    std::function<void()> on_admit,
                                                    std::function<void()> on_timeout,
                                                    bool is_reconnect) {
    if (!rate_limiter::instance().check_connection_rate(client_ip)) {
        return {admit_result::rejected_rate, "rate_limited", 0};
    }

    if (!rate_limiter::instance().check_auth_rate(client_ip)) {
        return {admit_result::rejected_banned, "ip_banned", 0};
    }

    if (overload_protector::instance().is_overloaded()) {
        return {admit_result::rejected_overload, "system_overloaded", 0};
    }

    {
        std::shared_lock lock(m_user_mutex);
        auto it = m_user_connections.find(username);
        if (it != m_user_connections.end() && it->second >= m_max_connections_per_user.load()) {
            return {admit_result::rejected_user, "user_limit_reached", 0};
        }
    }

    if (m_active_connections.load() >= m_max_connections.load()) {
        if (!m_queue_enabled.load()) {
            return {admit_result::rejected_total, "total_limit_reached", 0};
        }

        std::unique_lock lock(m_queue_mutex);
        if (static_cast<int>(m_queue.size()) >= m_max_queue_size.load()) {
            return {admit_result::rejected_total, "queue_full", 0};
        }

        queued_connection qc;
        qc.session_id = session_id;
        qc.username = username;
        qc.client_ip = client_ip;
        qc.enqueue_time = std::chrono::steady_clock::now();
        qc.on_admit = std::move(on_admit);
        qc.on_timeout = std::move(on_timeout);
        qc.is_reconnect = is_reconnect;

        if (is_reconnect) {
            auto insert_pos = m_queue.begin();
            for (; insert_pos != m_queue.end(); ++insert_pos) {
                if (!insert_pos->is_reconnect) break;
            }
            m_queue.insert(insert_pos, std::move(qc));
        } else {
            m_queue.push_back(std::move(qc));
        }

        int pos = static_cast<int>(m_queue.size());
        structured_logger::instance().info("connection_admitter",
            "Connection queued: session=" + session_id + " position=" + std::to_string(pos));
        return {admit_result::queued, "queued", pos};
    }

    m_active_connections.fetch_add(1);
    {
        std::unique_lock lock(m_user_mutex);
        m_user_connections[username]++;
    }

    return {admit_result::accepted, "accepted", 0};
}

void connection_admitter::release_connection(const std::string& username) {
    m_active_connections.fetch_sub(1);
    {
        std::unique_lock lock(m_user_mutex);
        auto it = m_user_connections.find(username);
        if (it != m_user_connections.end()) {
            it->second--;
            if (it->second <= 0) {
                m_user_connections.erase(it);
            }
        }
    }
    dequeue_connections();
}

void connection_admitter::dequeue_connections() {
    std::unique_lock lock(m_queue_mutex);
    while (!m_queue.empty() && m_active_connections.load() < m_max_connections.load()) {
        auto& front = m_queue.front();
        auto callback = std::move(front.on_admit);
        auto session_id = front.session_id;
        auto username = front.username;
        m_queue.pop_front();

        m_active_connections.fetch_add(1);
        {
            std::unique_lock user_lock(m_user_mutex);
            m_user_connections[username]++;
        }

        lock.unlock();
        if (callback) {
            structured_logger::instance().info("connection_admitter",
                "Queue admit: session=" + session_id);
            callback();
        }
        lock.lock();
    }

    check_queue_timeouts();
}

void connection_admitter::check_queue_timeouts() {
    auto now = std::chrono::steady_clock::now();
    for (auto it = m_queue.begin(); it != m_queue.end();) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - it->enqueue_time).count();
        if (elapsed >= m_queue_timeout_seconds) {
            auto callback = std::move(it->on_timeout);
            auto session_id = it->session_id;
            it = m_queue.erase(it);
            structured_logger::instance().warn("connection_admitter",
                "Queue timeout: session=" + session_id);
            if (callback) callback();
        } else {
            ++it;
        }
    }
}

int connection_admitter::get_user_connections(const std::string& username) const {
    std::shared_lock lock(m_user_mutex);
    auto it = m_user_connections.find(username);
    return (it != m_user_connections.end()) ? it->second : 0;
}

int connection_admitter::get_active_connections() const {
    return m_active_connections.load();
}

int connection_admitter::get_queued_count() const {
    std::shared_lock lock(m_queue_mutex);
    return static_cast<int>(m_queue.size());
}

void connection_admitter::set_max_connections(int max) { m_max_connections = max; }
void connection_admitter::set_max_connections_per_user(int max) { m_max_connections_per_user = max; }
void connection_admitter::set_queue_enabled(bool enabled) { m_queue_enabled = enabled; }
void connection_admitter::set_max_queue_size(int size) { m_max_queue_size = size; }
