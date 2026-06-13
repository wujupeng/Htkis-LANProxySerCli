#include "route_decision/route_engine.h"
#include "route_decision/geoip_resolver.h"
#include "log_monitor/structured_logger.h"
#include <fstream>
#include <algorithm>
#include <random>

route_engine& route_engine::instance() {
    static route_engine inst;
    return inst;
}

void route_engine::init(const std::string& builtin_rules_path,
                         const std::string& custom_rules_path,
                         const std::string& geoip_db_path,
                         const std::string& default_action) {
    m_geoip_db_path = geoip_db_path;
    m_default_action = (default_action == "direct") ? route_action::direct : route_action::proxy;

    geoip_resolver::instance().init(geoip_db_path);
    m_geoip_available = geoip_resolver::instance().available();

    load_builtin_rules(builtin_rules_path);
    load_custom_rules(custom_rules_path);
    build_domain_index();

    structured_logger::instance().info("route_engine",
        "Initialized with " + std::to_string(m_builtin_rules.size()) + " builtin + " +
        std::to_string(m_custom_rules.size()) + " custom rules, geoip=" +
        (m_geoip_available ? "yes" : "no"));
}

route_result route_engine::decide(const std::string& host, const std::string& ip) {
    std::shared_lock<std::shared_mutex> lock(m_rules_mutex);

    auto result = match_custom_rules(host, ip);
    if (result.action != route_action::proxy || !result.rule_id.empty()) {
        if (!result.rule_id.empty()) return result;
    }

    result = match_domain_rules(host);
    if (!result.rule_id.empty()) return result;

    if (!ip.empty()) {
        result = match_geoip(ip);
        if (!result.rule_id.empty()) return result;
    }

    return default_decision();
}

route_result route_engine::match_custom_rules(const std::string& host,
                                                const std::string& ip) {
    for (const auto& rule : m_custom_rules) {
        if (!rule.enabled) continue;

        if (rule.rule_type == "domain_exact" && host == rule.pattern) {
            return {rule.action, rule.rule_id, "custom_domain_exact"};
        }
        if (rule.rule_type == "domain_suffix") {
            if (host.length() >= rule.pattern.length() &&
                host.compare(host.length() - rule.pattern.length(),
                             rule.pattern.length(), rule.pattern) == 0) {
                return {rule.action, rule.rule_id, "custom_domain_suffix"};
            }
        }
        if (rule.rule_type == "ip_cidr" && !ip.empty()) {
            return {rule.action, rule.rule_id, "custom_ip_cidr"};
        }
    }
    return {route_action::proxy, "", ""};
}

route_result route_engine::match_domain_rules(const std::string& host) {
    auto it = m_exact_domain_map.find(host);
    if (it != m_exact_domain_map.end()) {
        return {it->second, "builtin_exact_" + host, "builtin_domain_exact"};
    }

    for (const auto& [suffix, action] : m_suffix_domain_list) {
        if (host.length() >= suffix.length() &&
            host.compare(host.length() - suffix.length(),
                         suffix.length(), suffix) == 0) {
            return {action, "builtin_suffix_" + suffix, "builtin_domain_suffix"};
        }
    }
    return {route_action::proxy, "", ""};
}

route_result route_engine::match_geoip(const std::string& ip) {
    if (!m_geoip_available) return {route_action::proxy, "", ""};

    if (geoip_resolver::instance().is_china(ip)) {
        return {route_action::direct, "geoip_cn", "geoip"};
    }
    return {route_action::proxy, "geoip_international", "geoip"};
}

route_result route_engine::default_decision() {
    return {m_default_action, "default", "default"};
}

void route_engine::reload_rules() {
    std::unique_lock<std::shared_mutex> lock(m_rules_mutex);

    std::string builtin_path, custom_path;
    builtin_path = m_builtin_rules.empty() ? "" : "reload";
    load_builtin_rules(builtin_path);

    load_custom_rules(custom_path);
    build_domain_index();

    structured_logger::instance().info("route_engine", "Rules reloaded");
}

