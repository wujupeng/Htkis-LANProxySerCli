#include "concurrency/node_health_checker.h"
#include "v2rayn_manager/vm_node_manager.h"
#include "log_monitor/structured_logger.h"
#include <thread>
#include <asio.hpp>

node_health_checker& node_health_checker::instance() {
    static node_health_checker inst;
    return inst;
}

void node_health_checker::init(int check_interval_sec, int probe_timeout_sec, int failure_threshold) {
    m_check_interval_sec = check_interval_sec;
    m_probe_timeout_sec = probe_timeout_sec;
    m_failure_threshold = failure_threshold;
}

void node_health_checker::start() {
    if (m_running.exchange(true)) return;
    std::thread([this]() {
        while (m_running.load()) {
            periodic_check();
            for (int i = 0; i < m_check_interval_sec && m_running.load(); ++i) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    }).detach();
    structured_logger::instance().info("node_health_checker", "健康探测已启动");
}

void node_health_checker::stop() {
    m_running = false;
}

void node_health_checker::periodic_check() {
    auto nodes = vm_node_manager::instance().list_nodes(false);
    for (const auto& node : nodes) {
        do_probe(node.tag);
    }
}

void node_health_checker::do_probe(const std::string& tag) {
    auto node_opt = vm_node_manager::instance().get_node(tag);
    if (!node_opt.has_value()) return;

    auto& node = *node_opt;
    auto start = std::chrono::steady_clock::now();

    bool success = false;
    try {
        asio::io_context io;
        asio::ip::tcp::resolver resolver(io);
        auto results = resolver.resolve(node.address, std::to_string(node.port));
        asio::ip::tcp::socket socket(io);
        asio::async_connect(socket, results,
            [&](std::error_code ec, auto) { success = !ec; });
        io.run_for(std::chrono::seconds(m_probe_timeout_sec));
    } catch (...) {
        success = false;
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    std::unique_lock lock(m_mutex);
    auto& info = m_health[tag];
    info.tag = tag;
    info.last_check = std::chrono::steady_clock::now();
    info.total_checks++;

    if (success) {
        info.latency_ms = static_cast<int>(elapsed);
        info.consecutive_failures = 0;
        info.failed_checks = std::max(0, info.failed_checks - 1);
        if (info.consecutive_failures < m_failure_threshold) {
            info.status = node_health_status::available;
        }
    } else {
        info.consecutive_failures++;
        info.failed_checks++;
        if (info.consecutive_failures >= m_failure_threshold) {
            info.status = node_health_status::unavailable;
            structured_logger::instance().warn("node_health_checker",
                "节点 " + tag + " 不可用，连续失败 " + std::to_string(info.consecutive_failures) + " 次");
        } else if (info.consecutive_failures > 0) {
            info.status = node_health_status::unstable;
        }
    }
}

void node_health_checker::check_now(const std::string& tag) {
    do_probe(tag);
}

node_health_status node_health_checker::get_status(const std::string& tag) const {
    std::shared_lock lock(m_mutex);
    auto it = m_health.find(tag);
    if (it == m_health.end()) return node_health_status::available;
    return it->second.status;
}

int node_health_checker::get_latency_ms(const std::string& tag) const {
    std::shared_lock lock(m_mutex);
    auto it = m_health.find(tag);
    if (it == m_health.end()) return 0;
    return it->second.latency_ms;
}

node_health_info node_health_checker::get_info(const std::string& tag) const {
    std::shared_lock lock(m_mutex);
    auto it = m_health.find(tag);
    if (it == m_health.end()) return {};
    return it->second;
}

std::vector<node_health_info> node_health_checker::get_all_info() const {
    std::shared_lock lock(m_mutex);
    std::vector<node_health_info> result;
    for (const auto& [_, info] : m_health) {
        result.push_back(info);
    }
    return result;
}

void node_health_checker::mark_unavailable(const std::string& tag) {
    std::unique_lock lock(m_mutex);
    auto& info = m_health[tag];
    info.tag = tag;
    info.status = node_health_status::unavailable;
    info.consecutive_failures = m_failure_threshold;
}

void node_health_checker::mark_available(const std::string& tag) {
    std::unique_lock lock(m_mutex);
    auto& info = m_health[tag];
    info.tag = tag;
    info.status = node_health_status::available;
    info.consecutive_failures = 0;
}
