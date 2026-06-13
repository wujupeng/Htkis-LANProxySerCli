#pragma once
#include <string>
#include <vector>
#include <optional>
#include <shared_mutex>
#include <nlohmann/json.hpp>

struct vm_node {
    std::string tag;
    std::string address;
    uint16_t port{443};
    std::string user_id;
    int alter_id{0};
    std::string security{"auto"};
    std::string network{"tcp"};
    std::string tls{"none"};
    std::string remark;

    struct tcp_config {
        std::string header_type{"none"};
    } tcp_cfg;

    struct ws_config {
        std::string path{"/"};
        std::string host;
    } ws_cfg;

    struct h2_config {
        std::string path{"/"};
        std::vector<std::string> host;
    } h2_cfg;

    struct quic_config {
        std::string security{"none"};
        std::string key;
        std::string header_type{"none"};
    } quic_cfg;

    struct kcp_config {
        int mtu{1350};
        int tti{20};
        int uplink_capacity{5};
        int downlink_capacity{20};
        bool congestion{false};
        std::string header_type{"none"};
    } kcp_cfg;

    struct tls_config {
        bool allow_insecure{false};
        std::string server_name;
    } tls_cfg;
};

class vm_node_manager {
public:
    static vm_node_manager& instance();

    bool load_from_config(const std::string& config_path);

    std::vector<vm_node> list_nodes(bool desensitize_id = true) const;
    std::optional<vm_node> get_node(const std::string& tag) const;

    bool add_node(const vm_node& node, std::string& error_msg);
    bool update_node(const std::string& old_tag, const vm_node& node, std::string& error_msg);
    bool remove_node(const std::string& tag, std::string& error_msg);
    bool reorder_nodes(const std::vector<std::string>& ordered_tags);

    bool apply_config(std::string& error_msg);
    bool set_active_node(const std::string& tag, std::string& error_msg);
    std::string get_active_node() const;

    std::optional<vm_node> import_from_link(const std::string& vmess_link, std::string& error_msg);
    std::string export_to_link(const vm_node& node) const;

    std::string generate_auto_tag() const;
    static std::string desensitize_uuid(const std::string& uuid);

    nlohmann::json node_to_json(const vm_node& node, bool desensitize_id = false) const;
    vm_node json_to_node(const nlohmann::json& j) const;

private:
    vm_node_manager() = default;

    std::vector<vm_node> m_nodes;
    mutable std::shared_mutex m_nodes_mutex;
    std::string m_config_path;
    std::string m_active_node_tag;
    bool m_is_v5_core{true};

    std::optional<vm_node> parse_outbound(const nlohmann::json& outbound) const;
    bool save_to_config(std::string& error_msg);
    bool is_tag_unique(const std::string& tag, const std::string& exclude_tag = "") const;
};
