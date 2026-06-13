#pragma once
#include "route_decision/route_engine.h"
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

class rule_manager {
public:
    bool add_rule(const route_rule& rule, const std::string& save_path);
    bool remove_rule(const std::string& rule_id, const std::string& save_path);
    std::vector<route_rule> get_rules() const;
    bool reload_from_file(const std::string& path);
    bool save_to_file(const std::string& path) const;
};
