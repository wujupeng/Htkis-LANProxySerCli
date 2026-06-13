#include "concurrency/failover_handler.h"
#include "concurrency/node_health_checker.h"
#include "log_monitor/structured_logger.h"
#include <mutex>

failover_handler& failover_handler::instance() {
    static failover_handler inst;
    return inst;
}

std::optional<failover_result> failover_handler::on_connection_failed(
    const std::string& failed_tag,
    const std::string& client_ip,
    int retry_count) {

    if (retry_count >= MAX_RETRY) {
        structured_logger::instance().warn("failover_handler",
            "Failover exhausted for " + failed_tag + " retries=" + std::to_string(retry_count));
        return std::nullopt;
    }

    node_health_checker::instance().mark_unavailable(failed_tag);

    std::vector<std::string> exclude = {failed_tag};
    auto sched = load_balancer::instance().select_fallback_node(client_ip, exclude);

    if (!sched.has_value()) {
        structured_logger::instance().warn("failover_handler",
            "No fallback node available after " + failed_tag + " failed");
        return std::nullopt;
    }

    structured_logger::instance().info("failover_handler",
        "Failover: " + failed_tag + " -> " + sched->node_tag +
        " retry=" + std::to_string(retry_count + 1));

    return failover_result{sched->node_tag, true, retry_count + 1};
}
