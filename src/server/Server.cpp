#include "Server.h"
#include "Protocol.h"
#include "UserManager.h"
#include "Logger.h"
#include <iostream>
#include <cstring>
#include <string_view>

using asio::ip::tcp;

// Helper: format bytes to human-readable
static std::string formatBytes(uint64_t bytes) {
    if (bytes < 1024) return std::to_string(bytes) + " B";
    if (bytes < 1024 * 1024) return std::to_string(bytes / 1024) + " KB";
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f MB", bytes / (1024.0 * 1024.0));
    return buf;
}

// Helper: format duration
static std::string formatDuration(std::time_t seconds) {
    if (seconds < 60) return std::to_string(seconds) + "s";
    if (seconds < 3600) return std::to_string(seconds / 60) + "m " + std::to_string(seconds % 60) + "s";
    char buf[32];
    snprintf(buf, sizeof(buf), "%lldh %lldm", (long long)(seconds / 3600), (long long)((seconds % 3600) / 60));
    return buf;
}

Session::Session(tcp::socket socket)
    : m_client_socket(std::move(socket)),
      m_remote_socket(static_cast<asio::io_context&>(m_client_socket.get_executor().context())),
      m_resolver(static_cast<asio::io_context&>(m_client_socket.get_executor().context())) {
}

void Session::start() {
    auto ep = m_client_socket.remote_endpoint();
    m_client_addr = ep.address().to_string() + ":" + std::to_string(ep.port());
    m_start_time = std::time(nullptr);
    Logger::getInstance().info("[CONN] New connection from " + m_client_addr);
    do_handshake();
}

void Session::close_session(const std::string& reason) {
    std::error_code ec;
    m_client_socket.close(ec);
    m_remote_socket.close(ec);

    if (m_tunnel_active) {
        std::time_t duration = std::time(nullptr) - m_start_time;
        Logger::getInstance().info("[CLOSE] " + m_client_addr + " user=" + m_username +
            " target=" + m_target_host + ":" + std::to_string(m_target_port) +
            " reason=" + reason +
            " up=" + formatBytes(m_bytes_up.load()) +
            " down=" + formatBytes(m_bytes_down.load()) +
            " duration=" + formatDuration(duration));
    } else {
        Logger::getInstance().info("[CLOSE] " + m_client_addr + " reason=" + reason);
    }
    m_tunnel_active = false;
}

void Session::do_handshake() {
    auto self(shared_from_this());
    m_client_socket.async_read_some(asio::buffer(m_client_buffer),
        [this, self](std::error_code ec, std::size_t length) {
            if (!ec) {
                if (is_http_connect(m_client_buffer.data(), length)) {
                    m_is_http = true;
                    handle_http_connect(length);
                } else {
                    handle_socks5_handshake(length);
                }
            } else {
                Logger::getInstance().warn("[HANDSHAKE] Read error from " + m_client_addr + ": " + ec.message());
                close_session("handshake error");
            }
        });
}

bool Session::is_http_connect(const uint8_t* data, std::size_t length) {
    return length >= 8 && std::memcmp(data, "CONNECT ", 8) == 0;
}

// ============ HTTP CONNECT Flow ============

