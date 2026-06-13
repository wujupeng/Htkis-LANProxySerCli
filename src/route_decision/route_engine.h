#pragma once

#include <string>
#include <shared_mutex>
#include <vector>
#include <nlohmann/json.hpp>

enum class route_action { direct, proxy };

struct route_result {
    route_action action;
    std::string rule_id;
    std::string match_type;
};

struct route_rule {
    std::string rule_id;
    std::string rule_type;
    std::string pattern;
    route_action action;
    int priority{0};
    bool enabled{true};
    bool is_builtin{false};
};

class route_engine {
public:
    static route_engine& instance();

    void init(const std::string& builtin_rules_path,
              const std::string& custom_rules_path,
              const std::string& geoip_db_path,
              const std::string& default_action);

    route_result decide(const std::string& host, const std::string& ip = "");

    void reload_rules();
    void add_custom_rule(const route_rule& rule);
    bool remove_custom_rule(const std::string& rule_id);
    std::vector<route_rule> get_all_rules() const;

    void set_default_action(const std::string& action);

private:
    route_engine() = default;

    route_result match_custom_rules(const std::string& host, const std::string& ip);
    route_result match_domain_rules(const std::string& host);
    route_result match_geoip(const std::string& ip);
    route_result default_decision();

    std::vector<route_rule> m_builtin_rules;
    std::vector<route_rule> m_custom_rules;
    route_action m_default_action{route_action::proxy};

    std::unordered_map<std::string, route_action> m_exact_domain_map;
    std::vector<std::pair<std::string, route_action>> m_suffix_domain_list;

    bool m_geoip_available{false};
    std::string m_geoip_db_path;

    mutable std::shared_mutex m_rules_mutex;

    void load_builtin_rules(const std::string& path);
    void load_custom_rules(const std::string& path);
    void build_domain_index();
};
