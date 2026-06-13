#include "config_manager/config_loader.h"
#include "user_manager/password_hasher.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <random>
#include <algorithm>

config_loader& config_loader::instance() {
    static config_loader inst;
    return inst;
}

bool config_loader::load(const std::string& filepath) {
    m_config_path = filepath;
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Config file not found: " << filepath << ", using defaults\n";
        if (m_config.jwt_secret.empty()) {
            std::random_device rd;
            std::uniform_int_distribution<uint32_t> dist;
            m_config.jwt_secret = "";
            for (int i = 0; i < 8; ++i) {
                char buf[9];
                snprintf(buf, sizeof(buf), "%08x", dist(rd));
                m_config.jwt_secret += buf;
            }
        }
        if (m_config.admin_password_hash.empty() ||
            m_config.admin_password_hash.find("placeholder") != std::string::npos) {
            std::string default_password = "changeme";
            m_config.admin_password_hash = password_hasher::hash(default_password);
            std::cerr << "WARNING: Default password set to '" << default_password
                      << "'. Please change it immediately via Web UI or config file.\n";
        }
        return false;
    }

    try {
        nlohmann::json j;
        file >> j;

        if (j.contains("proxy_port")) m_config.proxy_port = j["proxy_port"];
        if (j.contains("web_ui_port")) m_config.web_ui_port = j["web_ui_port"];
        if (j.contains("web_ui_https_enabled")) m_config.web_ui_https_enabled = j["web_ui_https_enabled"];
        if (j.contains("default_route_action")) m_config.default_route_action = j["default_route_action"];
        if (j.contains("proxy_thread_count")) m_config.proxy_thread_count = j["proxy_thread_count"];
        if (j.contains("v2rayn_executable_path")) m_config.v2rayn_executable_path = j["v2rayn_executable_path"];
        if (j.contains("v2rayn_config_path")) m_config.v2rayn_config_path = j["v2rayn_config_path"];
        if (j.contains("v2rayn_local_socks_port")) m_config.v2rayn_local_socks_port = j["v2rayn_local_socks_port"];
        if (j.contains("v2rayn_local_http_port")) m_config.v2rayn_local_http_port = j["v2rayn_local_http_port"];
        if (j.contains("admin_username")) m_config.admin_username = j["admin_username"];
        if (j.contains("admin_password_hash")) m_config.admin_password_hash = j["admin_password_hash"];
        if (j.contains("jwt_secret")) m_config.jwt_secret = j["jwt_secret"];
        if (j.contains("log_file_path")) m_config.log_file_path = j["log_file_path"];
        if (j.contains("user_data_path")) m_config.user_data_path = j["user_data_path"];
        if (j.contains("geoip_db_path")) m_config.geoip_db_path = j["geoip_db_path"];
        if (j.contains("builtin_rules_path")) m_config.builtin_rules_path = j["builtin_rules_path"];
        if (j.contains("custom_rules_path")) m_config.custom_rules_path = j["custom_rules_path"];
        if (j.contains("cert_path")) m_config.cert_path = j["cert_path"];
        if (j.contains("key_path")) m_config.key_path = j["key_path"];

        if (j.contains("fd_threshold_pct")) m_config.fd_threshold_pct = j["fd_threshold_pct"];
        if (j.contains("mem_threshold_pct")) m_config.mem_threshold_pct = j["mem_threshold_pct"];
        if (j.contains("max_total_connections")) m_config.max_total_connections = j["max_total_connections"];
        if (j.contains("max_per_user")) m_config.max_per_user = j["max_per_user"];
        if (j.contains("max_conn_per_ip_per_sec")) m_config.max_conn_per_ip_per_sec = j["max_conn_per_ip_per_sec"];
        if (j.contains("max_auth_fails_per_min")) m_config.max_auth_fails_per_min = j["max_auth_fails_per_min"];
        if (j.contains("ban_duration_sec")) m_config.ban_duration_sec = j["ban_duration_sec"];
        if (j.contains("schedule_strategy")) m_config.schedule_strategy = j["schedule_strategy"];
        if (j.contains("ip_affinity_enabled")) m_config.ip_affinity_enabled = j["ip_affinity_enabled"];
        if (j.contains("ip_affinity_timeout_sec")) m_config.ip_affinity_timeout_sec = j["ip_affinity_timeout_sec"];
        if (j.contains("ip_affinity_max_records")) m_config.ip_affinity_max_records = j["ip_affinity_max_records"];
        if (j.contains("node_capacity_threshold")) m_config.node_capacity_threshold = j["node_capacity_threshold"];
        if (j.contains("health_check_interval_sec")) m_config.health_check_interval_sec = j["health_check_interval_sec"];
        if (j.contains("health_probe_timeout_sec")) m_config.health_probe_timeout_sec = j["health_probe_timeout_sec"];
        if (j.contains("health_failure_threshold")) m_config.health_failure_threshold = j["health_failure_threshold"];
        if (j.contains("idle_timeout_sec")) m_config.idle_timeout_sec = j["idle_timeout_sec"];

        if (m_config.jwt_secret.empty()) {
            std::random_device rd;
            std::uniform_int_distribution<uint32_t> dist;
            m_config.jwt_secret = "";
            for (int i = 0; i < 8; ++i) {
                char buf[9];
                snprintf(buf, sizeof(buf), "%08x", dist(rd));
                m_config.jwt_secret += buf;
            }
        }

        return true;
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "Config parse error: " << e.what() << "\n";
        return false;
    }
}

