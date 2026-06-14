#include "log_monitor/metrics_collector.h"

metrics_collector& metrics_collector::instance() {
    static metrics_collector inst;
    return inst;
}

void metrics_collector::inc_active_connections() { m_active_connections.fetch_add(1); }
void metrics_collector::dec_active_connections() {
    int current = m_active_connections.load();
    while (current > 0) {
        if (m_active_connections.compare_exchange_weak(current, current - 1)) {
            break;
        }
    }
}
void metrics_collector::inc_total_connections() { m_total_connections++; }
void metrics_collector::add_direct_bytes_up(uint64_t b) { m_direct_bytes_up += b; }
void metrics_collector::add_direct_bytes_down(uint64_t b) { m_direct_bytes_down += b; }
void metrics_collector::add_proxy_bytes_up(uint64_t b) { m_proxy_bytes_up += b; }
void metrics_collector::add_proxy_bytes_down(uint64_t b) { m_proxy_bytes_down += b; }

nlohmann::json metrics_collector::snapshot() const {
    return {
        {"active_connections", m_active_connections.load()},
        {"total_connections", m_total_connections.load()},
        {"direct_bytes_up", m_direct_bytes_up.load()},
        {"direct_bytes_down", m_direct_bytes_down.load()},
        {"proxy_bytes_up", m_proxy_bytes_up.load()},
        {"proxy_bytes_down", m_proxy_bytes_down.load()}
    };
}
