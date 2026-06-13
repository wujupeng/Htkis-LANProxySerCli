#include "route_decision/rule_manager.h"
#include "log_monitor/structured_logger.h"
#include <fstream>
#include <algorithm>

bool rule_manager::add_rule(const route_rule& rule, const std::string& save_path) {
    route_engine::instance().add_custom_rule(rule);
    if (!save_path.empty()) save_to_file(save_path);
    structured_logger::instance().info("rule_manager", "Rule added: " + rule.rule_id);
    return true;
}

bool rule_manager::remove_rule(const std::string& rule_id, const std::string& save_path) {
    if (route_engine::instance().remove_custom_rule(rule_id)) {
        if (!save_path.empty()) save_to_file(save_path);
        structured_logger::instance().info("rule_manager", "Rule removed: " + rule_id);
        return true;
    }
    return false;
}

std::vector<route_rule> rule_manager::get_rules() const {
    return route_engine::instance().get_all_rules();
}

bool rule_manager::reload_from_file(const std::string& path) {
    route_engine::instance().reload_rules();
    return true;
}

bool rule_manager::save_to_file(const std::string& path) const {
    auto rules = route_engine::instance().get_all_rules();
    nlohmann::json j;
    j["rules"] = nlohmann::json::array();

    for (const auto& r : rules) {
        if (r.is_builtin) continue;
        nlohmann::json rule_j;
        rule_j["rule_id"] = r.rule_id;
        rule_j["rule_type"] = r.rule_type;
        rule_j["pattern"] = r.pattern;
        rule_j["action"] = (r.action == route_action::direct) ? "direct" : "proxy";
        rule_j["priority"] = r.priority;
        rule_j["enabled"] = r.enabled;
        j["rules"].push_back(rule_j);
    }

    std::string tmp = path + ".tmp";
    {
        std::ofstream file(tmp);
        if (!file.is_open()) return false;
        file << j.dump(4);
    }
    return std::rename(tmp.c_str(), path.c_str()) == 0;
}