void Session::handle_http_connect(std::size_t length) {
    const char* str = reinterpret_cast<const char*>(m_client_buffer.data());
    std::string_view sv(str, length);

    auto host_start = sv.find(' ');
    if (host_start == std::string_view::npos) {
        Logger::getInstance().error("[HTTP] Bad CONNECT request from " + m_client_addr + " - no host");
        auto response = std::make_shared<std::string>("HTTP/1.1 400 Bad Request\r\n\r\n");
        asio::async_write(m_client_socket, asio::buffer(*response),
            [this, self = shared_from_this(), response](std::error_code, std::size_t) {
                close_session("bad request");
            });
        return;
    }
    host_start++;

    auto host_end = sv.find(' ', host_start);
    if (host_end == std::string_view::npos) {
        Logger::getInstance().error("[HTTP] Bad CONNECT request from " + m_client_addr + " - malformed");
        auto response = std::make_shared<std::string>("HTTP/1.1 400 Bad Request\r\n\r\n");
        asio::async_write(m_client_socket, asio::buffer(*response),
            [this, self = shared_from_this(), response](std::error_code, std::size_t) {
                close_session("bad request");
            });
        return;
    }

    std::string_view host_port = sv.substr(host_start, host_end - host_start);
    auto colon_pos = host_port.rfind(':');
    if (colon_pos == std::string_view::npos) {
        Logger::getInstance().error("[HTTP] Bad CONNECT request from " + m_client_addr + " - no port");
        auto response = std::make_shared<std::string>("HTTP/1.1 400 Bad Request\r\n\r\n");
        asio::async_write(m_client_socket, asio::buffer(*response),
            [this, self = shared_from_this(), response](std::error_code, std::size_t) {
                close_session("bad request");
            });
        return;
    }

    m_target_host = std::string(host_port.substr(0, colon_pos));
    std::string port_str(host_port.substr(colon_pos + 1));
    m_target_port = static_cast<uint16_t>(std::stoi(port_str));

    // Check for Proxy-Authorization: Basic base64(user:pass)
    auto auth_header_pos = sv.find("Proxy-Authorization: Basic ");
    if (auth_header_pos != std::string_view::npos) {
        auto cred_start = auth_header_pos + 27;
        auto line_end = sv.find("\r\n", cred_start);
        if (line_end != std::string_view::npos) {
            std::string cred_b64(sv.substr(cred_start, line_end - cred_start));
            static const std::string base64_chars =
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            std::string decoded;
            std::vector<int> T(256, -1);
            for (int i = 0; i < 64; i++) T[base64_chars[i]] = i;

            int val = 0, valb = -8;
            for (unsigned char c : cred_b64) {
                if (T[c] == -1) break;
                val = (val << 6) + T[c];
                valb += 6;
                if (valb >= 0) {
                    decoded.push_back(char((val >> valb) & 0xFF));
                    valb -= 8;
                }
            }
            auto colon = decoded.find(':');
            if (colon != std::string::npos) {
                m_http_username = decoded.substr(0, colon);
                m_http_password = decoded.substr(colon + 1);
            }
        }
    }

    // If no auth provided, request it
    if (m_http_username.empty() && m_http_password.empty()) {
        Logger::getInstance().warn("[HTTP] CONNECT " + m_target_host + ":" + std::to_string(m_target_port) + " from " + m_client_addr + " - no auth, requesting 407");
        auto response = std::make_shared<std::string>(
            "HTTP/1.1 407 Proxy Authentication Required\r\n"
            "Proxy-Authenticate: Basic realm=\"proxy\"\r\n"
            "\r\n");
        asio::async_write(m_client_socket, asio::buffer(*response),
            [this, self = shared_from_this(), response](std::error_code, std::size_t) {
                close_session("no auth");
            });
        return;
    }

    m_username = m_http_username;
    Logger::getInstance().info("[HTTP] CONNECT " + m_target_host + ":" + std::to_string(m_target_port) + " user=" + m_username + " from=" + m_client_addr);
    do_http_auth();
}

void Session::do_http_auth() {
    auto self(shared_from_this());

    bool authenticated = UserManager::getInstance().authenticate(m_http_username, m_http_password);

    if (!authenticated) {
        Logger::getInstance().warn("[HTTP] Auth failed user=" + m_http_username + " target=" + m_target_host + ":" + std::to_string(m_target_port) + " from=" + m_client_addr);
        auto response = std::make_shared<std::string>(
            "HTTP/1.1 407 Proxy Authentication Required\r\n"
            "Proxy-Authenticate: Basic realm=\"proxy\"\r\n"
            "\r\n");
        asio::async_write(m_client_socket, asio::buffer(*response),
            [this, self, response](std::error_code, std::size_t) {
                close_session("auth failed");
            });
        return;
    }

    Logger::getInstance().info("[HTTP] Auth success user=" + m_http_username + " target=" + m_target_host + ":" + std::to_string(m_target_port));
    do_http_request();
}

void Session::do_http_request() {
    do_resolve(m_target_host, m_target_port);
}

// ============ SOCKS5 Flow ============

void Session::handle_socks5_handshake(std::size_t length) {
    if (length < 2 || m_client_buffer[0] != LanProxy::SOCKS_VERSION) {
        Logger::getInstance().warn("[SOCKS5] Invalid handshake from " + m_client_addr + " (not SOCKS5)");
        close_session("invalid handshake");
        return;
    }

    auto self(shared_from_this());
    uint8_t nmethods = m_client_buffer[1];

    // Handle partial read: need at least 2 + nmethods bytes
    if (length < 2 + nmethods) {
        m_client_socket.async_read_some(asio::buffer(m_client_buffer),
            [this, self](std::error_code ec, std::size_t len) {
                if (!ec) {
                    handle_socks5_handshake(len);
                } else {
                    close_session("handshake read error");
                }
            });
        return;
    }

    bool support_auth = false;
    for (size_t i = 0; i < nmethods; ++i) {
        if (m_client_buffer[2 + i] == LanProxy::AUTH_USERPASS) {
            support_auth = true;
            break;
        }
    }

    auto response = std::make_shared<std::vector<uint8_t>>(std::initializer_list<uint8_t>{LanProxy::SOCKS_VERSION, LanProxy::AUTH_USERPASS});
    if (!support_auth) {
        (*response)[1] = LanProxy::AUTH_NO_ACCEPTABLE;
        Logger::getInstance().warn("[SOCKS5] Client from " + m_client_addr + " does not support username/password auth");
    }

    asio::async_write(m_client_socket, asio::buffer(*response),
        [this, self, response, support_auth](std::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                if (support_auth) {
                    do_auth();
                } else {
                    close_session("auth not supported");
                }
            }
        });
}

