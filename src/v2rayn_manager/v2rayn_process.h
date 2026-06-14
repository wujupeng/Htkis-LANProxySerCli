#pragma once
#include <string>
#include <atomic>
#include <functional>
#include <mutex>
#include <chrono>

enum class v2rayn_status { stopped, starting, running, failed };

class v2rayn_process {
public:
    static v2rayn_process& instance();

    bool start(const std::string& exec_path, const std::string& config_path,
               const std::string& log_path);
    bool stop();
    bool restart();

    v2rayn_status status() const { return m_status; }
    int pid() const { return m_pid; }
    int crash_count() const { return m_crash_count; }

    void set_on_exit(std::function<void(int)> cb);

    bool check_child_exit() {
        if (m_child_exited.exchange(false)) {
            on_child_exit();
            return true;
        }
        return false;
    }

private:
    v2rayn_process() = default;

    bool do_start();
    bool do_stop();
    void on_child_exit();
    void reset_crash_window();

    std::atomic<v2rayn_status> m_status{v2rayn_status::stopped};
    std::atomic<int> m_pid{0};
    std::atomic<int> m_crash_count{0};
    std::atomic<bool> m_child_exited{false};
    std::chrono::steady_clock::time_point m_crash_window_start;
    std::string m_exec_path;
    std::string m_config_path;
    std::string m_log_path;
    bool m_stopping{false};

    std::function<void(int)> m_on_exit;
    std::mutex m_mutex;
};
