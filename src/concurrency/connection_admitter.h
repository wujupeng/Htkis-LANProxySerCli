#pragma once
#include <string>
#include <vector>
#include <deque>
#include <functional>
#include <chrono>
#include <shared_mutex>
#include <atomic>

enum class admit_result {
    accepted,
    queued,
    rejected_total,
    rejected_user,
    rejected_rate,
    rejected_banned,
    rejected_overload
};

struct admit_response {
    admit_result result{admit_result::rejected_total};
    std::string reason;
    int queue_position{0};
};

struct queued_connection {
    std::string session_id;
    std::string username;
    std::string client_ip;
    std::chrono::steady_clock::time_point enqueue_time;
    std::function<void()> on_admit;
    std::function<void()> on_timeout;
    bool is_reconnect{false};
};

class connection_admitter {
public:
    static connection_admitter& instance();

    void init(int max_connections = 4000, int max_connections_per_user = 50,
              bool queue_enabled = true, int max_queue_size = 200,
              int queue_timeout_seconds = 60);

    admit_response request_admit(const std::string& session_id,
                                  const std::string& client_ip,
                                  const std::string& username,
                                  std::function<void()> on_admit,
                                  std::function<void()> on_timeout,
                                  bool is_reconnect = false);

    void release_connection(const std::string& username);

    int get_user_connections(const std::string& username) const;
    int get_active_connections() const;
    int get_queued_count() const;
    int get_max_connections() const;

    void set_max_connections(int max);
    void set_max_connections_per_user(int max);
    void set_queue_enabled(bool enabled);
    void set_max_queue_size(int size);

private:
    connection_admitter() = default;
    void dequeue_connections();
    void check_queue_timeouts();

    std::atomic<int> m_max_connections{4000};
    std::atomic<int> m_max_connections_per_user{50};
    std::atomic<bool> m_queue_enabled{true};
    std::atomic<int> m_max_queue_size{200};
    int m_queue_timeout_seconds{60};

    std::atomic<int> m_active_connections{0};
    std::unordered_map<std::string, int> m_user_connections;
    mutable std::shared_mutex m_user_mutex;

    std::deque<queued_connection> m_queue;
    mutable std::shared_mutex m_queue_mutex;
};
