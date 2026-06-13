#include "v2rayn_manager/v2rayn_config.h"
#include <fstream>

nlohmann::json v2rayn_config::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return {};
    try {
        nlohmann::json j;
        file >> j;
        return j;
    } catch (...) {
        return {};
    }
}

bool v2rayn_config::save(const std::string& path, const nlohmann::json& config) {
    std::string tmp = path + ".tmp";
    {
        std::ofstream file(tmp);
        if (!file.is_open()) return false;
        file << config.dump(4);
    }
    return std::rename(tmp.c_str(), path.c_str()) == 0;
}

bool v2rayn_config::validate(const nlohmann::json& config) {
    if (!config.is_object()) return false;
    if (!config.contains("inbounds") || !config["inbounds"].is_array()) return false;
    if (!config.contains("outbounds") || !config["outbounds"].is_array()) return false;
    return true;
}

nlohmann::json v2rayn_config::desensitize(const nlohmann::json& config) {
    nlohmann::json result = config;
    if (result.contains("outbounds") && result["outbounds"].is_array()) {
        for (auto& outbound : result["outbounds"]) {
            if (outbound.contains("settings") && outbound["settings"].is_object()) {
                auto& settings = outbound["settings"];
                if (settings.contains("vnext") && settings["vnext"].is_array()) {
                    for (auto& v : settings["vnext"]) {
                        if (v.contains("users") && v["users"].is_array()) {
                            for (auto& u : v["users"]) {
                                if (u.contains("id")) u["id"] = "********";
                            }
                        }
                    }
                }
                if (settings.contains("servers") && settings["servers"].is_array()) {
                    for (auto& s : settings["servers"]) {
                        if (s.contains("password")) s["password"] = "********";
                    }
                }
            }
        }
    }
    return result;
}

nlohmann::json v2rayn_config::generate_default_inbound(int socks_port, int http_port) {
    nlohmann::json config;
    config["log"] = {{"loglevel", "warning"}};
    config["inbounds"] = nlohmann::json::array({
        {
            {"tag", "socks-in"},
            {"protocol", "socks"},
            {"listen", "127.0.0.1"},
            {"port", socks_port},
            {"settings", {{"auth", "noauth"}, {"udp", false}}}
        },
        {
            {"tag", "http-in"},
            {"protocol", "http"},
            {"listen", "127.0.0.1"},
            {"port", http_port}
        }
    });
    config["outbounds"] = nlohmann::json::array({
        {
            {"protocol", "freedom"},
            {"tag", "direct"}
        }
    });
    return config;
}
