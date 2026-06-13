#include "v2rayn_manager/vm_node_validator.h"
#include <regex>

std::string vm_node_validator::validate(const vm_node& node, bool is_v5_core) {
    std::string err;
    err = validate_tag(node.tag); if (!err.empty()) return err;
    err = validate_address(node.address); if (!err.empty()) return err;
    err = validate_port(node.port); if (!err.empty()) return err;
    err = validate_uuid(node.user_id); if (!err.empty()) return err;
    err = validate_alter_id(node.alter_id, is_v5_core); if (!err.empty()) return err;
    err = validate_security(node.security); if (!err.empty()) return err;
    err = validate_network(node.network); if (!err.empty()) return err;
    err = validate_tls(node.tls); if (!err.empty()) return err;
    err = validate_remark(node.remark); if (!err.empty()) return err;

    if (node.network == "tcp") {
        err = validate_tcp_config(node.tcp_cfg); if (!err.empty()) return err;
    } else if (node.network == "ws") {
        err = validate_ws_config(node.ws_cfg); if (!err.empty()) return err;
    } else if (node.network == "h2") {
        err = validate_h2_config(node.h2_cfg); if (!err.empty()) return err;
    } else if (node.network == "quic") {
        err = validate_quic_config(node.quic_cfg); if (!err.empty()) return err;
    } else if (node.network == "kcp") {
        err = validate_kcp_config(node.kcp_cfg); if (!err.empty()) return err;
    }

    if (node.tls == "tls") {
        err = validate_tls_config(node.tls_cfg); if (!err.empty()) return err;
    }
    return "";
}

std::string vm_node_validator::validate_tag(const std::string& tag) {
    if (tag.empty()) return "节点标签不能为空";
    if (!std::regex_match(tag, std::regex("^[a-zA-Z0-9_-]+$")))
        return "节点标签只能包含字母、数字、连字符和下划线";
    return "";
}

std::string vm_node_validator::validate_address(const std::string& address) {
    if (address.empty()) return "服务器地址不能为空";
    std::regex ipv4(R"(^(\d{1,3}\.){3}\d{1,3}$)");
    std::regex ipv6(R"(^\[?([0-9a-fA-F]{0,4}:){2,7}[0-9a-fA-F]{0,4}\]?$)");
    std::regex domain(R"(^[a-zA-Z0-9]([a-zA-Z0-9-]*[a-zA-Z0-9])?(\.[a-zA-Z0-9]([a-zA-Z0-9-]*[a-zA-Z0-9])?)*\.[a-zA-Z]{2,}$)");
    if (!std::regex_match(address, ipv4) &&
        !std::regex_match(address, ipv6) &&
        !std::regex_match(address, domain))
        return "服务器地址格式无效（需为IPv4/IPv6/域名）";
    return "";
}

std::string vm_node_validator::validate_port(uint16_t port) {
    if (port == 0) return "端口不能为0";
    return "";
}

std::string vm_node_validator::validate_uuid(const std::string& uuid) {
    if (uuid.empty()) return "UUID不能为空";
    std::regex uuid_re(R"(^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$)");
    if (!std::regex_match(uuid, uuid_re)) return "UUID格式无效（需为8-4-4-4-12格式）";
    return "";
}

std::string vm_node_validator::validate_alter_id(int alter_id, bool is_v5_core) {
    if (is_v5_core && alter_id != 0) return "v2ray-core 5.x的alterId必须为0";
    if (alter_id < 0 || alter_id > 65535) return "alterId范围应为0-65535";
    return "";
}

std::string vm_node_validator::validate_security(const std::string& security) {
    if (security != "auto" && security != "aes-128-gcm" &&
        security != "chacha20-poly1305" && security != "none" && security != "zero")
        return "加密方式无效（可选：auto/aes-128-gcm/chacha20-poly1305/none/zero）";
    return "";
}

std::string vm_node_validator::validate_network(const std::string& network) {
    if (network != "tcp" && network != "ws" && network != "h2" &&
        network != "quic" && network != "kcp")
        return "传输协议无效（可选：tcp/ws/h2/quic/kcp）";
    return "";
}

std::string vm_node_validator::validate_tls(const std::string& tls) {
    if (tls != "none" && tls != "tls")
        return "TLS设置无效（可选：none/tls）";
    return "";
}

std::string vm_node_validator::validate_remark(const std::string& remark) {
    if (remark.size() > 128) return "备注长度不能超过128字符";
    return "";
}

std::string vm_node_validator::validate_ws_config(const vm_node::ws_config& cfg) {
    if (cfg.path.empty()) return "WebSocket路径不能为空";
    return "";
}

std::string vm_node_validator::validate_h2_config(const vm_node::h2_config& cfg) {
    if (cfg.path.empty()) return "HTTP/2路径不能为空";
    return "";
}

std::string vm_node_validator::validate_quic_config(const vm_node::quic_config& cfg) {
    if (cfg.security != "none" && cfg.security != "aes-128-gcm" && cfg.security != "chacha20-poly1305")
        return "QUIC加密方式无效";
    return "";
}

std::string vm_node_validator::validate_kcp_config(const vm_node::kcp_config& cfg) {
    if (cfg.mtu < 576 || cfg.mtu > 1500) return "mKCP的MTU范围应为576-1500";
    if (cfg.tti < 10 || cfg.tti > 100) return "mKCP的TTI范围应为10-100";
    if (cfg.uplink_capacity < 1 || cfg.uplink_capacity > 65535) return "mKCP上行容量范围应为1-65535";
    if (cfg.downlink_capacity < 1 || cfg.downlink_capacity > 65535) return "mKCP下行容量范围应为1-65535";
    return "";
}

std::string vm_node_validator::validate_tcp_config(const vm_node::tcp_config& cfg) {
    if (cfg.header_type != "none" && cfg.header_type != "http")
        return "TCP头部伪装类型无效（可选：none/http）";
    return "";
}

std::string vm_node_validator::validate_tls_config(const vm_node::tls_config& cfg) {
    return "";
}
