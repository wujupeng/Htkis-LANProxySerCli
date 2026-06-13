#include "log_monitor/audit_logger.h"
#include <chrono>
#include <fstream>

audit_logger& audit_logger::instance() {
    static audit_logger inst;
    return inst;
}

void audit_logger::init(const std::string& log_dir) {
    m_log_path = log_dir;
    if (!m_log_path.empty() && m_log_path.back() != '/') m_log_path += '/';
    m_log_path += "audit.log";
}

void audit_logger::log(const std::string& op, const std::string& action,
                         const std::string& target, const std::string& detail) {
    auto now = std::chrono::system_clock::now();
    auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    nlohmann::json entry;
    entry["ts"] = ts;
    entry["operator"] = op;
    entry["action"] = action;
    entry["target"] = target;
    if (!detail.empty()) entry["detail"] = detail;

    std::ofstream file(m_log_path, std::ios::app);
    if (file.is_open()) {
        file << entry.dump() << "\n";
    }
}
