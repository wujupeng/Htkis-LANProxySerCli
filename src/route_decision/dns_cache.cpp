#include "route_decision/dns_cache.h"

dns_cache& dns_cache::instance() {
    static dns_cache inst;
    return inst;
}

bool dns_cache::get(const std::string& domain, std::string& ip) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_cache.find(domain);
    if (it == m_cache.end()) return false;

    auto now = std::chrono::steady_clock::now();
    if (now > it->second.expire_time) {
        m_cache.erase(it);
        return false;
    }

    ip = it->second.ip;
    return true;
}

void dns_cache::put(const std::string& domain, const std::string& ip, int ttl_seconds) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto expire = std::chrono::steady_clock::now() + std::chrono::seconds(ttl_seconds);
    m_cache[domain] = {ip, expire};
}

void dns_cache::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cache.clear();
}
