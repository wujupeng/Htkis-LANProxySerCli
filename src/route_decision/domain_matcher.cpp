#include "route_decision/domain_matcher.h"
#include <nlohmann/json.hpp>
#include <fstream>

void domain_matcher::load_builtin_rules(const std::string& json_path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ifstream file(json_path);
    if (!file.is_open()) return;

    try {
        nlohmann::json j;
        file >> j;
        if (j.contains("rules") && j["rules"].is_array()) {
            for (const auto& r : j["rules"]) {
                std::string type = r.value("rule_type", "");
                std::string pattern = r.value("pattern", "");
                bool is_direct = r.value("action", "proxy") == "direct";
                if (type == "domain_exact") add_exact(pattern, is_direct);
                else if (type == "domain_suffix") add_suffix(pattern, is_direct);
            }
        }
    } catch (...) {}
}

void domain_matcher::add_exact(const std::string& domain, bool is_direct) {
    if (is_direct) m_direct_exact.insert(domain);
    else m_proxy_exact.insert(domain);
}

void domain_matcher::add_suffix(const std::string& suffix, bool is_direct) {
    m_suffix_list.push_back({suffix, is_direct});
}

bool domain_matcher::match(const std::string& domain, bool& is_direct) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_direct_exact.count(domain)) { is_direct = true; return true; }
    if (m_proxy_exact.count(domain)) { is_direct = false; return true; }

    for (const auto& [suffix, direct] : m_suffix_list) {
        if (domain.length() >= suffix.length() &&
            domain.compare(domain.length() - suffix.length(),
                           suffix.length(), suffix) == 0) {
            is_direct = direct;
            return true;
        }
    }
    return false;
}

void domain_matcher::clear() {
    m_direct_exact.clear();
    m_proxy_exact.clear();
    m_suffix_list.clear();
}
