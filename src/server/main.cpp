#include <iostream>
#include <string>
#include <memory>
#include <vector>
#include <conio.h> // For _getch()
#include <limits> // For numeric_limits

#define ASIO_STANDALONE
#include <asio.hpp>

#include "protocol.h"
#include "ui.h"

using asio::ip::tcp;

class ServerConfig {
public:
    static std::string upstream_ip;
    static int upstream_port;
    static bool use_upstream;
};

std::string ServerConfig::upstream_ip = "127.0.0.1";
int ServerConfig::upstream_port = 10808;
bool ServerConfig::use_upstream = false;

class ProxySession : public std::enable_shared_from_this<ProxySession> {
public:
    ProxySession(tcp::socket socket, asio::io_context& io_context)
        : client_socket_(std::move(socket)), 
          target_socket_(io_context),
          resolver_(io_context) {
    }

    void start() {
        do_handshake();
    }

private:
    void do_handshake() {
        auto self(shared_from_this());
        // Read our custom packet: [Magic 0xBE][Cmd 0x01][Port 2B][Len 1B][Host...]
        // Min header size: 5 bytes
        
        client_socket_.async_read_some(asio::buffer(header_, 5),
            [this, self](std::error_code ec, std::size_t length) {
                if (!ec && length == 5) {
                    if ((uint8_t)header_[0] != 0xBE) {
                        lanproxy::ui::error("Invalid Magic Byte");
                        return;
                    }
                    
                    uint16_t port = ntohs(*((uint16_t*)(header_ + 2)));
                    uint8_t host_len = (uint8_t)header_[4];
                    
                    client_socket_.async_read_some(asio::buffer(host_buffer_, host_len),
                        [this, self, port](std::error_code ec, std::size_t length) {
                            if (!ec) {
                                std::string host(host_buffer_, length);
                                lanproxy::ui::info("Proxying request to: " + host + ":" + std::to_string(port));
                                
                                if (ServerConfig::use_upstream) {
                                    do_connect_upstream(host, port);
                                } else {
                                    do_resolve_and_connect(host, std::to_string(port));
                                }
                            }
                        });
                }
            });
    }

    // --- Direct Connection Logic ---
    void do_resolve_and_connect(const std::string& host, const std::string& port) {
        auto self(shared_from_this());
        resolver_.async_resolve(host, port,
            [this, self](std::error_code ec, tcp::resolver::results_type results) {
                if (!ec) {
                    asio::async_connect(target_socket_, results,
                        [this, self](std::error_code ec, tcp::endpoint) {
                            if (!ec) {
                                send_client_success();
                            } else {
                                lanproxy::ui::error("Failed to connect to target: " + ec.message());
                                send_client_failure();
                            }
                        });
                } else {
                    lanproxy::ui::error("Failed to resolve target: " + ec.message());
                    send_client_failure();
                }
            });
    }

    // --- Upstream Proxy Logic (SOCKS5) ---
    void do_connect_upstream(const std::string& target_host, uint16_t target_port) {
        auto self(shared_from_this());
        // Resolve Upstream Proxy IP
        resolver_.async_resolve(ServerConfig::upstream_ip, std::to_string(ServerConfig::upstream_port),
            [this, self, target_host, target_port](std::error_code ec, tcp::resolver::results_type results) {
                if (!ec) {
                    asio::async_connect(target_socket_, results,
                        [this, self, target_host, target_port](std::error_code ec, tcp::endpoint) {
                            if (!ec) {
                                // Connected to Upstream Proxy, start SOCKS5 handshake
                                upstream_handshake(target_host, target_port);
                            } else {
                                lanproxy::ui::error("Failed to connect to Upstream Proxy: " + ec.message());
                                send_client_failure();
                            }
                        });
                } else {
                    lanproxy::ui::error("Failed to resolve Upstream Proxy: " + ec.message());
                    send_client_failure();
                }
            });
    }

    void upstream_handshake(const std::string& target_host, uint16_t target_port) {
        auto self(shared_from_this());
        // SOCKS5 Hello: [0x05, 0x01, 0x00] (Ver, NMethods, NoAuth)
        static uint8_t handshake[] = {0x05, 0x01, 0x00};
        asio::async_write(target_socket_, asio::buffer(handshake, 3),
            [this, self, target_host, target_port](std::error_code ec, std::size_t) {
                if (!ec) {
                    // Read Hello Reply
                    target_socket_.async_read_some(asio::buffer(upstream_buffer_, 2),
                        [this, self, target_host, target_port](std::error_code ec, std::size_t length) {
                            if (!ec && length >= 2 && upstream_buffer_[0] == 0x05 && upstream_buffer_[1] == 0x00) {
                                // Handshake OK, Send Connect Request
                                upstream_connect_req(target_host, target_port);
                            } else {
                                lanproxy::ui::error("Upstream Proxy Handshake Failed");
                                send_client_failure();
                            }
                        });
                } else {
                    send_client_failure();
                }
            });
    }