void Session::do_auth() {
    auto self(shared_from_this());
    m_client_socket.async_read_some(asio::buffer(m_client_buffer),
        [this, self](std::error_code ec, std::size_t length) {
            if (!ec) {
                if (length < 2 || m_client_buffer[0] != 0x01) {
                    Logger::getInstance().error("[SOCKS5] Invalid auth sub-negotiation from " + m_client_addr);
                    close_session("invalid auth");
                    return;
                }

                uint8_t ulen = m_client_buffer[1];
                if (length < 2 + ulen + 1) {
                    m_client_socket.async_read_some(asio::buffer(m_client_buffer),
                        [this, self](std::error_code ec2, std::size_t len2) {
                            if (!ec2) {
                                do_auth();
                            } else {
                                close_session("auth read error");
                            }
                        });
                    return;
                }

                std::string username((char*)&m_client_buffer[2], ulen);

                uint8_t plen = m_client_buffer[2 + ulen];
                if (length < 2 + ulen + 1 + plen) {
                    m_client_socket.async_read_some(asio::buffer(m_client_buffer),
                        [this, self](std::error_code ec2, std::size_t len2) {
                            if (!ec2) {
                                do_auth();
                            } else {
                                close_session("auth read error");
                            }
                        });
                    return;
                }

                std::string password((char*)&m_client_buffer[2 + ulen + 1], plen);

                bool authenticated = UserManager::getInstance().authenticate(username, password);

                if (authenticated) {
                    m_username = username;
                    Logger::getInstance().info("[SOCKS5] Auth success user=" + username + " from=" + m_client_addr);
                } else {
                    Logger::getInstance().warn("[SOCKS5] Auth failed user=" + username + " from=" + m_client_addr);
                }

                auto response = std::make_shared<std::vector<uint8_t>>(std::initializer_list<uint8_t>{0x01, authenticated ? (uint8_t)0x00 : (uint8_t)0x01});

                asio::async_write(m_client_socket, asio::buffer(*response),
                    [this, self, response, authenticated, username](std::error_code ec, std::size_t /*length*/) {
                        if (!ec && authenticated) {
                            do_request();
                        } else if (!authenticated) {
                            close_session("auth failed");
                        }
                    });
            } else {
                close_session("auth read error");
            }
        });
}

