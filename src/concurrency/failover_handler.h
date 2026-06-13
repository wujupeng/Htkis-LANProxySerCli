#pragma once
#include <string>
#include <vector>
#include <optional>
#include "concurrency/load_balancer.h"

struct failover_result {
    std::string node_tag;
    bool retried{false};
    int retry_count{0};
};

class failover_handler {
public:
    static failover_handler& instance();

    std::optional<failover_result> on_connection_failed(
        const std::string& failed_tag,
        const std::string& client_ip,
        int retry_count);

    static constexpr int MAX_RETRY = 2;

private:
    failover_handler() = default;
};
