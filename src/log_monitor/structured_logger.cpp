#include "log_monitor/structured_logger.h"
#include <chrono>
#include <fstream>

structured_logger& structured_logger::instance() {
    static structured_logger inst;
    return inst;
}

void structured_logger::init(const std::string& log_dir, const std::string& level) {
    std::vector<spdlog::sink_ptr> sinks;

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    sinks.push_back(console_sink);

    std::string log_path = log_dir;
    if (!log_path.empty() && log_path.back() != '/') log_path += '/';
    log_path += "lan_proxy_gateway.log";

    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        log_path, 10 * 1024 * 1024, 5);
    sinks.push_back(file_sink);

    m_logger = std::make_shared<spdlog::logger>("lpg", sinks.begin(), sinks.end());
    m_logger->set_pattern("%v");

    if (level == "debug") m_logger->set_level(spdlog::level::debug);
    else if (level == "warn") m_logger->set_level(spdlog::level::warn);
    else if (level == "error") m_logger->set_level(spdlog::level::err);
    else m_logger->set_level(spdlog::level::info);

    spdlog::set_default_logger(m_logger);
}

void structured_logger::info(const std::string& module, const std::string& msg,
                              const std::string& session_id) {
    auto now = std::chrono::system_clock::now();
    auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    nlohmann::json entry;
    entry["ts"] = ts;
    entry["level"] = "info";
    entry["module"] = module;
    entry["msg"] = msg;
    if (!session_id.empty()) entry["session_id"] = session_id;

    m_logger->info("{}", entry.dump());
    add_memory_entry(entry);

    std::lock_guard<std::mutex> lock(m_broadcast_mutex);
    if (m_broadcast_cb) m_broadcast_cb(entry.dump());
}

void structured_logger::warn(const std::string& module, const std::string& msg,
                              const std::string& session_id) {
    auto now = std::chrono::system_clock::now();
    auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    nlohmann::json entry;
    entry["ts"] = ts;
    entry["level"] = "warn";
    entry["module"] = module;
    entry["msg"] = msg;
    if (!session_id.empty()) entry["session_id"] = session_id;

    m_logger->warn("{}", entry.dump());
    add_memory_entry(entry);

    std::lock_guard<std::mutex> lock(m_broadcast_mutex);
    if (m_broadcast_cb) m_broadcast_cb(entry.dump());
}

void structured_logger::error(const std::string& module, const std::string& msg,
                               const std::string& session_id) {
    auto now = std::chrono::system_clock::now();
    auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    nlohmann::json entry;
    entry["ts"] = ts;
    entry["level"] = "error";
    entry["module"] = module;
    entry["msg"] = msg;
    if (!session_id.empty()) entry["session_id"] = session_id;

    m_logger->error("{}", entry.dump());
    add_memory_entry(entry);

    std::lock_guard<std::mutex> lock(m_broadcast_mutex);
    if (m_broadcast_cb) m_broadcast_cb(entry.dump());
}

void structured_logger::add_memory_entry(const nlohmann::json& entry) {
    std::unique_lock<std::shared_mutex> lock(m_buffer_mutex);
    m_memory_buffer.push_back(entry);
    if (m_memory_buffer.size() > MAX_MEMORY_ENTRIES) {
        m_memory_buffer.erase(m_memory_buffer.begin(),
            m_memory_buffer.begin() + (m_memory_buffer.size() - MAX_MEMORY_ENTRIES));
    }
}

nlohmann::json structured_logger::get_recent_logs(size_t count,
                                                    const std::string& level_filter) {
    std::shared_lock<std::shared_mutex> lock(m_buffer_mutex);
    nlohmann::json result = nlohmann::json::array();

    size_t added = 0;
    for (auto it = m_memory_buffer.rbegin();
         it != m_memory_buffer.rend() && added < count; ++it) {
        if (level_filter.empty() || (*it)["level"] == level_filter) {
            result.push_back(*it);
            ++added;
        }
    }
    return result;
}

nlohmann::json structured_logger::get_recent_logs(size_t count,
                                                    const std::string& level_filter,
                                                    int64_t start_ts, int64_t end_ts) {
    std::shared_lock<std::shared_mutex> lock(m_buffer_mutex);
    nlohmann::json result = nlohmann::json::array();

    size_t added = 0;
    for (auto it = m_memory_buffer.rbegin();
         it != m_memory_buffer.rend() && added < count; ++it) {
        int64_t ts = (*it)["ts"].get<int64_t>();
        if (ts < start_ts || ts > end_ts) continue;
        if (level_filter.empty() || (*it)["level"] == level_filter) {
            result.push_back(*it);
            ++added;
        }
    }
    return result;
}

void structured_logger::broadcast_callback(std::function<void(const std::string&)> cb) {
    std::lock_guard<std::mutex> lock(m_broadcast_mutex);
    m_broadcast_cb = std::move(cb);
}
