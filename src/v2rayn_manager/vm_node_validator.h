#pragma once
#include <string>
#include "v2rayn_manager/vm_node_manager.h"

class vm_node_validator {
public:
    static std::string validate(const vm_node& node, bool is_v5_core = true);

    static std::string validate_tag(const std::string& tag);
    static std::string validate_address(const std::string& address);
    static std::string validate_port(uint16_t port);
    static std::string validate_uuid(const std::string& uuid);
    static std::string validate_alter_id(int alter_id, bool is_v5_core);
    static std::string validate_security(const std::string& security);
    static std::string validate_network(const std::string& network);
    static std::string validate_tls(const std::string& tls);
    static std::string validate_remark(const std::string& remark);
    static std::string validate_ws_config(const vm_node::ws_config& cfg);
    static std::string validate_h2_config(const vm_node::h2_config& cfg);
    static std::string validate_quic_config(const vm_node::quic_config& cfg);
    static std::string validate_kcp_config(const vm_node::kcp_config& cfg);
    static std::string validate_tcp_config(const vm_node::tcp_config& cfg);
    static std::string validate_tls_config(const vm_node::tls_config& cfg);
};
