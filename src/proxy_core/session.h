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
#include <random>
#include <nlohmann/json.hpp>

class route_engine;
class user_manager;

enum class session_route_action { direct, proxy };

class session : public std::enable_shared_from_this<session> {
public:
    session(asio::ip::tcp::socket socket, route_engine& router, user_manager& users);
    void start();

private:
    void do_handshake();
    bool is_http_connect(const uint8_t* data, std::size_t length);
    void handle_http_connect(std::size_t length);
    void handle_socks5_handshake(std::size_t length);

    void do_auth();
    void do_request();
    void do_http_auth();
    void do_http_request();

    void do_route_decision();
    void do_resolve(const std::string& host, uint16_t port);
    void do_connect_direct(const asio::ip::tcp::resolver::results_type& endpoints);
    void do_connect_via_v2rayn();
    void do_socks5_client_handshake(const std::string& host, uint16_t port);
    void do_stream();

    void do_read_client();
    void do_write_remote(std::size_t length);
    void do_read_remote();
    void do_write_client(std::size_t length);

    void close_session(const std::string& reason);
    std::string generate_session_id();

    asio::ip::tcp::socket m_client_socket;
    asio::ip::tcp::socket m_remote_socket;
    asio::ip::tcp::resolver m_resolver;

    std::array<uint8_t, 8192> m_client_buffer;
    std::array<uint8_t, 8192> m_remote_buffer;

    bool m_is_http{false};
    std::string m_target_host;
    uint16_t m_target_port{0};
    std::string m_resolved_ip;

    std::string m_http_username;
    std::string m_http_password;

    std::string m_session_id;
    std::string m_username;
    std::string m_client_addr;
    std::atomic<uint64_t> m_bytes_up{0};
    std::atomic<uint64_t> m_bytes_down{0};
    std::time_t m_start_time{0};
    bool m_tunnel_active{false};
    session_route_action m_route_action{session_route_action::direct};
    std::string m_selected_node_tag;
    int m_failover_retry_count{0};
    bool m_admitted{false};

    route_engine& m_router;
    user_manager& m_users;
};

class proxy_server {
public:
    proxy_server(asio::io_context& io_context, short port,
                 route_engine& router, user_manager& users);
    void stop();

private:
    void do_accept();

    asio::ip::tcp::acceptor m_acceptor;
    bool m_running{true};
    route_engine& m_router;
    user_manager& m_users;
};

class server_app {
public:
    static server_app& instance() {
        static server_app inst;
        return inst;
    }

    void start(int port, int thread_count);
    void stop();

    bool is_running() const { return m_running; }
    int get_port() const { return m_port; }

private:
    server_app() = default;
    ~server_app() { stop(); }

    std::atomic<bool> m_running{false};
    int m_port{10800};

    std::unique_ptr<asio::io_context> m_io_context;
    std::optional<asio::executor_work_guard<asio::io_context::executor_type>> m_work_guard;
    std::unique_ptr<proxy_server> m_server;
    std::vector<std::thread> m_threads;
};
