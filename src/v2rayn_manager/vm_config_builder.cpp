#include "v2rayn_manager/vm_config_builder.h"
#include "v2rayn_manager/vm_node_manager.h"

nlohmann::json vm_config_builder::build_full_config(
    const std::vector<vm_node>& nodes,
    const nlohmann::json& existing_config,
    const std::string& active_node_tag)
{
    nlohmann::json config;
    if (existing_config.is_object()) {
        config = existing_config;
    } else {
        config = nlohmann::json::object();
    }

    nlohmann::json outbounds = nlohmann::json::array();
    for (const auto& node : nodes) {
        outbounds.push_back(build_vmess_outbound(node));
    }
    outbounds.push_back({{"protocol", "freedom"}, {"tag", "direct"}});
    outbounds.push_back({{"protocol", "blackhole"}, {"tag", "block"}});

    config["outbounds"] = outbounds;

    std::string active_tag = active_node_tag;
    if (active_tag.empty() && !nodes.empty()) {
        active_tag = nodes[0].tag;
    }

    if (!config.contains("routing") || !config["routing"].is_object()) {
        config["routing"] = nlohmann::json::object();
    }

    if (!active_tag.empty()) {
        nlohmann::json new_rules = nlohmann::json::array();
        bool found_default = false;

        if (config["routing"].contains("rules") && config["routing"]["rules"].is_array()) {
            for (auto& rule : config["routing"]["rules"]) {
                std::string ob_tag = rule.value("outboundTag", "");
                if (ob_tag.find("vmess-") == 0) {
                    rule["outboundTag"] = active_tag;
                    found_default = true;
                }
                new_rules.push_back(rule);
            }
        }

        if (!found_default) {
            new_rules.insert(new_rules.begin(), {
                {"type", "field"},
                {"outboundTag", active_tag},
                {"network", "tcp,udp"}
            });
        }

        config["routing"]["rules"] = new_rules;
    }

    if (!config.contains("inbounds") || !config["inbounds"].is_array() || config["inbounds"].empty()) {
        config["inbounds"] = nlohmann::json::array();
        config["inbounds"].push_back({
            {"tag", "socks-in"},
            {"port", 10808},
            {"listen", "127.0.0.1"},
            {"protocol", "socks"},
            {"settings", {{"auth", "noauth"}, {"udp", true}}},
            {"sniffing", {{"enabled", true}, {"destOverride", {"http", "tls"}}}}
        });
        config["inbounds"].push_back({
            {"tag", "http-in"},
            {"port", 10809},
            {"listen", "127.0.0.1"},
            {"protocol", "http"},
            {"settings", nlohmann::json::object()}
        });
    }

    if (!config.contains("log")) {
        config["log"] = {{"loglevel", "warning"}};
    }

    return config;
}

nlohmann::json vm_config_builder::build_vmess_outbound(const vm_node& node) {
    nlohmann::json outbound;
    outbound["tag"] = node.tag;
    outbound["protocol"] = "vmess";

    nlohmann::json user;
    user["id"] = node.user_id;
    user["alterId"] = node.alter_id;
    user["security"] = node.security;

    nlohmann::json vnext_entry;
    vnext_entry["address"] = node.address;
    vnext_entry["port"] = node.port;
    vnext_entry["users"] = nlohmann::json::array({user});

    outbound["settings"] = {{"vnext", nlohmann::json::array({vnext_entry})}};
    outbound["streamSettings"] = build_stream_settings(node);

    return outbound;
}

nlohmann::json vm_config_builder::build_stream_settings(const vm_node& node) {
    nlohmann::json ss;
    ss["network"] = node.network;
    ss["security"] = (node.tls == "tls") ? "tls" : "none";

    if (node.network == "tcp") {
        ss["tcpSettings"] = build_tcp_settings(node.tcp_cfg);
    } else if (node.network == "ws") {
        ss["wsSettings"] = build_ws_settings(node.ws_cfg);
    } else if (node.network == "h2") {
        ss["httpSettings"] = build_h2_settings(node.h2_cfg);
    } else if (node.network == "quic") {
        ss["quicSettings"] = build_quic_settings(node.quic_cfg);
    } else if (node.network == "kcp") {
        ss["kcpSettings"] = build_kcp_settings(node.kcp_cfg);
    }

    if (node.tls == "tls") {
        ss["tlsSettings"] = build_tls_settings(node.tls_cfg);
    }

    return ss;
}

nlohmann::json vm_config_builder::build_tcp_settings(const vm_node::tcp_config& cfg) {
    nlohmann::json j;
    j["header"] = {{"type", cfg.header_type}};
    return j;
}

nlohmann::json vm_config_builder::build_ws_settings(const vm_node::ws_config& cfg) {
    nlohmann::json j;
    j["path"] = cfg.path;
    if (!cfg.host.empty()) {
        j["headers"] = {{"Host", cfg.host}};
    }
    return j;
}

nlohmann::json vm_config_builder::build_h2_settings(const vm_node::h2_config& cfg) {
    nlohmann::json j;
    j["path"] = cfg.path;
    if (!cfg.host.empty()) {
        j["host"] = cfg.host;
    }
    return j;
}

nlohmann::json vm_config_builder::build_quic_settings(const vm_node::quic_config& cfg) {
    nlohmann::json j;
    j["security"] = cfg.security;
    j["key"] = cfg.key;
    j["header"] = {{"type", cfg.header_type}};
    return j;
}

nlohmann::json vm_config_builder::build_kcp_settings(const vm_node::kcp_config& cfg) {
    nlohmann::json j;
    j["mtu"] = cfg.mtu;
    j["tti"] = cfg.tti;
    j["uplinkCapacity"] = cfg.uplink_capacity;
    j["downlinkCapacity"] = cfg.downlink_capacity;
    j["congestion"] = cfg.congestion;
    j["header"] = {{"type", cfg.header_type}};
    return j;
}

nlohmann::json vm_config_builder::build_tls_settings(const vm_node::tls_config& cfg) {
    nlohmann::json j;
    j["allowInsecure"] = cfg.allow_insecure;
    if (!cfg.server_name.empty()) {
        j["serverName"] = cfg.server_name;
    }
    return j;
}
