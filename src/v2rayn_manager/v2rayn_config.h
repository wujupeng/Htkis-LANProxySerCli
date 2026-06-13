#pragma once
#include <string>
#include <nlohmann/json.hpp>

class v2rayn_config {
public:
    static nlohmann::json load(const std::string& path);
    static bool save(const std::string& path, const nlohmann::json& config);
    static bool validate(const nlohmann::json& config);
    static nlohmann::json desensitize(const nlohmann::json& config);
    static nlohmann::json generate_default_inbound(int socks_port, int http_port);
};
