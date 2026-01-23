#include "Server.h"
#include "Protocol.h"
#include "UserManager.h"
#include <iostream>

using asio::ip::tcp;

Session::Session(tcp::socket socket)
    : m_client_socket(std::move(socket)),
      m_remote_socket(m_client_socket.get_executor()),
      m_resolver(m_client_socket.get_executor()) {
}

void Session::start() {
    do_handshake();
}

void Session::do_handshake() {
    auto self(shared_from_this());
    m_client_socket.async_read_some(asio::buffer(m_client_buffer),
        [this, self](std::error_code ec, std::size_t length) {
            if (!ec) {
                if (length < 2 || m_client_buffer[0] != LanProxy::SOCKS_VERSION) {
                    return; // Invalid version
                }

                uint8_t nmethods = m_client_buffer[1];
                // Check if username/password auth is supported by client (0x02)
                bool support_auth = false;
                for (size_t i = 0; i < nmethods; ++i) {
                    if (m_client_buffer[2 + i] == LanProxy::AUTH_USERPASS) {
                        support_auth = true;
                        break;
                    }
                }

                // Prepare response
                auto response = std::make_shared<std::vector<uint8_t>>(std::initializer_list<uint8_t>{LanProxy::SOCKS_VERSION, LanProxy::AUTH_USERPASS});
                if (!support_auth) {
                    (*response)[1] = LanProxy::AUTH_NO_ACCEPTABLE;
                }

                asio::async_write(m_client_socket, asio::buffer(*response),
                    [this, self, response, support_auth](std::error_code ec, std::size_t /*length*/) {
                        if (!ec) {
                            if (support_auth) {
                                do_auth();
                            }
                        }
                    });
            }
        });
}

void Session::do_auth() {
    auto self(shared_from_this());
    m_client_socket.async_read_some(asio::buffer(m_client_buffer),
        [this, self](std::error_code ec, std::size_t length) {
            if (!ec) {
                if (length < 2 || m_client_buffer[0] != 0x01) { // Subnegotiation version is 0x01
                    return;
                }
                
                uint8_t ulen = m_client_buffer[1];
                if (length < 2 + ulen + 1) return;
                std::string username((char*)&m_client_buffer[2], ulen);
                
                uint8_t plen = m_client_buffer[2 + ulen];
                if (length < 2 + ulen + 1 + plen) return;
                std::string password((char*)&m_client_buffer[2 + ulen + 1], plen);

                bool authenticated = UserManager::getInstance().authenticate(username, password);
                
                auto response = std::make_shared<std::vector<uint8_t>>(std::initializer_list<uint8_t>{0x01, authenticated ? (uint8_t)0x00 : (uint8_t)0x01});
                
                asio::async_write(m_client_socket, asio::buffer(*response),
                    [this, self, response, authenticated](std::error_code ec, std::size_t /*length*/) {
                        if (!ec && authenticated) {
                            do_request();
                        }
                    });
            }
        });
}