void route_engine::add_custom_rule(const route_rule& rule) {
    std::unique_lock<std::shared_mutex> lock(m_rules_mutex);
    m_custom_rules.push_back(rule);
    std::sort(m_custom_rules.begin(), m_custom_rules.end(),
              [](const route_rule& a, const route_rule& b) {
                  return a.priority > b.priority;
              });
}

bool route_engine::remove_custom_rule(const std::string& rule_id) {
    std::unique_lock<std::shared_mutex> lock(m_rules_mutex);
    auto it = std::find_if(m_custom_rules.begin(), m_custom_rules.end(),
                           [&](const route_rule& r) { return r.rule_id == rule_id; });
    if (it != m_custom_rules.end() && !it->is_builtin) {
        m_custom_rules.erase(it);
        return true;
    }
    return false;
}

std::vector<route_rule> route_engine::get_all_rules() const {
    std::shared_lock<std::shared_mutex> lock(m_rules_mutex);
    std::vector<route_rule> all = m_builtin_rules;
    all.insert(all.end(), m_custom_rules.begin(), m_custom_rules.end());
    return all;
}

void route_engine::set_default_action(const std::string& action) {
    m_default_action = (action == "direct") ? route_action::direct : route_action::proxy;
}

void route_engine::load_builtin_rules(const std::string& path) {
    if (path.empty()) return;
    m_builtin_rules.clear();

    std::ifstream file(path);
    if (!file.is_open()) {
        structured_logger::instance().warn("route_engine",
            "Builtin rules file not found: " + path);
        return;
    }

    try {
        nlohmann::json j;
        file >> j;

        if (j.contains("rules") && j["rules"].is_array()) {
            for (const auto& r : j["rules"]) {
                route_rule rule;
                rule.rule_id = r.value("rule_id", "");
                rule.rule_type = r.value("rule_type", "");
                rule.pattern = r.value("pattern", "");
                rule.action = (r.value("action", "direct") == "direct")
                              ? route_action::direct : route_action::proxy;
                rule.priority = r.value("priority", 0);
                rule.enabled = r.value("enabled", true);
                rule.is_builtin = true;
                m_builtin_rules.push_back(rule);
            }
        }
    } catch (const nlohmann::json::exception& e) {
        structured_logger::instance().error("route_engine",
            "Parse builtin rules error: " + std::string(e.what()));
    }
}

void route_engine::load_custom_rules(const std::string& path) {
    m_custom_rules.clear();
    if (path.empty()) return;

    std::ifstream file(path);
    if (!file.is_open()) return;

    try {
        nlohmann::json j;
        file >> j;

        if (j.contains("rules") && j["rules"].is_array()) {
            for (const auto& r : j["rules"]) {
                route_rule rule;
                rule.rule_id = r.value("rule_id", "");
                rule.rule_type = r.value("rule_type", "");
                rule.pattern = r.value("pattern", "");
                rule.action = (r.value("action", "direct") == "direct")
                              ? route_action::direct : route_action::proxy;
                rule.priority = r.value("priority", 0);
                rule.enabled = r.value("enabled", true);
                rule.is_builtin = false;
                m_custom_rules.push_back(rule);
            }
        }
    } catch (const nlohmann::json::exception& e) {
        structured_logger::instance().error("route_engine",
            "Parse custom rules error: " + std::string(e.what()));
    }
}

void route_engine::build_domain_index() {
    m_exact_domain_map.clear();
    m_suffix_domain_list.clear();

    for (const auto& rule : m_builtin_rules) {
        if (!rule.enabled) continue;
        if (rule.rule_type == "domain_exact") {
            m_exact_domain_map[rule.pattern] = rule.action;
        } else if (rule.rule_type == "domain_suffix") {
            m_suffix_domain_list.push_back({rule.pattern, rule.action});
        }
    }
    for (const auto& rule : m_custom_rules) {
        if (!rule.enabled) continue;
        if (rule.rule_type == "domain_exact") {
            m_exact_domain_map[rule.pattern] = rule.action;
        } else if (rule.rule_type == "domain_suffix") {
            m_suffix_domain_list.push_back({rule.pattern, rule.action});
        }
    }
}
