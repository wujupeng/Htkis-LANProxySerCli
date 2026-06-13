#include "v2rayn_manager/vm_node_manager.h"
#include "v2rayn_manager/vm_config_builder.h"
#include "v2rayn_manager/vm_link_codec.h"
#include "v2rayn_manager/vm_node_validator.h"
#include "v2rayn_manager/v2rayn_config.h"
#include "v2rayn_manager/v2rayn_process.h"
#include "log_monitor/structured_logger.h"
#include "log_monitor/audit_logger.h"
#include "concurrency/node_health_checker.h"
#include "concurrency/load_balancer.h"
#include <fstream>
#include <algorithm>

vm_node_manager& vm_node_manager::instance() {
    static vm_node_manager inst;
    return inst;
}

bool vm_node_manager::load_from_config(const std::string& config_path) {
    m_config_path = config_path;
    auto config = v2rayn_config::load(config_path);
    if (!config.is_object()) {
        structured_logger::instance().info("vm_node_manager", "v2ray配置文件不存在或为空，节点列表为空");
        std::unique_lock lock(m_nodes_mutex);
        m_nodes.clear();
        return true;
    }

    std::vector<vm_node> nodes;
    if (config.contains("outbounds") && config["outbounds"].is_array()) {
        for (const auto& ob : config["outbounds"]) {
            if (ob.value("protocol", "") == "vmess") {
                auto node = parse_outbound(ob);
                if (node.has_value()) {
                    nodes.push_back(std::move(*node));
                } else {
                    structured_logger::instance().warn("vm_node_manager",
                        "跳过无法解析的VMess出站: " + ob.value("tag", "?"));
                }
            }
        }
    }

    std::string remarks_path = config_path.substr(0, config_path.find_last_of('/')) + "/../data/vmess_remarks.json";
    nlohmann::json remarks;
    std::ifstream rf(remarks_path);
    if (rf.is_open()) {
        try { rf >> remarks; } catch (...) {}
    }

    for (auto& node : nodes) {
        if (remarks.contains("remarks") && remarks["remarks"].contains(node.tag)) {
            node.remark = remarks["remarks"][node.tag].get<std::string>();
        }
    }

    {
        std::unique_lock lock(m_nodes_mutex);
        m_nodes = std::move(nodes);
    }

    if (config.contains("routing") && config["routing"].is_object()) {
        auto& routing = config["routing"];
        if (routing.contains("rules") && routing["rules"].is_array()) {
            for (const auto& rule : routing["rules"]) {
                std::string tag = rule.value("outboundTag", "");
                if (tag.find("vmess-") == 0) {
                    m_active_node_tag = tag;
                    break;
                }
            }
        }
    }
    if (m_active_node_tag.empty() && !m_nodes.empty()) {
        m_active_node_tag = m_nodes[0].tag;
    }

    structured_logger::instance().info("vm_node_manager",
        "从配置加载了 " + std::to_string(m_nodes.size()) + " 个VMess节点");
    return true;
}

