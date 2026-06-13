#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <shared_mutex>
#include <optional>
#include <functional>

enum class connection_state {
    authenticating,
    handshaking,
    active,
    idle,
    queued,
    closing
};

struct connection_record {
    std::string session_id;
    std::string client_ip;
    std::string username;
    std::string assigned_node_tag;
    connection_state state{connection_state::authenticating};
    std::chrono::steady_clock::time_point admit_time;
    std::chrono::steady_clock::time_point idle_since;
    bool exempt_idle{false};
};

class connection_registry {
public:
    static connection_registry& instance();

    void register_connection(const std::string& session_id, const std::string& client_ip,
                             const std::string& username);
    void unregister_connection(const std::string& session_id);

    void update_state(const std::string& session_id, connection_state state);
    void update_node(const std::string& session_id, const std::string& node_tag);
    void mark_idle(const std::string& session_id);
    void mark_active(const std::string& session_id);
    void set_exempt_idle(const std::string& session_id, bool exempt);

    std::optional<connection_record> get(const std::string& session_id) const;
    std::vector<connection_record> get_all() const;
    std::vector<connection_record> get_by_user(const std::string& username) const;
    std::vector<connection_record> get_by_node(const std::string& node_tag) const;

    int get_node_connection_count(const std::string& node_tag) const;
    int get_user_connection_count(const std::string& username) const;
    std::vector<connection_record> get_timeout_candidates() const;

    int size() const;
    void clear();

private:
    connection_registry() = default;

    std::unordered_map<std::string, connection_record> m_connections;
    mutable std::shared_mutex m_mutex;
};
