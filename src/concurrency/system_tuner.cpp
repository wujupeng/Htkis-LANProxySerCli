#include "concurrency/system_tuner.h"
#include "concurrency/overload_protector.h"
#include <fstream>
#include <sstream>
#include <sys/resource.h>

system_tuner& system_tuner::instance() {
    static system_tuner inst;
    return inst;
}

static std::string read_sysctl(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "N/A";
    std::string val;
    std::getline(f, val);
    while (!val.empty() && (val.back() == '\n' || val.back() == '\r' || val.back() == ' '))
        val.pop_back();
    return val;
}

std::vector<sys_check_result> system_tuner::run_startup_check() {
    m_results.clear();

    {
        sys_check_result r;
        r.item = "fs.file-max (ulimit -n)";
        std::ifstream fds("/proc/self/fd");
        int used_fds = 0;
        if (fds.is_open()) { std::string line; while (std::getline(fds, line)) used_fds++; }
        struct rlimit rl;
        if (getrlimit(RLIMIT_NOFILE, &rl) == 0) {
            r.current_value = std::to_string(rl.rlim_cur);
            r.recommend_value = "65535";
            r.is_ok = (rl.rlim_cur >= 65535);
            if (!r.is_ok) r.suggestion = "Add 'ulimit -n 65535' or set LimitNOFILE=65535 in systemd unit";
        } else {
            r.current_value = "unknown";
            r.recommend_value = "65535";
            r.is_ok = false;
        }
        m_results.push_back(r);
    }

    {
        sys_check_result r;
        r.item = "net.core.somaxconn";
        r.current_value = read_sysctl("/proc/sys/net/core/somaxconn");
        r.recommend_value = "65535";
        try { r.is_ok = std::stoi(r.current_value) >= 65535; } catch (...) { r.is_ok = false; }
        if (!r.is_ok) r.suggestion = "sysctl -w net.core.somaxconn=65535";
        m_results.push_back(r);
    }

    {
        sys_check_result r;
        r.item = "net.ipv4.tcp_tw_reuse";
        r.current_value = read_sysctl("/proc/sys/net/ipv4/tcp_tw_reuse");
        r.recommend_value = "1";
        r.is_ok = (r.current_value == "1");
        if (!r.is_ok) r.suggestion = "sysctl -w net.ipv4.tcp_tw_reuse=1";
        m_results.push_back(r);
    }

    {
        sys_check_result r;
        r.item = "net.ipv4.tcp_max_syn_backlog";
        r.current_value = read_sysctl("/proc/sys/net/ipv4/tcp_max_syn_backlog");
        r.recommend_value = "65535";
        try { r.is_ok = std::stoi(r.current_value) >= 65535; } catch (...) { r.is_ok = false; }
        if (!r.is_ok) r.suggestion = "sysctl -w net.ipv4.tcp_max_syn_backlog=65535";
        m_results.push_back(r);
    }

    return m_results;
}

std::vector<sys_check_result> system_tuner::run_runtime_check() {
    auto status = overload_protector::instance().get_status();
    m_results.clear();

    {
        sys_check_result r;
        r.item = "fd_usage";
        char buf[64];
        snprintf(buf, sizeof(buf), "%.1f%%", status.fd_usage_pct * 100);
        r.current_value = buf;
        r.recommend_value = "<90%";
        r.is_ok = status.fd_usage_pct < 0.9;
        if (!r.is_ok) r.suggestion = "Increase ulimit -n or reduce connections";
        m_results.push_back(r);
    }

    {
        sys_check_result r;
        r.item = "mem_usage";
        char buf[64];
        snprintf(buf, sizeof(buf), "%.1f%%", status.mem_usage_pct * 100);
        r.current_value = buf;
        r.recommend_value = "<85%";
        r.is_ok = status.mem_usage_pct < 0.85;
        if (!r.is_ok) r.suggestion = "Reduce max_total_connections or add memory";
        m_results.push_back(r);
    }

    {
        sys_check_result r;
        r.item = "total_connections";
        r.current_value = std::to_string(status.total_connections);
        r.recommend_value = "<4000";
        r.is_ok = !status.is_overloaded;
        if (!r.is_ok) r.suggestion = "System overloaded, reduce load";
        m_results.push_back(r);
    }

    return m_results;
}

std::string system_tuner::generate_report() {
    std::ostringstream oss;
    oss << "=== System Tuning Report ===\n";
    for (const auto& r : m_results) {
        oss << r.item << ": " << r.current_value
            << " (recommended: " << r.recommend_value << ")"
            << " [" << (r.is_ok ? "OK" : "WARN") << "]";
        if (!r.is_ok && !r.suggestion.empty()) {
            oss << " -> " << r.suggestion;
        }
        oss << "\n";
    }
    return oss.str();
}

std::vector<sys_check_result> system_tuner::get_results() const {
    return m_results;
}

std::string system_tuner::generate_tune_script() const {
    std::ostringstream oss;
    oss << "#!/bin/bash\n";
    oss << "# LAN Proxy Gateway - System Tuning Script\n";
    oss << "# Run as root: sudo bash tune.sh\n\n";
    oss << "sysctl -w net.core.somaxconn=65535\n";
    oss << "sysctl -w net.ipv4.tcp_tw_reuse=1\n";
    oss << "sysctl -w net.ipv4.tcp_max_syn_backlog=65535\n";
    oss << "sysctl -w net.ipv4.ip_local_port_range=\"1024 65535\"\n";
    oss << "sysctl -w net.ipv4.tcp_fin_timeout=15\n";
    oss << "\necho 'System tuning applied successfully'\n";
    return oss.str();
}