std::optional<vm_node> vm_node_manager::parse_outbound(const nlohmann::json& outbound) const {
    vm_node node;
    node.tag = outbound.value("tag", "");
    if (node.tag.empty()) return std::nullopt;

    if (!outbound.contains("settings") || !outbound["settings"].contains("vnext") ||
        !outbound["settings"]["vnext"].is_array() || outbound["settings"]["vnext"].empty())
        return std::nullopt;

    const auto& vnext = outbound["settings"]["vnext"][0];
    node.address = vnext.value("address", "");
    node.port = vnext.value("port", 443);

    if (vnext.contains("users") && vnext["users"].is_array() && !vnext["users"].empty()) {
        const auto& user = vnext["users"][0];
        node.user_id = user.value("id", "");
        node.alter_id = user.value("alterId", 0);
        node.security = user.value("security", "auto");
    }

    if (outbound.contains("streamSettings")) {
        const auto& ss = outbound["streamSettings"];
        node.network = ss.value("network", "tcp");
        std::string sec = ss.value("security", "none");
        node.tls = (sec == "tls") ? "tls" : "none";

        if (node.network == "tcp" && ss.contains("tcpSettings")) {
            const auto& tcp = ss["tcpSettings"];
            if (tcp.contains("header") && tcp["header"].contains("type"))
                node.tcp_cfg.header_type = tcp["header"]["type"].get<std::string>();
        } else if (node.network == "ws" && ss.contains("wsSettings")) {
            const auto& ws = ss["wsSettings"];
            node.ws_cfg.path = ws.value("path", "/");
            if (ws.contains("headers") && ws["headers"].contains("Host"))
                node.ws_cfg.host = ws["headers"]["Host"].get<std::string>();
        } else if (node.network == "h2" && ss.contains("httpSettings")) {
            const auto& h2 = ss["httpSettings"];
            node.h2_cfg.path = h2.value("path", "/");
            if (h2.contains("host") && h2["host"].is_array()) {
                for (const auto& h : h2["host"])
                    node.h2_cfg.host.push_back(h.get<std::string>());
            }
        } else if (node.network == "quic" && ss.contains("quicSettings")) {
            const auto& q = ss["quicSettings"];
            node.quic_cfg.security = q.value("security", "none");
            node.quic_cfg.key = q.value("key", "");
            if (q.contains("header") && q["header"].contains("type"))
                node.quic_cfg.header_type = q["header"]["type"].get<std::string>();
        } else if (node.network == "kcp" && ss.contains("kcpSettings")) {
            const auto& k = ss["kcpSettings"];
            node.kcp_cfg.mtu = k.value("mtu", 1350);
            node.kcp_cfg.tti = k.value("tti", 20);
            node.kcp_cfg.uplink_capacity = k.value("uplinkCapacity", 5);
            node.kcp_cfg.downlink_capacity = k.value("downlinkCapacity", 20);
            node.kcp_cfg.congestion = k.value("congestion", false);
            if (k.contains("header") && k["header"].contains("type"))
                node.kcp_cfg.header_type = k["header"]["type"].get<std::string>();
        }

        if (node.tls == "tls" && ss.contains("tlsSettings")) {
            const auto& tls_s = ss["tlsSettings"];
            node.tls_cfg.allow_insecure = tls_s.value("allowInsecure", false);
            node.tls_cfg.server_name = tls_s.value("serverName", "");
        }
    }

    return node;
}

std::vector<vm_node> vm_node_manager::list_nodes(bool desensitize_id) const {
    std::shared_lock lock(m_nodes_mutex);
    auto nodes = m_nodes;
    if (desensitize_id) {
        for (auto& n : nodes) {
            n.user_id = desensitize_uuid(n.user_id);
        }
    }
    return nodes;
}

std::optional<vm_node> vm_node_manager::get_node(const std::string& tag) const {
    std::shared_lock lock(m_nodes_mutex);
    for (const auto& n : m_nodes) {
        if (n.tag == tag) return n;
    }
    return std::nullopt;
}

bool vm_node_manager::add_node(const vm_node& node, std::string& error_msg) {
    auto err = vm_node_validator::validate(node, m_is_v5_core);
    if (!err.empty()) { error_msg = err; return false; }
    if (!is_tag_unique(node.tag)) { error_msg = "节点标签已存在: " + node.tag; return false; }

    {
        std::unique_lock lock(m_nodes_mutex);
        m_nodes.push_back(node);
    }

    if (!save_to_config(error_msg)) {
        std::unique_lock lock(m_nodes_mutex);
        m_nodes.pop_back();
        return false;
    }

    audit_logger::instance().log("admin", "add_vmess_node", node.tag);

    node_health_checker::instance().check_now(node.tag);

    return true;
}

bool vm_node_manager::update_node(const std::string& old_tag, const vm_node& node, std::string& error_msg) {
    auto err = vm_node_validator::validate(node, m_is_v5_core);
    if (!err.empty()) { error_msg = err; return false; }
    if (!is_tag_unique(node.tag, old_tag)) { error_msg = "节点标签已存在: " + node.tag; return false; }

    vm_node backup;
    {
        std::unique_lock lock(m_nodes_mutex);
        auto it = std::find_if(m_nodes.begin(), m_nodes.end(),
            [&](const vm_node& n) { return n.tag == old_tag; });
        if (it == m_nodes.end()) { error_msg = "节点不存在: " + old_tag; return false; }
        backup = *it;
        *it = node;
    }

    if (!save_to_config(error_msg)) {
        std::unique_lock lock(m_nodes_mutex);
        auto it = std::find_if(m_nodes.begin(), m_nodes.end(),
            [&](const vm_node& n) { return n.tag == node.tag; });
        if (it != m_nodes.end()) *it = backup;
        return false;
    }

    audit_logger::instance().log("admin", "update_vmess_node", old_tag);
    return true;
}