void Session::do_request() {
    auto self(shared_from_this());
    m_client_socket.async_read_some(asio::buffer(m_client_buffer),
        [this, self](std::error_code ec, std::size_t length) {
            if (!ec) {
                if (length < 4 || m_client_buffer[0] != LanProxy::SOCKS_VERSION) {
                    Logger::getInstance().error("[SOCKS5] Invalid request from " + m_client_addr + " (bad version)");
                    close_session("invalid request");
                    return;
                }

                if (m_client_buffer[1] != LanProxy::CMD_CONNECT) {
                    Logger::getInstance().error("[SOCKS5] Unsupported command " + std::to_string(m_client_buffer[1]) + " from " + m_client_addr);
                    auto reply = std::make_shared<std::vector<uint8_t>>(std::initializer_list<uint8_t>{
                        LanProxy::SOCKS_VERSION, 0x07, 0x00, LanProxy::ATYP_IPV4,
                        0, 0, 0, 0, 0, 0
                    });
                    asio::async_write(m_client_socket, asio::buffer(*reply),
                        [this, self, reply](std::error_code, std::size_t) {
                            close_session("unsupported command");
                        });
                    return;
                }

                uint8_t atyp = m_client_buffer[3];
                std::string host;
                uint16_t port = 0;
                size_t needed = 0;

                if (atyp == LanProxy::ATYP_IPV4) {
                    needed = 10;
                    if (length < needed) {
                        m_client_socket.async_read_some(asio::buffer(m_client_buffer),
                            [this, self](std::error_code ec2, std::size_t) {
                                if (!ec2) do_request();
                                else close_session("request read error");
                            });
                        return;
                    }
                    asio::ip::address_v4::bytes_type bytes;
                    std::copy_n(&m_client_buffer[4], 4, bytes.begin());
                    host = asio::ip::make_address_v4(bytes).to_string();
                    port = (m_client_buffer[8] << 8) | m_client_buffer[9];
                } else if (atyp == LanProxy::ATYP_DOMAIN) {
                    uint8_t domain_len = m_client_buffer[4];
                    needed = 5 + domain_len + 2;
                    if (length < needed) {
                        m_client_socket.async_read_some(asio::buffer(m_client_buffer),
                            [this, self](std::error_code ec2, std::size_t) {
                                if (!ec2) do_request();
                                else close_session("request read error");
                            });
                        return;
                    }
                    host = std::string((char*)&m_client_buffer[5], domain_len);
                    port = (m_client_buffer[5 + domain_len] << 8) | m_client_buffer[5 + domain_len + 1];
                } else if (atyp == LanProxy::ATYP_IPV6) {
                    needed = 22;
                    if (length < needed) {
                        m_client_socket.async_read_some(asio::buffer(m_client_buffer),
                            [this, self](std::error_code ec2, std::size_t) {
                                if (!ec2) do_request();
                                else close_session("request read error");
                            });
                        return;
                    }
                    asio::ip::address_v6::bytes_type bytes;
                    std::copy_n(&m_client_buffer[4], 16, bytes.begin());
                    host = asio::ip::make_address_v6(bytes).to_string();
                    port = (m_client_buffer[20] << 8) | m_client_buffer[21];
                } else {
                    Logger::getInstance().error("[SOCKS5] Unknown address type " + std::to_string(atyp) + " from " + m_client_addr);
                    auto reply = std::make_shared<std::vector<uint8_t>>(std::initializer_list<uint8_t>{
                        LanProxy::SOCKS_VERSION, 0x08, 0x00, LanProxy::ATYP_IPV4,
                        0, 0, 0, 0, 0, 0
                    });
                    asio::async_write(m_client_socket, asio::buffer(*reply),
                        [this, self, reply](std::error_code, std::size_t) {
                            close_session("unknown address type");
                        });
                    return;
                }

                m_target_host = host;
                m_target_port = port;

                Logger::getInstance().info("[SOCKS5] CONNECT " + m_target_host + ":" + std::to_string(m_target_port) + " user=" + m_username + " from=" + m_client_addr);
                do_resolve(m_target_host, m_target_port);
            } else {
                close_session("request read error");
            }
        });
}

// ============ Common ============

void Session::do_resolve(const std::string& host, uint16_t port) {
    auto self(shared_from_this());
    Logger::getInstance().info("[DNS] Resolving " + host + " ...");
    m_resolver.async_resolve(host, std::to_string(port),
        [this, self, host, port](std::error_code ec, tcp::resolver::results_type results) {
            if (!ec) {
                Logger::getInstance().info("[DNS] Resolved " + host + " -> " + std::to_string(results.size()) + " endpoint(s)");
                do_connect(results);
            } else {
                Logger::getInstance().error("[" + std::string(m_is_http ? "HTTP" : "SOCKS5") + "] DNS resolve failed: " + host + " - " + ec.message());
                if (m_is_http) {
                    auto response = std::make_shared<std::string>("HTTP/1.1 502 Bad Gateway\r\n\r\n");
                    asio::async_write(m_client_socket, asio::buffer(*response),
                        [this, self, response](std::error_code, std::size_t) {
                            close_session("dns failed");
                        });
                } else {
                    auto reply = std::make_shared<std::vector<uint8_t>>(std::initializer_list<uint8_t>{
                        LanProxy::SOCKS_VERSION, 0x04, 0x00, LanProxy::ATYP_IPV4,
                        0, 0, 0, 0, 0, 0
                    });
                    asio::async_write(m_client_socket, asio::buffer(*reply),
                        [this, self, reply](std::error_code, std::size_t) {
                            close_session("dns failed");
                        });
                }
            }
        });
}

