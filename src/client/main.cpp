#include <iostream>
#include <string>
#include <memory>
#include <vector>

#define ASIO_STANDALONE
#include <asio.hpp>

#include "protocol.h"

using asio::ip::tcp;

class ProxyClient {
public:
    ProxyClient(asio::io_context& io_context, 
                const std::string& server_host, short server_port,
                const std::string& local_host, short local_port)
        : io_context_(io_context),
          server_endpoint_(asio::ip::make_address(server_host), server_port),
          local_endpoint_(asio::ip::make_address(local_host), local_port) {
        do_connect_control();
    }

private:
    void do_connect_control() {
        auto socket = std::make_shared<tcp::socket>(io_context_);
        socket->async_connect(server_endpoint_, [this, socket](std::error_code ec) {
            if (!ec) {
                std::cout << "Connected to Control Server!" << std::endl;
                // Start reading commands from server
                // If receive MSG_CONNECT -> create_bridge()
            } else {
                std::cout << "Connect failed: " << ec.message() << std::endl;
                // Retry logic...
            }
        });
    }

    void create_bridge(const std::string& session_id) {
        // 1. Connect to Local App
        // 2. Connect to Server Data Port
        // 3. Bridge them
    }

    asio::io_context& io_context_;
    tcp::endpoint server_endpoint_;
    tcp::endpoint local_endpoint_;
};

int main(int argc, char* argv[]) {
    try {
        if (argc != 5) {
            std::cerr << "Usage: client <server_ip> <server_port> <local_ip> <local_port>\n";
            std::cerr << "Example: client 127.0.0.1 4900 127.0.0.1 80\n";
            // return 1; 
            // For demo purpose, continue with defaults or exit
        }

        asio::io_context io_context;
        
        std::string server_ip = "127.0.0.1";
        int server_port = 4900;
        std::string local_ip = "127.0.0.1";
        int local_port = 80;

        ProxyClient client(io_context, server_ip, server_port, local_ip, local_port);

        std::cout << "LanProxy Client started." << std::endl;
        io_context.run();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}