bool vm_node_manager::remove_node(const std::string& tag, std::string& error_msg) {
    {
        std::shared_lock lock(m_nodes_mutex);
        if (m_nodes.size() <= 1) {
            error_msg = "至少保留一个VMess节点";
            return false;
        }
    }

    vm_node backup;
    {
        std::unique_lock lock(m_nodes_mutex);
        auto it = std::find_if(m_nodes.begin(), m_nodes.end(),
            [&](const vm_node& n) { return n.tag == tag; });
        if (it == m_nodes.end()) { error_msg = "节点不存在: " + tag; return false; }
        backup = *it;
        m_nodes.erase(it);
    }

    if (!save_to_config(error_msg)) {
        std::unique_lock lock(m_nodes_mutex);
        m_nodes.push_back(backup);
        return false;
    }

    audit_logger::instance().log("admin", "remove_vmess_node", tag);

    load_balancer::instance().dec_node_connections(tag);
    node_health_checker::instance().mark_unavailable(tag);

    return true;
}

bool vm_node_manager::reorder_nodes(const std::vector<std::string>& ordered_tags) {
    std::unique_lock lock(m_nodes_mutex);
    std::vector<vm_node> reordered;
    for (const auto& tag : ordered_tags) {
        auto it = std::find_if(m_nodes.begin(), m_nodes.end(),
            [&](const vm_node& n) { return n.tag == tag; });
        if (it != m_nodes.end()) {
            reordered.push_back(std::move(*it));
        }
    }
    for (auto& n : m_nodes) {
        if (std::find_if(reordered.begin(), reordered.end(),
            [&](const vm_node& r) { return r.tag == n.tag; }) == reordered.end()) {
            reordered.push_back(std::move(n));
        }
    }
    m_nodes = std::move(reordered);
    return true;
}

bool vm_node_manager::set_active_node(const std::string& tag, std::string& error_msg) {
    std::shared_lock lock(m_nodes_mutex);
    bool found = false;
    for (const auto& n : m_nodes) {
        if (n.tag == tag) { found = true; break; }
    }
    if (!found) {
        error_msg = "节点不存在: " + tag;
        return false;
    }
    m_active_node_tag = tag;
    lock.unlock();

    structured_logger::instance().info("vm_node_manager", "活动节点已设置为: " + tag);
    audit_logger::instance().log("admin", "set_active_node", tag);
    return true;
}

std::string vm_node_manager::get_active_node() const {
    std::shared_lock lock(m_nodes_mutex);
    return m_active_node_tag;
}

bool vm_node_manager::apply_config(std::string& error_msg) {
    if (!save_to_config(error_msg)) return false;

    v2rayn_process::instance().restart();
    structured_logger::instance().info("vm_node_manager", "配置已应用，v2rayN正在重启");
    return true;
}

std::optional<vm_node> vm_node_manager::import_from_link(const std::string& vmess_link, std::string& error_msg) {
    auto node = vm_link_codec::decode(vmess_link, error_msg);
    if (!node.has_value()) return std::nullopt;

    node->tag = generate_auto_tag();

    auto err = vm_node_validator::validate(*node, m_is_v5_core);
    if (!err.empty()) { error_msg = err; return std::nullopt; }

    {
        std::unique_lock lock(m_nodes_mutex);
        m_nodes.push_back(*node);
    }

    if (!save_to_config(error_msg)) {
        std::unique_lock lock(m_nodes_mutex);
        if (!m_nodes.empty() && m_nodes.back().tag == node->tag) m_nodes.pop_back();
        return std::nullopt;
    }

    audit_logger::instance().log("admin", "import_vmess_link", node->tag);
    return node;
}

std::string vm_node_manager::export_to_link(const vm_node& node) const {
    return vm_link_codec::encode(node);
}

std::string vm_node_manager::generate_auto_tag() const {
    std::shared_lock lock(m_nodes_mutex);
    int n = 1;
    while (true) {
        std::string tag = "vmess-" + std::to_string(n);
        if (std::find_if(m_nodes.begin(), m_nodes.end(),
            [&](const vm_node& nd) { return nd.tag == tag; }) == m_nodes.end()) {
            return tag;
        }
        n++;
    }
}

std::string vm_node_manager::desensitize_uuid(const std::string& uuid) {
    if (uuid.size() < 8) return "***";
    return uuid.substr(0, 4) + "****" + uuid.substr(uuid.size() - 4);
}

