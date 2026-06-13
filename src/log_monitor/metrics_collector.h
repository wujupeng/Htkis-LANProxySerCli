#pragma once
#include <atomic>
#include <cstdint>
#include <nlohmann/json.hpp>

class metrics_collector {
public:
    static metrics_collector& instance();

    void inc_active_connections();
    void dec_active_connections();
    void inc_total_connections();
    void add_direct_bytes_up(uint64_t bytes);
    void add_direct_bytes_down(uint64_t bytes);
    void add_proxy_bytes_up(uint64_t bytes);
    void add_proxy_bytes_down(uint64_t bytes);

    nlohmann::json snapshot() const;

private:
    metrics_collector() = default;

    std::atomic<int> m_active_connections{0};
    std::atomic<int64_t> m_total_connections{0};
    std::atomic<uint64_t> m_direct_bytes_up{0};
    std::atomic<uint64_t> m_direct_bytes_down{0};
    std::atomic<uint64_t> m_proxy_bytes_up{0};
    std::atomic<uint64_t> m_proxy_bytes_down{0};
};