bool config_loader::save() {
    if (m_config_path.empty()) return false;

    nlohmann::json j;
    j["proxy_port"] = m_config.proxy_port;
    j["web_ui_port"] = m_config.web_ui_port;
    j["web_ui_https_enabled"] = m_config.web_ui_https_enabled;
    j["default_route_action"] = m_config.default_route_action;
    j["proxy_thread_count"] = m_config.proxy_thread_count;
    j["v2rayn_executable_path"] = m_config.v2rayn_executable_path;
    j["v2rayn_config_path"] = m_config.v2rayn_config_path;
    j["v2rayn_local_socks_port"] = m_config.v2rayn_local_socks_port;
    j["v2rayn_local_http_port"] = m_config.v2rayn_local_http_port;
    j["admin_username"] = m_config.admin_username;
    j["admin_password_hash"] = m_config.admin_password_hash;
    j["jwt_secret"] = m_config.jwt_secret;
    j["log_file_path"] = m_config.log_file_path;
    j["user_data_path"] = m_config.user_data_path;
    j["geoip_db_path"] = m_config.geoip_db_path;
    j["builtin_rules_path"] = m_config.builtin_rules_path;
    j["custom_rules_path"] = m_config.custom_rules_path;
    j["cert_path"] = m_config.cert_path;
    j["key_path"] = m_config.key_path;

    j["fd_threshold_pct"] = m_config.fd_threshold_pct;
    j["mem_threshold_pct"] = m_config.mem_threshold_pct;
    j["max_total_connections"] = m_config.max_total_connections;
    j["max_per_user"] = m_config.max_per_user;
    j["max_conn_per_ip_per_sec"] = m_config.max_conn_per_ip_per_sec;
    j["max_auth_fails_per_min"] = m_config.max_auth_fails_per_min;
    j["ban_duration_sec"] = m_config.ban_duration_sec;
    j["schedule_strategy"] = m_config.schedule_strategy;
    j["ip_affinity_enabled"] = m_config.ip_affinity_enabled;
    j["ip_affinity_timeout_sec"] = m_config.ip_affinity_timeout_sec;
    j["ip_affinity_max_records"] = m_config.ip_affinity_max_records;
    j["node_capacity_threshold"] = m_config.node_capacity_threshold;
    j["health_check_interval_sec"] = m_config.health_check_interval_sec;
    j["health_probe_timeout_sec"] = m_config.health_probe_timeout_sec;
    j["health_failure_threshold"] = m_config.health_failure_threshold;
    j["idle_timeout_sec"] = m_config.idle_timeout_sec;

    std::string tmp_path = m_config_path + ".tmp";
    {
        std::ofstream file(tmp_path);
        if (!file.is_open()) return false;
        file << j.dump(4);
    }
    if (std::rename(tmp_path.c_str(), m_config_path.c_str()) != 0) {
        return false;
    }
    return true;
}

bool config_loader::validate() const {
    if (m_config.proxy_port < 1 || m_config.proxy_port > 65535) return false;
    if (m_config.web_ui_port < 1 || m_config.web_ui_port > 65535) return false;
    if (m_config.v2rayn_local_socks_port < 1 || m_config.v2rayn_local_socks_port > 65535) return false;
    if (m_config.v2rayn_local_http_port < 1 || m_config.v2rayn_local_http_port > 65535) return false;
    if (m_config.proxy_thread_count < 1) return false;
    if (m_config.admin_username.empty()) return false;
    if (m_config.jwt_secret.empty()) return false;
    if (m_config.admin_password_hash.empty()) return false;
    if (m_config.admin_password_hash.find("placeholder") != std::string::npos) return false;
    if (m_config.default_route_action != "direct" && m_config.default_route_action != "proxy") return false;
    return true;
}

void config_loader::apply_cli_overrides(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            load(argv[++i]);
        } else if (arg == "--proxy-port" && i + 1 < argc) {
            m_config.proxy_port = std::stoi(argv[++i]);
        } else if (arg == "--web-port" && i + 1 < argc) {
            m_config.web_ui_port = std::stoi(argv[++i]);
        } else if (arg == "--help") {
            std::cout << "Usage: lan_proxy_gateway [OPTIONS]\n"
                      << "  --config <path>       Config file path\n"
                      << "  --proxy-port <port>   Proxy listen port (default: 10800)\n"
                      << "  --web-port <port>     Web UI listen port (default: 8080)\n"
                      << "  --help                Show this help\n";
            exit(0);
        }
    }
}

system_config& config_loader::config() { return m_config; }
const system_config& config_loader::config() const { return m_config; }
