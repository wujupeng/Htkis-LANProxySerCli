#include <iostream>
#include <string>
#include <memory>
#include <vector>
#include <thread>
#include <conio.h> // For _getch()
#include <sstream>

#define ASIO_STANDALONE
#include <asio.hpp>

#include "protocol.h"
#include "socks5.h"
#include "ui.h"

using asio::ip::tcp;

class ClientSession : public std::enable_shared_from_this<ClientSession> {
public:
    ClientSession(tcp::socket socket, asio::io_context& io_context, 
                  const std::string& server_host, short server_port)
        : client_socket_(std::move(socket)), 
          server_socket_(io_context),
          server_endpoint_(asio::ip::make_address(server_host), server_port),
          resolver_(io_context) {
    }

    void start() {
        do_peek();
    }

private:
    void do_peek() {
        auto self(shared_from_this());
        // Peek first 8 bytes to determine protocol
        client_socket_.async_read_some(asio::buffer(peek_buffer_),
            [this, self](std::error_code ec, std::size_t length) {
                if (!ec && length > 0) {
                    if (peek_buffer_[0] == 0x05) {
                        // SOCKS5
                        handle_socks5(length);
                    } else {
                        // Assume HTTP
                        handle_http(length);
                    }
                }
            });
    }

    // --- SOCKS5 Logic ---
    void handle_socks5(std::size_t initial_len) {
        // We already read 'initial_len' bytes into peek_buffer_
        // But for simplicity in this structure, we'll process what we have.
        // Handshake: [0x05, nmethods, methods...]
        if (initial_len >= 2 && peek_buffer_[0] == 0x05) {
            // Reply: No Auth (0x00)
            uint8_t resp[] = {0x05, 0x00};
            asio::async_write(client_socket_, asio::buffer(resp, 2),
                [this, self = shared_from_this()](std::error_code ec, std::size_t) {
                    if (!ec) do_socks5_request();
                });
        }
    }

    void do_socks5_request() {
        auto self(shared_from_this());
        // Read version, cmd, rsv, atyp (4 bytes)
        client_socket_.async_read_some(asio::buffer(data_, 4),
            [this, self](std::error_code ec, std::size_t length) {
                if (!ec && length == 4) {
                    uint8_t ver = data_[0];
                    uint8_t cmd = data_[1];
                    uint8_t atyp = data_[3];
                    
                    if (ver != 0x05) {
                         lanproxy::ui::error("SOCKS5: Invalid Version");
                         return;
                    }

                    if (cmd != lanproxy::socks5::CMD_CONNECT) {
                        lanproxy::ui::error("SOCKS5: Unsupported Command");
                        return;
                    }

                    if (atyp == lanproxy::socks5::ATYP_IPV4) {
                        client_socket_.async_read_some(asio::buffer(data_ + 4, 6),
                            [this, self](std::error_code ec, std::size_t) {
                                if (!ec) {
                                    asio::ip::address_v4 ip(ntohl(*((uint32_t*)(data_ + 4))));
                                    uint16_t port = ntohs(*((uint16_t*)(data_ + 8)));
                                    handle_target(ip.to_string(), port, true); // true = SOCKS5 mode
                                }
                            });
                    } else if (atyp == lanproxy::socks5::ATYP_DOMAIN) {
                        client_socket_.async_read_some(asio::buffer(data_ + 4, 1),
                            [this, self](std::error_code ec, std::size_t) {
                                if (!ec) {
                                    uint8_t len = data_[4];
                                    client_socket_.async_read_some(asio::buffer(data_ + 5, len + 2),
                                        [this, self, len](std::error_code ec, std::size_t) {
                                            if (!ec) {
                                                std::string domain((char*)data_ + 5, len);
                                                uint16_t port = ntohs(*((uint16_t*)(data_ + 5 + len)));
                                                handle_target(domain, port, true);
                                            }
                                        });
                                }
                            });
                    }
                }
            });
    }

    // --- HTTP Proxy Logic ---
    void handle_http(std::size_t initial_len) {
        // We have initial bytes. We need to read until \r\n\r\n to parse header.
        // For simplicity, append what we have to a string and read more if needed.
        http_buffer_.assign((char*)peek_buffer_, initial_len);
        check_http_header();
    }

    void check_http_header() {
        auto self(shared_from_this());
        if (http_buffer_.find("\r\n\r\n") != std::string::npos) {
            parse_http_connect();
        } else {
            // Read more
            client_socket_.async_read_some(asio::buffer(data_),
                [this, self](std::error_code ec, std::size_t length) {
                    if (!ec) {
                        http_buffer_.append((char*)data_, length);
                        check_http_header();
                    }
                });
        }
    }

