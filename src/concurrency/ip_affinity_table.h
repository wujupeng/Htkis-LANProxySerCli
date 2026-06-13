#pragma once
#include <unordered_map>
#include <string>
#include <chrono>
#include <shared_mutex>
#include <list>
#include <optional>

struct affinity_record {
    std::string client_ip;
    std::string node_tag;
    std::chrono::steady_clock::time_point last_access;
};

class ip_affinity_table {
public:
    void init(int timeout_sec = 300, int max_records = 10000);

    std::optional<std::string> lookup(const std::string& client_ip);
    void update(const std::string& client_ip, const std::string& node_tag);
    void invalidate(const std::string& client_ip);
    void invalidate_node(const std::string& node_tag);
    void cleanup_expired();
    void clear();
    int size() const;

private:
    int m_timeout_sec{300};
    int m_max_records{10000};
    std::unordered_map<std::string, std::list<affinity_record>::iterator> m_index;
    std::list<affinity_record> m_lru;
    mutable std::shared_mutex m_mutex;
};