bool vm_node_manager::save_to_config(std::string& error_msg) {
    std::shared_lock lock(m_nodes_mutex);

    auto existing = v2rayn_config::load(m_config_path);
    auto config = vm_config_builder::build_full_config(m_nodes, existing, m_active_node_tag);

    if (!v2rayn_config::validate(config)) {
        error_msg = "生成的v2ray配置验证失败";
        return false;
    }

    if (!v2rayn_config::save(m_config_path, config)) {
        error_msg = "v2ray配置文件写入失败";
        return false;
    }

    std::string data_dir = m_config_path.substr(0, m_config_path.find_last_of('/')) + "/../data";
    nlohmann::json remarks;
    remarks["version"] = 1;
    remarks["remarks"] = nlohmann::json::object();
    for (const auto& n : m_nodes) {
        if (!n.remark.empty()) {
            remarks["remarks"][n.tag] = n.remark;
        }
    }

    std::string remarks_path = data_dir + "/vmess_remarks.json";
    std::ofstream rf(remarks_path);
    if (rf.is_open()) {
        rf << remarks.dump(4);
    }

    return true;
}

bool vm_node_manager::is_tag_unique(const std::string& tag, const std::string& exclude_tag) const {
    for (const auto& n : m_nodes) {
        if (n.tag == tag && n.tag != exclude_tag) return false;
    }
    return true;
}

nlohmann::json vm_node_manager::node_to_json(const vm_node& node, bool desensitize_id) const {
    nlohmann::json j;
    j["tag"] = node.tag;
    j["address"] = node.address;
    j["port"] = node.port;
    j["user_id"] = desensitize_id ? desensitize_uuid(node.user_id) : node.user_id;
    j["alter_id"] = node.alter_id;
    j["security"] = node.security;
    j["network"] = node.network;
    j["tls"] = node.tls;
    j["remark"] = node.remark;

    j["tcp_config"] = {{"header_type", node.tcp_cfg.header_type}};
    j["ws_config"] = {{"path", node.ws_cfg.path}, {"host", node.ws_cfg.host}};
    j["h2_config"] = {{"path", node.h2_cfg.path}, {"host", node.h2_cfg.host}};
    j["quic_config"] = {{"security", node.quic_cfg.security}, {"key", node.quic_cfg.key}, {"header_type", node.quic_cfg.header_type}};
    j["kcp_config"] = {{"mtu", node.kcp_cfg.mtu}, {"tti", node.kcp_cfg.tti},
                       {"uplink_capacity", node.kcp_cfg.uplink_capacity}, {"downlink_capacity", node.kcp_cfg.downlink_capacity},
                       {"congestion", node.kcp_cfg.congestion}, {"header_type", node.kcp_cfg.header_type}};
    j["tls_config"] = {{"allow_insecure", node.tls_cfg.allow_insecure}, {"server_name", node.tls_cfg.server_name}};
    return j;
}

vm_node vm_node_manager::json_to_node(const nlohmann::json& j) const {
    vm_node node;
    node.tag = j.value("tag", "");
    node.address = j.value("address", "");
    node.port = j.value("port", 443);
    node.user_id = j.value("user_id", "");
    node.alter_id = j.value("alter_id", 0);
    node.security = j.value("security", "auto");
    node.network = j.value("network", "tcp");
    node.tls = j.value("tls", "none");
    node.remark = j.value("remark", "");

    if (j.contains("tcp_config")) {
        node.tcp_cfg.header_type = j["tcp_config"].value("header_type", "none");
    }
    if (j.contains("ws_config")) {
        node.ws_cfg.path = j["ws_config"].value("path", "/");
        node.ws_cfg.host = j["ws_config"].value("host", "");
    }
    if (j.contains("h2_config")) {
        node.h2_cfg.path = j["h2_config"].value("path", "/");
        if (j["h2_config"].contains("host") && j["h2_config"]["host"].is_array()) {
            for (const auto& h : j["h2_config"]["host"])
                node.h2_cfg.host.push_back(h.get<std::string>());
        }
    }
    if (j.contains("quic_config")) {
        node.quic_cfg.security = j["quic_config"].value("security", "none");
        node.quic_cfg.key = j["quic_config"].value("key", "");
        node.quic_cfg.header_type = j["quic_config"].value("header_type", "none");
    }
    if (j.contains("kcp_config")) {
        node.kcp_cfg.mtu = j["kcp_config"].value("mtu", 1350);
        node.kcp_cfg.tti = j["kcp_config"].value("tti", 20);
        node.kcp_cfg.uplink_capacity = j["kcp_config"].value("uplink_capacity", 5);
        node.kcp_cfg.downlink_capacity = j["kcp_config"].value("downlink_capacity", 20);
        node.kcp_cfg.congestion = j["kcp_config"].value("congestion", false);
        node.kcp_cfg.header_type = j["kcp_config"].value("header_type", "none");
    }
    if (j.contains("tls_config")) {
        node.tls_cfg.allow_insecure = j["tls_config"].value("allow_insecure", false);
        node.tls_cfg.server_name = j["tls_config"].value("server_name", "");
    }
    return node;
}
