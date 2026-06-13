#pragma once
#include <string>
#include <unordered_set>
#include <vector>
#include <mutex>

struct domain_rule {
    std::string pattern;
    bool is_direct;
};

class domain_matcher {
public:
    void load_builtin_rules(const std::string& json_path);
    void add_exact(const std::string& domain, bool is_direct);
    void add_suffix(const std::string& suffix, bool is_direct);
    bool match(const std::string& domain, bool& is_direct) const;
    void clear();

private:
    std::unordered_set<std::string> m_direct_exact;
    std::unordered_set<std::string> m_proxy_exact;
    std::vector<std::pair<std::string, bool>> m_suffix_list;
    mutable std::mutex m_mutex;
};
