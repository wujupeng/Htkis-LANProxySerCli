#pragma once

#include <string>
#include <memory>
#include <shared_mutex>
#include <deque>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <nlohmann/json.hpp>

class structured_logger {
public:
    static structured_logger& instance();

    void init(const std::string& log_dir, const std::string& level = "info");

    void info(const std::string& module, const std::string& msg, const std::string& session_id = "");
    void warn(const std::string& module, const std::string& msg, const std::string& session_id = "");
    void error(const std::string& module, const std::string& msg, const std::string& session_id = "");

    nlohmann::json get_recent_logs(size_t count, const std::string& level_filter = "");
    nlohmann::json get_recent_logs(size_t count, const std::string& level_filter,
                                    int64_t start_ts, int64_t end_ts);

    void broadcast_callback(std::function<void(const std::string&)> cb);

private:
    structured_logger() = default;

    void add_memory_entry(const nlohmann::json& entry);

    std::shared_ptr<spdlog::logger> m_logger;
    std::deque<nlohmann::json> m_memory_buffer;
    std::shared_mutex m_buffer_mutex;
    static constexpr size_t MAX_MEMORY_ENTRIES = 2000;
    std::function<void(const std::string&)> m_broadcast_cb;
    std::mutex m_broadcast_mutex;
};
