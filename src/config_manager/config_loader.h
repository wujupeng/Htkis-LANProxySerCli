#pragma once

#include <string>
#include <cstdint>
#include <nlohmann/json.hpp>

struct system_config {
    int proxy_port{10800};
    int web_ui_port{8080};
    bool web_ui_https_enabled{true};
    std::string default_route_action{"proxy"};
    int proxy_thread_count{4};
    std::string v2rayn_executable_path{"/home/debian/LanProxySerCli/v2rayn/v2ray"};
    std::string v2rayn_config_path{"/home/debian/LanProxySerCli/v2rayn/config.json"};
    int v2rayn_local_socks_port{10808};
    int v2rayn_local_http_port{10809};
    std::string admin_username{"admin"};
    std::string admin_password_hash;
    std::string jwt_secret;
    std::string log_file_path{"/home/debian/LanProxySerCli/logs/"};
    std::string user_data_path{"/home/debian/LanProxySerCli/data/users.json"};
    std::string geoip_db_path{"/home/debian/LanProxySerCli/data/GeoLite2-Country.mmdb"};
    std::string builtin_rules_path{"/home/debian/LanProxySerCli/data/builtin_rules.json"};
    std::string custom_rules_path{"/home/debian/LanProxySerCli/data/custom_rules.json"};
    std::string cert_path{"/home/debian/LanProxySerCli/config/cert.pem"};
    std::string key_path{"/home/debian/LanProxySerCli/config/key.pem"};

    double fd_threshold_pct{0.9};
    double mem_threshold_pct{0.85};
    int max_total_connections{4000};
    int max_per_user{50};
    int max_conn_per_ip_per_sec{10};
    int max_auth_fails_per_min{5};
    int ban_duration_sec{60};
    std::string schedule_strategy{"round_robin"};
    bool ip_affinity_enabled{true};
    int ip_affinity_timeout_sec{300};
    int ip_affinity_max_records{10000};
    int node_capacity_threshold{1000};
    int health_check_interval_sec{10};
    int health_probe_timeout_sec{5};
    int health_failure_threshold{3};
    int idle_timeout_sec{300};
};

class config_loader {
public:
    static config_loader& instance();

    bool load(const std::string& filepath);
    bool save();
    bool validate() const;
    void apply_cli_overrides(int argc, char* argv[]);

    system_config& config();
    const system_config& config() const;

private:
    config_loader() = default;

    system_config m_config;
    std::string m_config_path;
};