void Session::do_connect(const tcp::resolver::results_type& endpoints) {
    auto self(shared_from_this());
    asio::async_connect(m_remote_socket, endpoints,
        [this, self](std::error_code ec, tcp::endpoint endpoint) {
            if (!ec) {
                m_tunnel_active = true;
                Logger::getInstance().info("[" + std::string(m_is_http ? "HTTP" : "SOCKS5") + "] Tunnel established -> " + m_target_host + ":" + std::to_string(m_target_port) + " user=" + m_username + " from=" + m_client_addr + " remote=" + endpoint.address().to_string() + ":" + std::to_string(endpoint.port()));
                if (m_is_http) {
                    auto response = std::make_shared<std::string>("HTTP/1.1 200 Connection established\r\n\r\n");
                    asio::async_write(m_client_socket, asio::buffer(*response),
                        [this, self, response](std::error_code ec, std::size_t /*length*/) {
                            if (!ec) {
                                do_stream();
                            } else {
                                close_session("200 write error");
                            }
                        });
                } else {
                    auto bound_ep = m_remote_socket.local_endpoint();
                    auto bound_addr = bound_ep.address().to_v4();
                    auto bound_bytes = bound_addr.to_bytes();
                    uint16_t bound_port = bound_ep.port();

                    auto response = std::make_shared<std::vector<uint8_t>>(std::initializer_list<uint8_t>{
                        LanProxy::SOCKS_VERSION, 0x00, 0x00, LanProxy::ATYP_IPV4,
                        bound_bytes[0], bound_bytes[1], bound_bytes[2], bound_bytes[3],
                        (uint8_t)(bound_port >> 8), (uint8_t)(bound_port & 0xFF)
                    });
                    asio::async_write(m_client_socket, asio::buffer(*response),
                        [this, self, response](std::error_code ec, std::size_t /*length*/) {
                            if (!ec) {
                                do_stream();
                            } else {
                                close_session("socks5 reply error");
                            }
                        });
                }
            } else {
                Logger::getInstance().error("[" + std::string(m_is_http ? "HTTP" : "SOCKS5") + "] Connect failed: " + m_target_host + ":" + std::to_string(m_target_port) + " - " + ec.message());
                if (m_is_http) {
                    auto response = std::make_shared<std::string>("HTTP/1.1 502 Bad Gateway\r\n\r\n");
                    asio::async_write(m_client_socket, asio::buffer(*response),
                        [this, self, response](std::error_code, std::size_t) {
                            close_session("connect failed");
                        });
                } else {
                    auto reply = std::make_shared<std::vector<uint8_t>>(std::initializer_list<uint8_t>{
                        LanProxy::SOCKS_VERSION, 0x05, 0x00, LanProxy::ATYP_IPV4,
                        0, 0, 0, 0, 0, 0
                    });
                    asio::async_write(m_client_socket, asio::buffer(*reply),
                        [this, self, reply](std::error_code, std::size_t) {
                            close_session("connect failed");
                        });
                }
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
                m_bytes_up += length;
                do_write_remote(length);
            } else {
                close_session("client disconnect");
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
                close_session("remote write error");
            }
        });
}

void Session::do_read_remote() {
    auto self(shared_from_this());
    m_remote_socket.async_read_some(asio::buffer(m_remote_buffer),
        [this, self](std::error_code ec, std::size_t length) {
            if (!ec) {
                m_bytes_down += length;
                do_write_client(length);
            } else {
                close_session("remote disconnect");
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
                close_session("client write error");
            }
        });
}

// ============ Server Acceptor ============

Server::Server(asio::io_context& io_context, short port)
    : m_acceptor(io_context, tcp::endpoint(tcp::v4(), port)) {
    Logger::getInstance().info("[SERVER] Listening on port " + std::to_string(port));
    do_accept();
}

void Server::stop() {
    m_running = false;
    std::error_code ec;
    m_acceptor.close(ec);
    Logger::getInstance().info("[SERVER] Stopped");
}

void Server::do_accept() {
    m_acceptor.async_accept(
        [this](std::error_code ec, tcp::socket socket) {
            if (!ec && m_running) {
                std::make_shared<Session>(std::move(socket))->start();
            }
            if (m_running) {
                do_accept();
            }
        });
}

// ============ ServerApp ============

void ServerApp::start(int port) {
    if (m_running) return;
    m_port = port;

    m_io_context = std::make_unique<asio::io_context>();
    m_work_guard.emplace(asio::make_work_guard(*m_io_context));
    m_server = std::make_unique<Server>(*m_io_context, m_port);

    m_thread = std::thread([this]() {
        m_io_context->run();
    });
    m_running = true;
    Logger::getInstance().info("[SERVER] Started on port " + std::to_string(m_port));
}

void ServerApp::stop() {
    if (!m_running) return;
    m_running = false;
    if (m_server) m_server->stop();
    m_work_guard.reset();
    if (m_io_context) m_io_context->stop();
    if (m_thread.joinable()) m_thread.join();
    m_server.reset();
    m_io_context.reset();
    Logger::getInstance().info("[SERVER] Shutdown complete");
}
