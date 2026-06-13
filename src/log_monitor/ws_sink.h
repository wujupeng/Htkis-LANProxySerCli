#pragma once
#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>
#include <mutex>
#include <functional>
#include <string>

template<typename Mutex>
class ws_sink : public spdlog::sinks::base_sink<Mutex> {
public:
    void set_broadcast(std::function<void(const std::string&)> cb) {
        m_broadcast_cb = std::move(cb);
    }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        if (!m_broadcast_cb) return;
        spdlog::memory_buf_t formatted;
        spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);
        m_broadcast_cb(std::string(formatted.data(), formatted.size()));
    }

    void flush_() override {}

private:
    std::function<void(const std::string&)> m_broadcast_cb;
};

using ws_sink_mt = ws_sink<std::mutex>;
