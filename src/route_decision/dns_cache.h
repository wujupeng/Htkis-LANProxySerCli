#pragma once
#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono>

struct dns_entry {
    std::string ip;
    std::chrono::steady_clock::time_point expire_time;
};

class dns_cache {
public:
    static dns_cache& instance();

    bool get(const std::string& domain, std::string& ip);
    void put(const std::string& domain, const std::string& ip,
             int ttl_seconds = 300);
    void clear();

private:
    dns_cache() = default;
    std::unordered_map<std::string, dns_entry> m_cache;
    std::mutex m_mutex;
    static constexpr int DEFAULT_TTL = 300;
};
