#pragma once
#include <asio.hpp>
#include <memory>
#include <string>
#include <array>
#include <thread>
#include <atomic>

struct ClientConfig {
    std::string server_ip;
    uint16_t server_port;
    std::string username;
    std::string password;
    uint16_t local_port;
};

class ClientSession : public std::enable_shared_from_this<ClientSession> {
public:
    ClientSession(asio::ip::tcp::socket socket, const ClientConfig& config);
    void start();

private:
    void do_read_local_handshake();
    void connect_to_remote();
    void do_remote_handshake();
    void do_remote_auth();
    void do_forward_request();
    void do_stream();
    
    void do_read_local();
    void do_write_remote(std::size_t length);
    void do_read_remote();
    void do_write_local(std::size_t length);

    asio::ip::tcp::socket m_local_socket;
    asio::ip::tcp::socket m_remote_socket;
    asio::ip::tcp::resolver m_resolver;
    ClientConfig m_config;
    
    std::array<uint8_t, 8192> m_local_buffer;
    std::array<uint8_t, 8192> m_remote_buffer;
    
    // Buffer to store the request from local app until we are authenticated with remote
    std::vector<uint8_t> m_pending_request; 
};

class Client {
public:
    Client(asio::io_context& io_context, const ClientConfig& config);

    void stop() {
        m_acceptor.close();
    }

private:
    void do_accept();

    asio::ip::tcp::acceptor m_acceptor;
    ClientConfig m_config;
};

// Helper class for Client GUI
class ClientApp {
public:
    static ClientApp& getInstance() {
        static ClientApp instance;
        return instance;
    }

    void start(const ClientConfig& config) {
        if (m_running) return;
        m_running = true;
        m_config = config;
        
        m_thread = std::thread([this]() {
            try {
                m_io_context = std::make_unique<asio::io_context>();
                m_client = std::make_unique<Client>(*m_io_context, m_config);
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
        m_client.reset();
        m_io_context.reset();
        m_running = false;
    }

    bool isRunning() const { return m_running; }
    ClientConfig getConfig() const { return m_config; }

private:
    ClientApp() = default;
    ~ClientApp() { stop(); }

    std::atomic<bool> m_running{false};
    ClientConfig m_config;
    
    std::unique_ptr<asio::io_context> m_io_context;
    std::unique_ptr<Client> m_client;
    std::thread m_thread;
};