void Session::do_request() {
    auto self(shared_from_this());
    m_client_socket.async_read_some(asio::buffer(m_client_buffer),
        [this, self](std::error_code ec, std::size_t length) {
            if (!ec) {
                if (length < 4 || m_client_buffer[0] != LanProxy::SOCKS_VERSION || 
                    m_client_buffer[1] != LanProxy::CMD_CONNECT) {
                    // Only support CONNECT
                    return;
                }

                uint8_t atyp = m_client_buffer[3];
                std::string host;
                uint16_t port = 0;
                
                // Parse address
                if (atyp == LanProxy::ATYP_IPV4) {
                    if (length < 10) return;
                    asio::ip::address_v4::bytes_type bytes;
                    std::copy_n(&m_client_buffer[4], 4, bytes.begin());
                    host = asio::ip::make_address_v4(bytes).to_string();
                    port = (m_client_buffer[8] << 8) | m_client_buffer[9];
                } else if (atyp == LanProxy::ATYP_DOMAIN) {
                    uint8_t domain_len = m_client_buffer[4];
                    if (length < 5 + domain_len + 2) return;
                    host = std::string((char*)&m_client_buffer[5], domain_len);
                    port = (m_client_buffer[5 + domain_len] << 8) | m_client_buffer[5 + domain_len + 1];
                } else if (atyp == LanProxy::ATYP_IPV6) {
                     // Simplified IPv6 support (not fully implemented in parsing for brevity but structure is here)
                     // Assuming IPv4/Domain mostly for now as per requirements usually implies typical web usage
                     // To be safe, let's reject IPv6 for this simple implementation or implement if needed.
                     // Implementing basic parsing:
                     if (length < 22) return;
                     asio::ip::address_v6::bytes_type bytes;
                     std::copy_n(&m_client_buffer[4], 16, bytes.begin());
                     host = asio::ip::make_address_v6(bytes).to_string();
                     port = (m_client_buffer[20] << 8) | m_client_buffer[21];
                } else {
                    return;
                }

                do_resolve(host, port);
            }
        });
}

void Session::do_resolve(const std::string& host, uint16_t port) {
    auto self(shared_from_this());
    m_resolver.async_resolve(host, std::to_string(port),
        [this, self](std::error_code ec, tcp::resolver::results_type results) {
            if (!ec) {
                do_connect(results);
            } else {
                // Send failure
                // ...
            }
        });
}

void Session::do_connect(const tcp::resolver::results_type& endpoints) {
    auto self(shared_from_this());
    asio::async_connect(m_remote_socket, endpoints,
        [this, self](std::error_code ec, tcp::endpoint /*endpoint*/) {
            if (!ec) {
                // Send success response to client
                // 0x05 0x00 0x00 0x01 [0.0.0.0] [0]
                auto response = std::make_shared<std::vector<uint8_t>>(std::initializer_list<uint8_t>{
                    LanProxy::SOCKS_VERSION, 0x00, 0x00, LanProxy::ATYP_IPV4,
                    0, 0, 0, 0, 0, 0
                });
                
                asio::async_write(m_client_socket, asio::buffer(*response),
                    [this, self, response](std::error_code ec, std::size_t /*length*/) {
                        if (!ec) {
                            do_stream();
                        }
                    });
            }
        });
}

void Session::do_stream() {
    do_read_client();
    do_read_remote();
}

void Session::do_read_client() {
    auto self(shared_from_this());
    m_client_socket.async_read_some(asio::buffer(m_client_buffer),
        [this, self](std::error_code ec, std::size_t length) {
            if (!ec) {
                do_write_remote(length);
            } else {
                // Close both on error
                m_remote_socket.close();
            }
        });
}

void Session::do_write_remote(std::size_t length) {
    auto self(shared_from_this());
    asio::async_write(m_remote_socket, asio::buffer(m_client_buffer, length),
        [this, self](std::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                do_read_client();
            } else {
                m_client_socket.close();
            }
        });
}

void Session::do_read_remote() {
    auto self(shared_from_this());
    m_remote_socket.async_read_some(asio::buffer(m_remote_buffer),
        [this, self](std::error_code ec, std::size_t length) {
            if (!ec) {
                do_write_client(length);
            } else {
                m_client_socket.close();
            }
        });
}

void Session::do_write_client(std::size_t length) {
    auto self(shared_from_this());
    asio::async_write(m_client_socket, asio::buffer(m_remote_buffer, length),
        [this, self](std::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                do_read_remote();
            } else {
                m_remote_socket.close();
            }
        });
}

// Server implementation
Server::Server(asio::io_context& io_context, short port)
    : m_acceptor(io_context, tcp::endpoint(tcp::v4(), port)) {
    do_accept();
}

void Server::do_accept() {
    m_acceptor.async_accept(
        [this](std::error_code ec, tcp::socket socket) {
            if (!ec) {
                std::make_shared<Session>(std::move(socket))->start();
            }
            do_accept();
        });
}