    void upstream_connect_req(const std::string& target_host, uint16_t target_port) {
        auto self(shared_from_this());
        // Request: [0x05, 0x01, 0x00, 0x03 (Domain), Len, Domain..., Port(2)]
        std::vector<uint8_t> req;
        req.push_back(0x05); // Ver
        req.push_back(0x01); // CMD: Connect
        req.push_back(0x00); // RSV
        req.push_back(0x03); // ATYP: Domain
        req.push_back((uint8_t)target_host.size());
        for (char c : target_host) req.push_back(c);
        
        uint16_t p = htons(target_port);
        req.push_back(((uint8_t*)&p)[0]);
        req.push_back(((uint8_t*)&p)[1]);

        asio::async_write(target_socket_, asio::buffer(req),
            [this, self](std::error_code ec, std::size_t) {
                if (!ec) {
                    // Read Connect Reply (min 10 bytes usually)
                    // We just read enough to check status
                    target_socket_.async_read_some(asio::buffer(upstream_buffer_, 1024),
                        [this, self](std::error_code ec, std::size_t length) {
                            if (!ec && length >= 4) {
                                if (upstream_buffer_[1] == 0x00) {
                                    // SOCKS5 Success
                                    send_client_success();
                                } else {
                                    lanproxy::ui::error("Upstream Proxy Connect Failed: Rep=" + std::to_string(upstream_buffer_[1]));
                                    send_client_failure();
                                }
                            } else {
                                send_client_failure();
                            }
                        });
                } else {
                    send_client_failure();
                }
            });
    }

    // --- Helper Functions ---
    void send_client_success() {
        auto self(shared_from_this());
        auto status = std::make_shared<uint8_t>(0x00);
        asio::async_write(client_socket_, asio::buffer(status.get(), 1),
            [this, self, status](std::error_code ec, std::size_t) {
                if (!ec) {
                    lanproxy::ui::success("Connected to target, tunnel started");
                    start_tunnel(target_socket_);
                } else {
                    close_sockets();
                }
            });
    }

    void send_client_failure() {
        auto self(shared_from_this());
        auto status = std::make_shared<uint8_t>(0x01);
        asio::async_write(client_socket_, asio::buffer(status.get(), 1),
            [this, self, status](std::error_code, std::size_t) {
                close_sockets();
            });
    }

    void start_tunnel(tcp::socket& remote) {
        do_upstream(remote);
        do_downstream(remote);
    }

    void do_upstream(tcp::socket& remote) {
        auto self(shared_from_this());
        client_socket_.async_read_some(asio::buffer(client_buffer_),
            [this, self, &remote](std::error_code ec, std::size_t length) {
                if (!ec) {
                    asio::async_write(remote, asio::buffer(client_buffer_, length),
                        [this, self, &remote](std::error_code ec, std::size_t) {
                            if (!ec) do_upstream(remote);
                        });
                } else {
                    close_sockets();
                }
            });
    }

    void do_downstream(tcp::socket& remote) {
        auto self(shared_from_this());
        remote.async_read_some(asio::buffer(target_buffer_),
            [this, self, &remote](std::error_code ec, std::size_t length) {
                if (!ec) {
                    asio::async_write(client_socket_, asio::buffer(target_buffer_, length),
                        [this, self, &remote](std::error_code ec, std::size_t) {
                            if (!ec) do_downstream(remote);
                        });
                } else {
                    close_sockets();
                }
            });
    }

    void close_sockets() {
        if (client_socket_.is_open()) {
            std::error_code ec;
            client_socket_.shutdown(tcp::socket::shutdown_both, ec);
            client_socket_.close(ec);
        }
        if (target_socket_.is_open()) {
            std::error_code ec;
            target_socket_.shutdown(tcp::socket::shutdown_both, ec);
            target_socket_.close(ec);
        }
    }

    tcp::socket client_socket_;
    tcp::socket target_socket_;
    tcp::resolver resolver_;
    char header_[5];
    char host_buffer_[256];
    char client_buffer_[8192];
    char target_buffer_[8192];
    uint8_t upstream_buffer_[1024];
};

class ProxyServer {
public:
    ProxyServer(asio::io_context& io_context, short port)
        : io_context_(io_context),
          acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
        lanproxy::ui::enable_ansi();
        lanproxy::ui::print_banner("SERVER");
        lanproxy::ui::info("Listening on port " + std::to_string(port));
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(
            [this](std::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<ProxySession>(std::move(socket), io_context_)->start();
                }
                do_accept();
            });
    }

    asio::io_context& io_context_;
    tcp::acceptor acceptor_;
};

int main(int argc, char* argv[]) {
    try {
        asio::io_context io_context;
        int port = 4900;
        
        if (argc >= 2) port = std::stoi(argv[1]);
        if (argc >= 4) {
            ServerConfig::use_upstream = true;
            ServerConfig::upstream_ip = argv[2];
            ServerConfig::upstream_port = std::stoi(argv[3]);
            lanproxy::ui::info("Upstream Proxy: " + ServerConfig::upstream_ip + ":" + std::to_string(ServerConfig::upstream_port));
        } else {
            // Interactive Config if not provided
            std::cout << "\n    Use Upstream Proxy (e.g., v2rayN 127.0.0.1:10808)? [y/N]: ";
            char c = _getch();
            if (c == 'y' || c == 'Y') {
                ServerConfig::use_upstream = true;
                std::cout << "Y\n";
                // Flush stdin buffer before getline
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

                std::cout << "    Upstream IP [127.0.0.1]: ";
                std::string ip;
                std::getline(std::cin, ip);
                // Trim whitespace
                ip.erase(ip.find_last_not_of(" \n\r\t") + 1);
                if (!ip.empty()) ServerConfig::upstream_ip = ip;
                
                std::cout << "    Upstream Port [10808]: ";
                std::string p;
                std::getline(std::cin, p);
                // Trim whitespace
                p.erase(p.find_last_not_of(" \n\r\t") + 1);
                if (!p.empty()) {
                    try {
                        ServerConfig::upstream_port = std::stoi(p);
                    } catch (...) {}
                }
                
                lanproxy::ui::info("Upstream Proxy Enabled: " + ServerConfig::upstream_ip + ":" + std::to_string(ServerConfig::upstream_port));
            } else {
                std::cout << "N\n";
            }
        }

        ProxyServer server(io_context, port);
        io_context.run();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}
