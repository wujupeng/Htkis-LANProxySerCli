#include "Client.h"
#include "Protocol.h"
#include <iostream>
#include <thread>

int main(int argc, char* argv[]) {
    ClientConfig config;
    config.local_port = LanProxy::DEFAULT_CLIENT_PORT;

    std::cout << "LanProxy Client\n";
    std::cout << "Enter Server IP: ";
    std::cin >> config.server_ip;
    config.server_port = LanProxy::DEFAULT_SERVER_PORT; // Assume default
    
    std::cout << "Enter Username: ";
    std::cin >> config.username;
    
    std::cout << "Enter Password: ";
    std::cin >> config.password;

    try {
        asio::io_context io_context;
        Client client(io_context, config);
        
        std::cout << "Client started on local port " << config.local_port << "\n";
        std::cout << "Please configure your browser/system to use SOCKS5 proxy at 127.0.0.1:" << config.local_port << "\n";
        
        io_context.run();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}
