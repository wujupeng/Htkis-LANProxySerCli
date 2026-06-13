#include "v2rayn_manager/v2rayn_process.h"
#include "log_monitor/structured_logger.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

v2rayn_process& v2rayn_process::instance() {
    static v2rayn_process inst;
    return inst;
}

bool v2rayn_process::start(const std::string& exec_path,
                             const std::string& config_path,
                             const std::string& log_path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_exec_path = exec_path;
    m_config_path = config_path;
    m_log_path = log_path;
    return do_start();
}

bool v2rayn_process::do_start() {
    if (m_status == v2rayn_status::running) return true;

    m_status = v2rayn_status::starting;
    m_stopping = false;

    pid_t pid = fork();
    if (pid < 0) {
        m_status = v2rayn_status::failed;
        structured_logger::instance().error("v2rayn_process",
            "fork() failed: " + std::string(strerror(errno)));
        return false;
    }

    if (pid == 0) {
        for (int fd = 3; fd < 1024; ++fd) {
            int flags = fcntl(fd, F_GETFD);
            if (flags != -1) fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
        }

        if (!m_log_path.empty()) {
            int log_fd = open(m_log_path.c_str(),
                              O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (log_fd >= 0) {
                dup2(log_fd, STDOUT_FILENO);
                dup2(log_fd, STDERR_FILENO);
                if (log_fd > 2) close(log_fd);
            }
        }

        execl(m_exec_path.c_str(), m_exec_path.c_str(),
              "run", "-c", m_config_path.c_str(), nullptr);
        _exit(127);
    }

    m_pid = pid;
    m_status = v2rayn_status::running;

    structured_logger::instance().info("v2rayn_process",
        "v2rayN started, pid=" + std::to_string(pid));

    struct child_watcher {
        static void handler(int sig) {
            int status;
            pid_t p = waitpid(-1, &status, WNOHANG);
            if (p > 0) {
                v2rayn_process::instance().on_child_exit();
            }
        }
    };
    struct sigaction sa{};
    sa.sa_handler = child_watcher::handler;
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, nullptr);

    return true;
}

bool v2rayn_process::do_stop() {
    m_stopping = true;
    int p = m_pid.load();
    if (p <= 0) return true;

    kill(p, SIGTERM);

    for (int i = 0; i < 30; ++i) {
        int status;
        pid_t ret = waitpid(p, &status, WNOHANG);
        if (ret == p) {
            m_pid = 0;
            m_status = v2rayn_status::stopped;
            structured_logger::instance().info("v2rayn_process", "v2rayN stopped gracefully");
            return true;
        }
        usleep(100000);
    }

    kill(p, SIGKILL);
    int status;
    waitpid(p, &status, 0);
    m_pid = 0;
    m_status = v2rayn_status::stopped;
    structured_logger::instance().warn("v2rayn_process", "v2rayN force killed");
    return true;
}

bool v2rayn_process::stop() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return do_stop();
}

bool v2rayn_process::restart() {
    std::lock_guard<std::mutex> lock(m_mutex);
    do_stop();
    return do_start();
}

void v2rayn_process::on_child_exit() {
    if (m_stopping) return;

    m_pid = 0;
    m_crash_count++;

    auto now = std::chrono::steady_clock::now();
    if (m_crash_count == 1) {
        m_crash_window_start = now;
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(
        now - m_crash_window_start).count();

    if (elapsed >= 5) {
        m_crash_count = 1;
        m_crash_window_start = now;
    }

    if (m_crash_count >= 3) {
        m_status = v2rayn_status::failed;
        structured_logger::instance().error("v2rayn_process",
            "v2rayN crashed 3 times in 5 minutes, stopping auto-restart");
        if (m_on_exit) m_on_exit(m_crash_count);
        return;
    }

    structured_logger::instance().warn("v2rayn_process",
        "v2rayN exited unexpectedly, auto-restarting (crash #" +
        std::to_string(m_crash_count) + ")");

    do_start();
}

void v2rayn_process::reset_crash_window() {
    m_crash_count = 0;
}

void v2rayn_process::set_on_exit(std::function<void(int)> cb) {
    m_on_exit = std::move(cb);
}
