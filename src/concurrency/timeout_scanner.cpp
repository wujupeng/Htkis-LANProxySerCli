#include "concurrency/timeout_scanner.h"
#include "concurrency/connection_registry.h"
#include "log_monitor/structured_logger.h"
#include <thread>
#include <mutex>

timeout_scanner& timeout_scanner::instance() {
    static timeout_scanner inst;
    return inst;
}

void timeout_scanner::init(int auth_timeout_sec, int handshake_timeout_sec,
                             int idle_timeout_sec, int max_lifetime_sec,
                             int scan_interval_sec) {
    m_auth_timeout_sec = auth_timeout_sec;
    m_handshake_timeout_sec = handshake_timeout_sec;
    m_idle_timeout_sec = idle_timeout_sec;
    m_max_lifetime_sec = max_lifetime_sec;
    m_scan_interval_sec = scan_interval_sec;
}

void timeout_scanner::start() {
    if (m_running.exchange(true)) return;
    std::thread([this]() {
        while (m_running.load()) {
            scan_loop();
            for (int i = 0; i < m_scan_interval_sec && m_running.load(); ++i) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    }).detach();
    structured_logger::instance().info("timeout_scanner", "超时扫描已启动");
}

void timeout_scanner::stop() {
    m_running = false;
}

void timeout_scanner::set_timeout_callback(
    std::function<void(const std::string&, const std::string&)> callback) {
    m_on_timeout = std::move(callback);
}

void timeout_scanner::scan_loop() {
    auto candidates = connection_registry::instance().get_timeout_candidates();
    auto now = std::chrono::steady_clock::now();

    std::vector<std::string> batch;
    for (const auto& rec : candidates) {
        std::string reason;
        bool should_close = false;

        if (rec.state == connection_state::authenticating) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - rec.admit_time).count();
            if (elapsed >= m_auth_timeout_sec) {
                reason = "auth_timeout";
                should_close = true;
            }
        } else if (rec.state == connection_state::handshaking) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - rec.admit_time).count();
            if (elapsed >= m_handshake_timeout_sec) {
                reason = "handshake_timeout";
                should_close = true;
            }
        } else if (rec.state == connection_state::idle && !rec.exempt_idle) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - rec.idle_since).count();
            if (elapsed >= m_idle_timeout_sec) {
                reason = "idle_timeout";
                should_close = true;
            }
        }

        if (rec.state == connection_state::active || rec.state == connection_state::idle) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - rec.admit_time).count();
            if (elapsed >= m_max_lifetime_sec) {
                reason = "max_lifetime";
                should_close = true;
            }
        }

        if (should_close && m_on_timeout) {
            batch.push_back(rec.session_id);
            m_on_timeout(rec.session_id, reason);
        }

        if (batch.size() >= 100) {
            structured_logger::instance().info("timeout_scanner",
                "Batch closing " + std::to_string(batch.size()) + " connections");
            batch.clear();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}
