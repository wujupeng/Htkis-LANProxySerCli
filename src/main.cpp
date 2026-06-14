#include "config_manager/config_loader.h"
#include "log_monitor/structured_logger.h"
#include "log_monitor/audit_logger.h"
#include "log_monitor/metrics_collector.h"
#include "route_decision/route_engine.h"
#include "user_manager/user_manager.h"
#include "v2rayn_manager/v2rayn_process.h"
#include "v2rayn_manager/v2rayn_config.h"
#include "v2rayn_manager/vm_node_manager.h"
#include "proxy_core/session.h"
#include "web_server/crow_server.h"
#include "web_server/api_router.h"
#include "web_server/ws_handler.h"
#include "web_server/jwt_util.h"
#include "concurrency/load_balancer.h"
#include "concurrency/node_health_checker.h"
#include "concurrency/rate_limiter.h"
#include "concurrency/overload_protector.h"
#include "concurrency/connection_admitter.h"
#include "concurrency/connection_registry.h"
#include "concurrency/failover_handler.h"
#include "concurrency/timeout_scanner.h"
#include "concurrency/concurrency_metrics.h"
#include "concurrency/system_tuner.h"

#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif
#include <asio.hpp>

#include <iostream>
#include <csignal>
#include <thread>
#include <atomic>

static std::atomic<bool> g_running{true};

static void signal_handler(int sig) {
    if (sig == SIGTERM || sig == SIGINT) {
        g_running = false;
    }
}

int main(int argc, char* argv[]) {
    auto& cfg_loader = config_loader::instance();
    cfg_loader.apply_cli_overrides(argc, argv);

    std::string config_path = "config/default_system.json";
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--config" && i + 1 < argc) {
            config_path = argv[++i];
        }
    }

    cfg_loader.load(config_path);
    if (!cfg_loader.validate()) {
        std::cerr << "Invalid configuration, using defaults\n";
    }

    auto& cfg = cfg_loader.config();

    auto& logger = structured_logger::instance();
    logger.init(cfg.log_file_path);

    logger.info("main", "LAN Proxy Gateway starting...");

    audit_logger::instance().init(cfg.log_file_path);

    logger.broadcast_callback([](const std::string& msg) {
        ws_handler::instance().broadcast(msg);
    });

    jwt_util::set_secret(cfg.jwt_secret);

    user_manager::instance().load(cfg.user_data_path);

    route_engine::instance().init(
        cfg.builtin_rules_path,
        cfg.custom_rules_path,
        cfg.geoip_db_path,
        cfg.default_route_action
    );

    v2rayn_process::instance().start(
        cfg.v2rayn_executable_path,
        cfg.v2rayn_config_path,
        cfg.log_file_path + "v2rayn.log"
    );

    vm_node_manager::instance().load_from_config(cfg.v2rayn_config_path);

    overload_protector::instance().init(
        cfg.fd_threshold_pct, cfg.mem_threshold_pct,
        cfg.max_total_connections, cfg.max_per_user);

    rate_limiter::instance().init(
        cfg.max_conn_per_ip_per_sec, cfg.max_auth_fails_per_min, cfg.ban_duration_sec);

    load_balancer::instance().init(
        load_balancer::strategy_from_string(cfg.schedule_strategy),
        cfg.ip_affinity_enabled, cfg.ip_affinity_timeout_sec,
        cfg.ip_affinity_max_records, cfg.node_capacity_threshold);

    node_health_checker::instance().init(
        cfg.health_check_interval_sec, cfg.health_probe_timeout_sec,
        cfg.health_failure_threshold);

    connection_admitter::instance().init(
        cfg.max_total_connections, cfg.max_per_user,
        true, 200, 60);

    timeout_scanner::instance().init(
        30, 15, cfg.idle_timeout_sec, 28800, 5);

    auto tune_results = system_tuner::instance().run_startup_check();
    for (const auto& r : tune_results) {
        if (!r.is_ok) {
            logger.warn("system_tuner", r.item + ": " + r.current_value +
                " (recommended: " + r.recommend_value + ") -> " + r.suggestion);
        }
    }

    server_app::instance().start(cfg.proxy_port, cfg.proxy_thread_count);

    auto& web_srv = crow_server::instance();
    web_srv.init("0.0.0.0", cfg.web_ui_port,
                 cfg.web_ui_https_enabled, cfg.cert_path, cfg.key_path);
    register_api_routes(web_srv.app());

    node_health_checker::instance().start();
    timeout_scanner::instance().start();
    concurrency_metrics::instance().start_history_collection(60);

    timeout_scanner::instance().set_timeout_callback([](const std::string& session_id, const std::string& reason) {
        structured_logger::instance().warn("timeout_scanner",
            "Timeout close: session=" + session_id + " reason=" + reason);
        connection_registry::instance().unregister_connection(session_id);
    });

    std::thread web_thread([&]() {
        web_srv.run();
    });

    struct sigaction sa{};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT, &sa, nullptr);

    asio::io_context signal_io;
    asio::signal_set signals(signal_io, SIGTERM, SIGINT);
    signals.async_wait([](std::error_code, int) {
        g_running = false;
    });
    std::thread signal_thread([&]() { signal_io.run(); });

    logger.info("main", "LAN Proxy Gateway started successfully");
    logger.info("main", "Proxy port: " + std::to_string(cfg.proxy_port));
    logger.info("main", "Web UI port: " + std::to_string(cfg.web_ui_port));
    logger.info("main", "v2rayN SOCKS5 port: " + std::to_string(cfg.v2rayn_local_socks_port));

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        v2rayn_process::instance().check_child_exit();
    }

    logger.info("main", "Shutting down...");

    timeout_scanner::instance().stop();
    concurrency_metrics::instance().stop_history_collection();
    node_health_checker::instance().stop();
    server_app::instance().stop();
    v2rayn_process::instance().stop();
    web_srv.stop();

    if (web_thread.joinable()) web_thread.join();
    signal_io.stop();
    if (signal_thread.joinable()) signal_thread.join();

    logger.info("main", "LAN Proxy Gateway stopped");
    return 0;
}
