#include "Client.h"
#include "Protocol.h"
#include <iostream>

using asio::ip::tcp;

ClientSession::ClientSession(tcp::socket socket, const ClientConfig& config)
    : m_local_socket(std::move(socket)),
      m_remote_socket(m_local_socket.get_executor()),
      m_resolver(m_local_socket.get_executor()),
      m_config(config) {
}

void ClientSession::start() {
    do_read_local_handshake();
}

void ClientSession::do_read_local_handshake() {
    auto self(shared_from_this());
    m_local_socket.async_read_some(asio::buffer(m_local_buffer),
        [this, self](std::error_code ec, std::size_t length) {
            if (!ec) {
                if (length < 2 || m_local_buffer[0] != 0x05) {
                    return; // Invalid SOCKS5
                }
                
                // We accept NO AUTH for local connection
                auto response = std::make_shared<std::vector<uint8_t>>(std::initializer_list<uint8_t>{0x05, 0x00});
                asio::async_write(m_local_socket, asio::buffer(*response),
                    [this, self, response](std::error_code ec, std::size_t /*length*/) {
                        if (!ec) {
                            // Read the actual request from local app
                            m_local_socket.async_read_some(asio::buffer(m_local_buffer),
                                [this, self](std::error_code ec, std::size_t length) {
                                    if (!ec) {
                                        // Store this request to forward later
                                        m_pending_request.assign(m_local_buffer.begin(), m_local_buffer.begin() + length);
                                        // Now connect to remote server
                                        connect_to_remote();
                                    }
                                });
                        }
                    });
            }
        });
}

void ClientSession::connect_to_remote() {
    auto self(shared_from_this());
    m_resolver.async_resolve(m_config.server_ip, std::to_string(m_config.server_port),
        [this, self](std::error_code ec, tcp::resolver::results_type results) {
            if (!ec) {
                asio::async_connect(m_remote_socket, results,
                    [this, self](std::error_code ec, tcp::endpoint /*endpoint*/) {
                        if (!ec) {
                            do_remote_handshake();
                        } else {
                            // Failed to connect to proxy server
                            m_local_socket.close();
                        }
                    });
            } else {
                 m_local_socket.close();
            }
        });
}

void ClientSession::do_remote_handshake() {
    auto self(shared_from_this());
    // Send 05 01 02 (User/Pass auth)
    auto handshake = std::make_shared<std::vector<uint8_t>>(std::initializer_list<uint8_t>{0x05, 0x01, 0x02});
    asio::async_write(m_remote_socket, asio::buffer(*handshake),
        [this, self, handshake](std::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                // Read response
                m_remote_socket.async_read_some(asio::buffer(m_remote_buffer),
                    [this, self](std::error_code ec, std::size_t length) {
                        if (!ec && length >= 2 && m_remote_buffer[1] == 0x02) {
                            do_remote_auth();
                        } else {
                            // Auth method rejected
                            m_local_socket.close();
                            m_remote_socket.close();
                        }
                    });
            }
        });
}

void ClientSession::do_remote_auth() {
    auto self(shared_from_this());
    // Send 01 ULEN User PLEN Pass
    auto auth_packet = std::make_shared<std::vector<uint8_t>>();
    auth_packet->push_back(0x01);
    auth_packet->push_back(static_cast<uint8_t>(m_config.username.length()));
    auth_packet->insert(auth_packet->end(), m_config.username.begin(), m_config.username.end());
    auth_packet->push_back(static_cast<uint8_t>(m_config.password.length()));
    auth_packet->insert(auth_packet->end(), m_config.password.begin(), m_config.password.end());

    asio::async_write(m_remote_socket, asio::buffer(*auth_packet),
        [this, self, auth_packet](std::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                m_remote_socket.async_read_some(asio::buffer(m_remote_buffer),
                    [this, self](std::error_code ec, std::size_t length) {
                        if (!ec && length >= 2 && m_remote_buffer[1] == 0x00) {
                            // Auth success
                            do_forward_request();
                        } else {
                            // Auth failed
                            m_local_socket.close();
                            m_remote_socket.close();
                        }
                    });
            }
        });
}

void ClientSession::do_forward_request() {
    auto self(shared_from_this());
    // Forward the pending request to remote server
    asio::async_write(m_remote_socket, asio::buffer(m_pending_request),
        [this, self](std::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                // Read response from remote server (Connect success/fail)
                m_remote_socket.async_read_some(asio::buffer(m_remote_buffer),
                    [this, self](std::error_code ec, std::size_t length) {
                        if (!ec) {
                            // Forward response to local app
                            asio::async_write(m_local_socket, asio::buffer(m_remote_buffer, length),
                                [this, self](std::error_code ec, std::size_t /*length*/) {
                                    if (!ec) {
                                        // Now start streaming
                                        do_stream();
                                    }
                                });
                        } else {
                            // Close
                        }
                    });
            } else {
                // Close
            }
        });
}

void ClientSession::do_stream() {
    do_read_local();
    do_read_remote();
}

void ClientSession::do_read_local() {
    auto self(shared_from_this());
    m_local_socket.async_read_some(asio::buffer(m_local_buffer),
        [this, self](std::error_code ec, std::size_t length) {
            if (!ec) {
                do_write_remote(length);
            } else {
                m_remote_socket.close();
            }
        });
}

void ClientSession::do_write_remote(std::size_t length) {
    auto self(shared_from_this());
    asio::async_write(m_remote_socket, asio::buffer(m_local_buffer, length),
        [this, self](std::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                do_read_local();
            } else {
                m_local_socket.close();
            }
        });
}

void ClientSession::do_read_remote() {
    auto self(shared_from_this());
    m_remote_socket.async_read_some(asio::buffer(m_remote_buffer),
        [this, self](std::error_code ec, std::size_t length) {
            if (!ec) {
                do_write_local(length);
            } else {
                m_local_socket.close();
            }
        });
}

void ClientSession::do_write_local(std::size_t length) {
    auto self(shared_from_this());
    asio::async_write(m_local_socket, asio::buffer(m_remote_buffer, length),
        [this, self](std::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                do_read_remote();
            } else {
                m_remote_socket.close();
            }
        });
}

Client::Client(asio::io_context& io_context, const ClientConfig& config)
    : m_acceptor(io_context, tcp::endpoint(tcp::v4(), config.local_port)),
      m_config(config) {
    do_accept();
}

void Client::do_accept() {
    m_acceptor.async_accept(
        [this](std::error_code ec, tcp::socket socket) {
            if (!ec) {
                std::make_shared<ClientSession>(std::move(socket), m_config)->start();
            }
            do_accept();
        });
}
