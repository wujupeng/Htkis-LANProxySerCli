#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <ctime>
#include <deque>

enum class LogLevel {
    Info,
    Warn,
    Error
};

struct LogEntry {
    std::time_t time;
    LogLevel level;
    std::string message;
};

class Logger {
public:
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    void info(const std::string& msg) { add(LogLevel::Info, msg); }
    void warn(const std::string& msg) { add(LogLevel::Warn, msg); }
    void error(const std::string& msg) { add(LogLevel::Error, msg); }

    std::vector<LogEntry> getEntries() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return std::vector<LogEntry>(m_entries.begin(), m_entries.end());
    }

    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_entries.clear();
    }

    bool hasNewEntries() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_dirty;
    }

    void markRead() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_dirty = false;
    }

    static const size_t MAX_ENTRIES = 2000;

private:
    Logger() = default;

    void add(LogLevel level, const std::string& msg) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_entries.push_back({std::time(nullptr), level, msg});
        if (m_entries.size() > MAX_ENTRIES) {
            m_entries.erase(m_entries.begin(), m_entries.begin() + (m_entries.size() - MAX_ENTRIES));
        }
        m_dirty = true;
    }

    std::deque<LogEntry> m_entries;
    std::mutex m_mutex;
    bool m_dirty = false;
};
