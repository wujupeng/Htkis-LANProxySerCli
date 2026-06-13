#pragma once
#include <nlohmann/json.hpp>
#include "v2rayn_manager/vm_node_manager.h"

class vm_config_builder {
public:
    static nlohmann::json build_full_config(
        const std::vector<vm_node>& nodes,
        const nlohmann::json& existing_config,
        const std::string& active_node_tag = ""
    );

    static nlohmann::json build_vmess_outbound(const vm_node& node);

private:
    static nlohmann::json build_stream_settings(const vm_node& node);
    static nlohmann::json build_tcp_settings(const vm_node::tcp_config& cfg);
    static nlohmann::json build_ws_settings(const vm_node::ws_config& cfg);
    static nlohmann::json build_h2_settings(const vm_node::h2_config& cfg);
    static nlohmann::json build_quic_settings(const vm_node::quic_config& cfg);
    static nlohmann::json build_kcp_settings(const vm_node::kcp_config& cfg);
    static nlohmann::json build_tls_settings(const vm_node::tls_config& cfg);
};