    void parse_http_connect() {
        std::istringstream iss(http_buffer_);
        std::string method, url, version;
        iss >> method >> url >> version;

        if (method == "CONNECT") {
            // CONNECT host:port HTTP/1.1
            size_t colon = url.find(':');
            if (colon != std::string::npos) {
                std::string host = url.substr(0, colon);
                std::string port_str = url.substr(colon + 1);
                int port = std::stoi(port_str);
                
                handle_target(host, (uint16_t)port, false); // false = HTTP mode
            }
        } else {
            // Normal HTTP Proxy (GET http://...) not fully implemented for MVP
            // But we can parse host from URL or Host header
            // For now, just log and close
            lanproxy::ui::warn("HTTP Direct Proxy not fully supported yet (Only CONNECT)");
            client_socket_.close();
        }
    }

    // --- Common Target Handling ---
    void handle_target(const std::string& host, uint16_t port, bool is_socks5) {
        bool domestic = lanproxy::socks5::Router::is_domestic(host);
        
        if (domestic) {
            lanproxy::ui::traffic("DIRECT", host + ":" + std::to_string(port), false);
            do_resolve_and_connect_direct(host, std::to_string(port), is_socks5);
        } else {
            lanproxy::ui::traffic("PROXY ", host + ":" + std::to_string(port), true);
            do_connect_proxy(host, port, is_socks5);
        }
    }

    void do_resolve_and_connect_direct(const std::string& host, const std::string& port, bool is_socks5) {
        auto self(shared_from_this());
        resolver_.async_resolve(host, port,
            [this, self, is_socks5](std::error_code ec, tcp::resolver::results_type results) {
                if (!ec) {
                    asio::async_connect(server_socket_, results,
                        [this, self, is_socks5](std::error_code ec, tcp::endpoint) {
                            if (!ec) {
                                if (is_socks5) send_socks5_reply(0x00);
                                else send_http_connect_ok();
                                start_tunnel(server_socket_);
                            } else {
                                if (is_socks5) send_socks5_reply(0x04);
                                else client_socket_.close();
                            }
                        });
                } else {
                    if (is_socks5) send_socks5_reply(0x04);
                    else client_socket_.close();
                }
            });
    }

    void do_connect_proxy(const std::string& target_host, uint16_t target_port, bool is_socks5) {
        auto self(shared_from_this());
        server_socket_.async_connect(server_endpoint_,
            [this, self, target_host, target_port, is_socks5](std::error_code ec) {
                if (!ec) {
                    // Handshake with LanProxy Server (Custom Protocol)
                    std::string payload;
                    payload.push_back(0xBE); // Magic
                    payload.push_back(0x01); // CMD: CONNECT
                    
                    uint16_t p = htons(target_port);
                    payload.append((char*)&p, 2);
                    
                    uint8_t len = (uint8_t)target_host.size();
                    payload.push_back(len);
                    payload.append(target_host);

                    asio::async_write(server_socket_, asio::buffer(payload),
                        [this, self, is_socks5](std::error_code ec, std::size_t) {
                            if (!ec) {
                                wait_for_server_status(is_socks5);
                            }
                        });
                } else {
                    lanproxy::ui::error("Failed to connect to LanProxy Server: " + ec.message());
                    if (is_socks5) send_socks5_reply(0x04);
                    else client_socket_.close();
                }
            });
    }

    void wait_for_server_status(bool is_socks5) {
        auto self(shared_from_this());
        server_socket_.async_read_some(asio::buffer(server_status_buffer_, 1),
            [this, self, is_socks5](std::error_code ec, std::size_t length) {
                if (!ec && length == 1) {
                    if (server_status_buffer_[0] == 0x00) {
                        // Success
                        if (is_socks5) send_socks5_reply(0x00);
                        else send_http_connect_ok();
                        
                        // IMPORTANT: Start tunnel immediately, but do NOT block reading from server
                        // We must ensure 'remote_buffer_' is ready to receive data from server
                        // And we must also start reading from client.
                        start_tunnel(server_socket_);
                    } else {
                        // Failure
                        lanproxy::ui::error("Server failed to connect to target");
                        if (is_socks5) send_socks5_reply(0x04);
                        else client_socket_.close();
                    }
                } else {
                     lanproxy::ui::error("Failed to read server status");
                     close_sockets();
                }
            });
    }

    void send_socks5_reply(uint8_t rep) {
        uint8_t reply[] = {0x05, rep, 0x00, 0x01, 0, 0, 0, 0, 0, 0};
        asio::async_write(client_socket_, asio::buffer(reply, 10),
            [this, rep](std::error_code ec, std::size_t) {
                // Just send, no extra action needed here
            });
    }

