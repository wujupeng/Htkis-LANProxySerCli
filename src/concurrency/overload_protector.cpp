#include "concurrency/overload_protector.h"
#include <sys/resource.h>
#include <fstream>
#include <mutex>

overload_protector& overload_protector::instance() {
    static overload_protector inst;
    return inst;
}

void overload_protector::init(double fd_threshold_pct, double mem_threshold_pct,
                                int max_total_connections, int max_per_user) {
    m_fd_threshold_pct = fd_threshold_pct;
    m_mem_threshold_pct = mem_threshold_pct;
    m_max_total_connections = max_total_connections;
    m_max_per_user = max_per_user;
}

overload_status overload_protector::check() const {
    overload_status status;

    status.total_connections = m_total_connections.load();
    if (status.total_connections >= m_max_total_connections) {
        status.is_overloaded = true;
        status.reason = "total_connections_limit";
        return status;
    }

    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) == 0 && rl.rlim_cur > 0) {
        int used_fds = 0;
        std::ifstream fds("/proc/self/fd");
        if (fds.is_open()) {
            std::string line;
            while (std::getline(fds, line)) used_fds++;
        }
        status.fd_usage_pct = static_cast<double>(used_fds) / rl.rlim_cur;
        if (status.fd_usage_pct >= m_fd_threshold_pct) {
            status.is_overloaded = true;
            status.reason = "fd_limit";
        }
    }

    std::ifstream meminfo("/proc/meminfo");
    if (meminfo.is_open()) {
        long mem_total = 0, mem_available = 0;
        std::string line;
        while (std::getline(meminfo, line)) {
            if (line.find("MemTotal:") == 0) {
                sscanf(line.c_str(), "MemTotal: %ld kB", &mem_total);
            } else if (line.find("MemAvailable:") == 0) {
                sscanf(line.c_str(), "MemAvailable: %ld kB", &mem_available);
            }
        }
        if (mem_total > 0) {
            status.mem_usage_pct = 1.0 - static_cast<double>(mem_available) / mem_total;
            if (status.mem_usage_pct >= m_mem_threshold_pct) {
                status.is_overloaded = true;
                status.reason = "memory_limit";
            }
        }
    }

    return status;
}

bool overload_protector::is_overloaded() const {
    return check().is_overloaded;
}

bool overload_protector::check_user_limit(const std::string& username) const {
    std::shared_lock lock(m_user_mutex);
    auto it = m_user_connections.find(username);
    if (it == m_user_connections.end()) return true;
    return it->second->load() < m_max_per_user;
}

void overload_protector::inc_user_connections(const std::string& username) {
    std::unique_lock lock(m_user_mutex);
    auto it = m_user_connections.find(username);
    if (it != m_user_connections.end()) {
        it->second->fetch_add(1);
    } else {
        m_user_connections[username] = std::make_unique<std::atomic<int>>(1);
    }
}

void overload_protector::dec_user_connections(const std::string& username) {
    std::unique_lock lock(m_user_mutex);
    auto it = m_user_connections.find(username);
    if (it != m_user_connections.end()) {
        int prev = it->second->fetch_sub(1);
        if (prev <= 1) m_user_connections.erase(it);
    }
}

int overload_protector::get_user_connections(const std::string& username) const {
    std::shared_lock lock(m_user_mutex);
    auto it = m_user_connections.find(username);
    if (it != m_user_connections.end()) return it->second->load();
    return 0;
}

int overload_protector::get_total_connections() const { return m_total_connections.load(); }
void overload_protector::inc_total_connections() { m_total_connections.fetch_add(1); }
void overload_protector::dec_total_connections() {
    int current = m_total_connections.load();
    while (current > 0) {
        if (m_total_connections.compare_exchange_weak(current, current - 1)) {
            break;
        }
    }
}

overload_status overload_protector::get_status() const { return check(); }
