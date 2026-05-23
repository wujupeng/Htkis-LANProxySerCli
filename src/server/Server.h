#pragma once

#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif

#include <asio.hpp>
#include <memory>
#include <string>
#include <vector>
#include <array>
#include <thread>
#include <atomic>
#include <optional>

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(asio::ip::tcp::socket socket);
    void start();

private:
    // Protocol detection
    void do_handshake();
    bool is_http_connect(const uint8_t* data, std::size_t length);
    void handle_http_connect(std::size_t length);
    void handle_socks5_handshake(std::size_t length);

    // SOCKS5 flow
    void do_auth();
    void do_request();

    // HTTP CONNECT flow
    void do_http_auth();
    void do_http_request();

    // Common
    void do_resolve(const std::string& host, uint16_t port);
    void do_connect(const asio::ip::tcp::resolver::results_type& endpoints);
    void do_stream();

    // Read from client, write to remote
    void do_read_client();
    void do_write_remote(std::size_t length);

    // Read from remote, write to client
    void do_read_remote();
    void do_write_client(std::size_t length);

    // Session close with logging
    void close_session(const std::string& reason);

    asio::ip::tcp::socket m_client_socket;
    asio::ip::tcp::socket m_remote_socket;
    asio::ip::tcp::resolver m_resolver;

    std::array<uint8_t, 8192> m_client_buffer;
    std::array<uint8_t, 8192> m_remote_buffer;

    // Target info (used by both HTTP and SOCKS5)
    bool m_is_http{false};
    std::string m_target_host;
    uint16_t m_target_port{0};

    // HTTP CONNECT auth state
    std::string m_http_username;
    std::string m_http_password;

    // Session tracking
    std::string m_username;         // authenticated username (both protocols)
    std::string m_client_addr;      // client IP:port
    std::atomic<uint64_t> m_bytes_up{0};    // bytes sent to remote
    std::atomic<uint64_t> m_bytes_down{0};  // bytes sent to client
    std::time_t m_start_time{0};    // session start time
    bool m_tunnel_active{false};    // tunnel was established
};

class Server {
public:
    Server(asio::io_context& io_context, short port);
    void stop();

private:
    void do_accept();

    asio::ip::tcp::acceptor m_acceptor;
    bool m_running{true};
};

// Helper class to manage Server lifecycle for GUI
class ServerApp {
public:
    static ServerApp& getInstance() {
        static ServerApp instance;
        return instance;
    }

    void start(int port);
    void stop();

    bool isRunning() const { return m_running; }
    int getPort() const { return m_port; }

private:
    ServerApp() = default;
    ~ServerApp() { stop(); }

    std::atomic<bool> m_running{false};
    int m_port{10800};

    std::unique_ptr<asio::io_context> m_io_context;
    std::optional<asio::executor_work_guard<asio::io_context::executor_type>> m_work_guard;
    std::unique_ptr<Server> m_server;
    std::thread m_thread;
};