    void send_http_connect_ok() {
        // Standard HTTP 200 OK for CONNECT
        // Note: No Content-Length or Transfer-Encoding for CONNECT response usually
        std::string response = "HTTP/1.1 200 Connection Established\r\n\r\n";
        asio::write(client_socket_, asio::buffer(response));
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
        remote.async_read_some(asio::buffer(remote_buffer_),
            [this, self, &remote](std::error_code ec, std::size_t length) {
                if (!ec) {
                    asio::async_write(client_socket_, asio::buffer(remote_buffer_, length),
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
        if (server_socket_.is_open()) {
            std::error_code ec;
            server_socket_.shutdown(tcp::socket::shutdown_both, ec);
            server_socket_.close(ec);
        }
    }

    tcp::socket client_socket_;
    tcp::socket server_socket_;
    tcp::endpoint server_endpoint_;
    tcp::resolver resolver_;
    
    uint8_t peek_buffer_[1024];
    uint8_t data_[1024];
    uint8_t client_buffer_[8192];
    uint8_t remote_buffer_[8192];
    uint8_t server_status_buffer_[1];
    std::string http_buffer_;
};

class ProxyClient {
public:
    ProxyClient(asio::io_context& io_context, 
                const std::string& server_host, short server_port,
                short local_port)
        : io_context_(io_context),
          acceptor_(io_context, tcp::endpoint(tcp::v4(), local_port)),
          server_host_(server_host),
          server_port_(server_port) {
        
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(
            [this](std::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<ClientSession>(std::move(socket), io_context_, server_host_, server_port_)->start();
                }
                do_accept();
            });
    }

    asio::io_context& io_context_;
    tcp::acceptor acceptor_;
    std::string server_host_;
    short server_port_;
};

// Simple TUI Logic
struct Config {
    std::string server_ip = "192.168.2.127";
    int server_port = 4900;
    int local_port = 1080;
};

void draw_ui(const Config& cfg) {
    system("cls"); // Windows clear screen
    lanproxy::ui::enable_ansi();
    lanproxy::ui::print_banner("CLIENT");
    
    std::cout << "\n    " << lanproxy::ui::WHITE << "CURRENT CONFIGURATION" << lanproxy::ui::RESET << "\n";
    std::cout << "    --------------------------------------------------------\n";
    std::cout << "    [1] Server IP   : " << lanproxy::ui::YELLOW << cfg.server_ip << lanproxy::ui::RESET << "\n";
    std::cout << "    [2] Server Port : " << lanproxy::ui::YELLOW << cfg.server_port << lanproxy::ui::RESET << "\n";
    std::cout << "    [3] Local Port  : " << lanproxy::ui::YELLOW << cfg.local_port << lanproxy::ui::RESET << "\n";
    std::cout << "    --------------------------------------------------------\n";
    std::cout << "    [ENTER] Start Proxy    [Q] Quit\n\n";
    std::cout << "    Select an option to modify: ";
}

int main(int argc, char* argv[]) {
    try {
        Config cfg;
        
        // If arguments provided, skip interactive mode
        if (argc >= 2) {
            cfg.server_ip = argv[1];
            if (argc >= 3) cfg.server_port = std::stoi(argv[2]);
            if (argc >= 4) cfg.local_port = std::stoi(argv[3]);
            
            asio::io_context io_context;
            lanproxy::ui::enable_ansi();
            lanproxy::ui::print_banner("CLIENT");
            lanproxy::ui::info("Auto-starting with provided args...");
            lanproxy::ui::info("SOCKS5/HTTP Server listening on port " + std::to_string(cfg.local_port));
            lanproxy::ui::info("Proxy Server: " + cfg.server_ip + ":" + std::to_string(cfg.server_port));
            
            ProxyClient client(io_context, cfg.server_ip, cfg.server_port, cfg.local_port);
            io_context.run();
            return 0;
        }

        // Interactive Mode
        while (true) {
            draw_ui(cfg);
            char ch = _getch();
            
            if (ch == 'q' || ch == 'Q') return 0;
            if (ch == 13) break; // Enter key

            if (ch == '1') {
                std::cout << "\n    New Server IP: ";
                std::cin >> cfg.server_ip;
            } else if (ch == '2') {
                std::cout << "\n    New Server Port: ";
                std::cin >> cfg.server_port;
            } else if (ch == '3') {
                std::cout << "\n    New Local Port: ";
                std::cin >> cfg.local_port;
            }
        }

        system("cls");
        lanproxy::ui::print_banner("CLIENT");
        lanproxy::ui::info("Starting Dual-Protocol (SOCKS5/HTTP) Proxy Service...");
        lanproxy::ui::info("Local Server listening on port " + std::to_string(cfg.local_port));
        lanproxy::ui::info("Remote Server: " + cfg.server_ip + ":" + std::to_string(cfg.server_port));
        
        std::cout << "\n    " << lanproxy::ui::GREEN << "CONFIGURATION GUIDE:" << lanproxy::ui::RESET << "\n";
        std::cout << "    " << lanproxy::ui::WHITE << "Windows Settings -> Network & internet -> Proxy" << lanproxy::ui::RESET << "\n";
        std::cout << "    " << lanproxy::ui::WHITE << "Proxy IP  : " << lanproxy::ui::CYAN << "127.0.0.1" << lanproxy::ui::RESET << "\n";
        std::cout << "    " << lanproxy::ui::WHITE << "Proxy Port: " << lanproxy::ui::CYAN << cfg.local_port << lanproxy::ui::RESET << "\n";
        std::cout << "    " << lanproxy::ui::WHITE << "--------------------------------------------------------" << lanproxy::ui::RESET << "\n\n";

        asio::io_context io_context;
        ProxyClient client(io_context, cfg.server_ip, cfg.server_port, cfg.local_port);
        io_context.run();

    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
        system("pause");
    }

    return 0;
}
