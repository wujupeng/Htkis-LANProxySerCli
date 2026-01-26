#pragma once
#include <asio.hpp>
#include <memory>
#include <string>
#include <vector>
#include <array>
#include <thread>
#include <atomic>

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(asio::ip::tcp::socket socket);
    void start();

private:
    void do_handshake();
    void do_auth();
    void do_request();
    void do_resolve(const std::string& host, uint16_t port);
    void do_connect(const asio::ip::tcp::resolver::results_type& endpoints);
    void do_stream();
    
    // Read from client, write to remote
    void do_read_client();
    void do_write_remote(std::size_t length);
    
    // Read from remote, write to client
    void do_read_remote();
    void do_write_client(std::size_t length);

    asio::ip::tcp::socket m_client_socket;
    asio::ip::tcp::socket m_remote_socket;
    asio::ip::tcp::resolver m_resolver;
    
    std::array<uint8_t, 8192> m_client_buffer;
    std::array<uint8_t, 8192> m_remote_buffer;
};

class Server {
public:
    Server(asio::io_context& io_context, short port);
    // Destructor to ensure acceptor is closed?
    ~Server() {
        // Acceptor close handled by io_context usually, or manually if needed
    }

    void stop() {
        m_acceptor.close();
    }

private:
    void do_accept();

    asio::ip::tcp::acceptor m_acceptor;
};

// Helper class to manage Server lifecycle for GUI
class ServerApp {
public:
    static ServerApp& getInstance() {
        static ServerApp instance;
        return instance;
    }

    void start(int port) {
        if (m_running) return;
        m_running = true;
        m_port = port;
        
        m_thread = std::thread([this]() {
            try {
                m_io_context = std::make_unique<asio::io_context>();
                m_server = std::make_unique<Server>(*m_io_context, m_port);
                // Keep the io_context running even if no work
                auto work_guard = asio::make_work_guard(*m_io_context);
                m_io_context->run();
            } catch (...) {
                m_running = false;
            }
        });
    }

    void stop() {
        if (!m_running) return;
        if (m_io_context) {
            m_io_context->stop();
        }
        if (m_thread.joinable()) {
            m_thread.join();
        }
        m_server.reset();
        m_io_context.reset();
        m_running = false;
    }

    bool isRunning() const { return m_running; }
    int getPort() const { return m_port; }

private:
    ServerApp() = default;
    ~ServerApp() { stop(); }

    std::atomic<bool> m_running{false};
    int m_port{10800};
    
    std::unique_ptr<asio::io_context> m_io_context;
    std::unique_ptr<Server> m_server;
    std::thread m_thread;
};
