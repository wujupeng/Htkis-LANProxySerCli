#pragma once
#include <string>
#include <nlohmann/json.hpp>

class audit_logger {
public:
    static audit_logger& instance();

    void init(const std::string& log_dir);
    void log(const std::string& op, const std::string& action,
             const std::string& target, const std::string& detail = "");

private:
    audit_logger() = default;
    std::string m_log_path;
};
