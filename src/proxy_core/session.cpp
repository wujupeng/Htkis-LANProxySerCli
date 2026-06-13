#include "proxy_core/session.h"
#include "proxy_core/protocol.h"
#include "route_decision/route_engine.h"
#include "user_manager/user_manager.h"
#include "log_monitor/structured_logger.h"
#include "log_monitor/metrics_collector.h"
#include "config_manager/config_loader.h"
#include "concurrency/load_balancer.h"
#include "concurrency/overload_protector.h"
#include "concurrency/rate_limiter.h"
#include "concurrency/connection_admitter.h"
#include "concurrency/connection_registry.h"
#include "concurrency/failover_handler.h"
#include "concurrency/concurrency_metrics.h"
#include <cstring>
#include <string_view>
#include <iomanip>
#include <sstream>

using asio::ip::tcp;

static std::string format_bytes(uint64_t bytes) {
    if (bytes < 1024) return std::to_string(bytes) + " B";
    if (bytes < 1024 * 1024) return std::to_string(bytes / 1024) + " KB";
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f MB", bytes / (1024.0 * 1024.0));
    return buf;
}

static std::string format_duration(std::time_t seconds) {
    if (seconds < 60) return std::to_string(seconds) + "s";
    if (seconds < 3600) return std::to_string(seconds / 60) + "m " + std::to_string(seconds % 60) + "s";
    char buf[32];
    snprintf(buf, sizeof(buf), "%lldh %lldm", (long long)(seconds / 3600), (long long)((seconds % 3600) / 60));
    return buf;
}

std::string session::generate_session_id() {
    std::random_device rd;
    std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFF);
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (int i = 0; i < 4; ++i) ss << std::setw(8) << dist(rd);
    return ss.str();
}

session::session(tcp::socket socket, route_engine& router, user_manager& users)
    : m_client_socket(std::move(socket)),
      m_remote_socket(static_cast<asio::io_context&>(m_client_socket.get_executor().context())),
      m_resolver(static_cast<asio::io_context&>(m_client_socket.get_executor().context())),
      m_router(router),
      m_users(users) {
    m_session_id = generate_session_id();
}

void session::start() {
    auto ep = m_client_socket.remote_endpoint();
    m_client_addr = ep.address().to_string() + ":" + std::to_string(ep.port());
    m_start_time = std::time(nullptr);

    if (overload_protector::instance().is_overloaded()) {
        structured_logger::instance().warn("session",
            "Rejected: system overloaded from " + m_client_addr, m_session_id);
        close_session("system overloaded");
        return;
    }

    std::string client_ip = ep.address().to_string();
    if (!rate_limiter::instance().check_connection_rate(client_ip)) {
        structured_logger::instance().warn("session",
            "Rejected: rate limited from " + m_client_addr, m_session_id);
        close_session("rate limited");
        return;
    }

    overload_protector::instance().inc_total_connections();

    auto admit = connection_admitter::instance().request_admit(
        m_session_id, client_ip, "",
        nullptr, nullptr, false);

    if (admit.result == admit_result::rejected_total ||
        admit.result == admit_result::rejected_user ||
        admit.result == admit_result::rejected_rate ||
        admit.result == admit_result::rejected_banned ||
        admit.result == admit_result::rejected_overload) {
        overload_protector::instance().dec_total_connections();
        structured_logger::instance().warn("session",
            "Rejected: " + admit.reason + " from " + m_client_addr, m_session_id);
        close_session(admit.reason);
        return;
    }

    if (admit.result == admit_result::queued) {
        structured_logger::instance().info("session",
            "Queued: position=" + std::to_string(admit.queue_position) +
            " from " + m_client_addr, m_session_id);
    }

    m_admitted = true;

    connection_registry::instance().register_connection(
        m_session_id, m_client_addr, "");

    metrics_collector::instance().inc_active_connections();
    metrics_collector::instance().inc_total_connections();
    structured_logger::instance().info("session",
        "New connection from " + m_client_addr, m_session_id);
    do_handshake();
}

void session::close_session(const std::string& reason) {
    std::error_code ec;
    m_client_socket.close(ec);
    m_remote_socket.close(ec);

    overload_protector::instance().dec_total_connections();

    metrics_collector::instance().dec_active_connections();

    if (m_tunnel_active && m_route_action == session_route_action::proxy && !m_selected_node_tag.empty()) {
        load_balancer::instance().dec_node_connections(m_selected_node_tag);
        concurrency_metrics::instance().dec_node_connections(m_selected_node_tag);
    }

    if (m_admitted) {
        connection_admitter::instance().release_connection(m_username);
    }

    connection_registry::instance().unregister_connection(m_session_id);

    if (m_tunnel_active) {
        std::time_t duration = std::time(nullptr) - m_start_time;
        std::string route_str = (m_route_action == session_route_action::direct)
                                 ? "direct" : "proxy";

        nlohmann::json stats;
        stats["session_id"] = m_session_id;
        stats["client"] = m_client_addr;
        stats["user"] = m_username;
        stats["target"] = m_target_host + ":" + std::to_string(m_target_port);
        stats["route"] = route_str;
        stats["bytes_up"] = m_bytes_up.load();
        stats["bytes_down"] = m_bytes_down.load();
        stats["duration"] = (int64_t)duration;
        stats["reason"] = reason;

        structured_logger::instance().info("session",
            "Close: " + stats.dump(), m_session_id);

        if (m_route_action == session_route_action::direct) {
            metrics_collector::instance().add_direct_bytes_up(m_bytes_up.load());
            metrics_collector::instance().add_direct_bytes_down(m_bytes_down.load());
        } else {
            metrics_collector::instance().add_proxy_bytes_up(m_bytes_up.load());
            metrics_collector::instance().add_proxy_bytes_down(m_bytes_down.load());
        }
    } else {
        structured_logger::instance().info("session",
            "Close: " + m_client_addr + " reason=" + reason, m_session_id);
    }
    m_tunnel_active = false;
}

void session::do_handshake() {
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
                close_session("handshake error");
            }
        });
}

bool session::is_http_connect(const uint8_t* data, std::size_t length) {
    return length >= 8 && std::memcmp(data, "CONNECT ", 8) == 0;
}

void session::handle_http_connect(std::size_t length) {
    const char* str = reinterpret_cast<const char*>(m_client_buffer.data());
    std::string_view sv(str, length);

    auto host_start = sv.find(' ');
    if (host_start == std::string_view::npos) {
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

    if (m_http_username.empty() && m_http_password.empty()) {
        structured_logger::instance().warn("session",
            "HTTP CONNECT no auth " + m_target_host + ":" + std::to_string(m_target_port),
            m_session_id);
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
    structured_logger::instance().info("session",
        "HTTP CONNECT " + m_target_host + ":" + std::to_string(m_target_port) +
        " user=" + m_username, m_session_id);
    do_http_auth();
}

void session::do_http_auth() {
    auto self(shared_from_this());
    bool authenticated = m_users.authenticate(m_http_username, m_http_password);

    if (!authenticated) {
        std::string client_ip = m_client_addr.substr(0, m_client_addr.rfind(':'));
        rate_limiter::instance().record_auth_failure(client_ip);
        structured_logger::instance().warn("session",
            "HTTP auth failed user=" + m_http_username, m_session_id);
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

    structured_logger::instance().info("session",
        "HTTP auth success user=" + m_http_username, m_session_id);
    std::fill(m_http_password.begin(), m_http_password.end(), '\0');
    m_http_password.clear();
    connection_registry::instance().update_state(m_session_id, connection_state::handshaking);
    concurrency_metrics::instance().inc_user_connections(m_http_username);
    do_http_request();
}

void session::do_http_request() {
    do_route_decision();
}

void session::handle_socks5_handshake(std::size_t length) {
    if (length < 2 || m_client_buffer[0] != LanProxy::SOCKS_VERSION) {
        structured_logger::instance().warn("session",
            "Invalid handshake from " + m_client_addr, m_session_id);
        close_session("invalid handshake");
        return;
    }

    auto self(shared_from_this());
    uint8_t nmethods = m_client_buffer[1];

    if (length < 2 + nmethods) {
        m_client_socket.async_read_some(asio::buffer(m_client_buffer),
            [this, self](std::error_code ec, std::size_t len) {
                if (!ec) handle_socks5_handshake(len);
                else close_session("handshake read error");
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

    auto response = std::make_shared<std::vector<uint8_t>>(
        std::initializer_list<uint8_t>{LanProxy::SOCKS_VERSION, LanProxy::AUTH_USERPASS});
    if (!support_auth) {
        (*response)[1] = LanProxy::AUTH_NO_ACCEPTABLE;
        structured_logger::instance().warn("session",
            "Client no auth support from " + m_client_addr, m_session_id);
    }

    asio::async_write(m_client_socket, asio::buffer(*response),
        [this, self, response, support_auth](std::error_code ec, std::size_t) {
            if (!ec) {
                if (support_auth) do_auth();
                else close_session("auth not supported");
            }
        });
}

void session::do_auth() {
    auto self(shared_from_this());
    m_client_socket.async_read_some(asio::buffer(m_client_buffer),
        [this, self](std::error_code ec, std::size_t length) {
            if (!ec) {
                if (length < 2 || m_client_buffer[0] != 0x01) {
                    close_session("invalid auth");
                    return;
                }

                uint8_t ulen = m_client_buffer[1];
                if (length < 2 + ulen + 1) {
                    m_client_socket.async_read_some(asio::buffer(m_client_buffer),
                        [this, self](std::error_code ec2, std::size_t len2) {
                            if (!ec2) do_auth();
                            else close_session("auth read error");
                        });
                    return;
                }

                std::string username((char*)&m_client_buffer[2], ulen);
                uint8_t plen = m_client_buffer[2 + ulen];
                if (length < 2 + ulen + 1 + plen) {
                    m_client_socket.async_read_some(asio::buffer(m_client_buffer),
                        [this, self](std::error_code ec2, std::size_t len2) {
                            if (!ec2) do_auth();
                            else close_session("auth read error");
                        });
                    return;
                }

                std::string password((char*)&m_client_buffer[2 + ulen + 1], plen);
                bool authenticated = m_users.authenticate(username, password);
                std::fill(password.begin(), password.end(), '\0');

                if (authenticated) {
                    m_username = username;
                    connection_registry::instance().update_state(m_session_id, connection_state::handshaking);
                    concurrency_metrics::instance().inc_user_connections(username);
                    structured_logger::instance().info("session",
                        "SOCKS5 auth success user=" + username, m_session_id);
                } else {
                    std::string client_ip = m_client_addr.substr(0, m_client_addr.rfind(':'));
                    rate_limiter::instance().record_auth_failure(client_ip);
                    structured_logger::instance().warn("session",
                        "SOCKS5 auth failed user=" + username, m_session_id);
                }

                auto response = std::make_shared<std::vector<uint8_t>>(
                    std::initializer_list<uint8_t>{0x01, authenticated ? (uint8_t)0x00 : (uint8_t)0x01});

                asio::async_write(m_client_socket, asio::buffer(*response),
                    [this, self, response, authenticated](std::error_code ec, std::size_t) {
                        if (!ec && authenticated) do_request();
                        else if (!authenticated) close_session("auth failed");
                    });
            } else {
                close_session("auth read error");
            }
        });
}

void session::do_request() {
    auto self(shared_from_this());
    m_client_socket.async_read_some(asio::buffer(m_client_buffer),
        [this, self](std::error_code ec, std::size_t length) {
            if (!ec) {
                if (length < 4 || m_client_buffer[0] != LanProxy::SOCKS_VERSION) {
                    close_session("invalid request");
                    return;
                }

                if (m_client_buffer[1] != LanProxy::CMD_CONNECT) {
                    auto reply = std::make_shared<std::vector<uint8_t>>(
                        std::initializer_list<uint8_t>{
                            LanProxy::SOCKS_VERSION, 0x07, 0x00, LanProxy::ATYP_IPV4,
                            0, 0, 0, 0, 0, 0});
                    asio::async_write(m_client_socket, asio::buffer(*reply),
                        [this, self, reply](std::error_code, std::size_t) {
                            close_session("unsupported command");
                        });
                    return;
                }

                uint8_t atyp = m_client_buffer[3];
                std::string host;
                uint16_t port = 0;

                if (atyp == LanProxy::ATYP_IPV4) {
                    if (length < 10) {
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
                    if (length < (size_t)(5 + domain_len + 2)) {
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
                    if (length < 22) {
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
                    auto reply = std::make_shared<std::vector<uint8_t>>(
                        std::initializer_list<uint8_t>{
                            LanProxy::SOCKS_VERSION, 0x08, 0x00, LanProxy::ATYP_IPV4,
                            0, 0, 0, 0, 0, 0});
                    asio::async_write(m_client_socket, asio::buffer(*reply),
                        [this, self, reply](std::error_code, std::size_t) {
                            close_session("unknown address type");
                        });
                    return;
                }

                m_target_host = host;
                m_target_port = port;

                structured_logger::instance().info("session",
                    "SOCKS5 CONNECT " + m_target_host + ":" + std::to_string(m_target_port) +
                    " user=" + m_username, m_session_id);

                do_route_decision();
            } else {
                close_session("request read error");
            }
        });
}

void session::do_route_decision() {
    auto result = m_router.decide(m_target_host, m_resolved_ip);

    if (result.action == route_action::direct) {
        m_route_action = session_route_action::direct;
        structured_logger::instance().info("session",
            "Route: DIRECT " + m_target_host + " match=" + result.match_type +
            " rule=" + result.rule_id, m_session_id);
        do_resolve(m_target_host, m_target_port);
    } else {
        m_route_action = session_route_action::proxy;

        auto client_ip = m_client_addr.substr(0, m_client_addr.rfind(':'));
        auto sched = load_balancer::instance().select_node(client_ip);
        if (sched.has_value()) {
            m_selected_node_tag = sched->node_tag;
            load_balancer::instance().inc_node_connections(m_selected_node_tag);
            structured_logger::instance().info("session",
                "Route: PROXY(v2rayN) " + m_target_host +
                " node=" + m_selected_node_tag +
                " strategy=" + sched->strategy_used +
                " affinity=" + std::to_string(sched->from_affinity) +
                " match=" + result.match_type +
                " rule=" + result.rule_id, m_session_id);
        } else {
            structured_logger::instance().warn("session",
                "Route: PROXY(v2rayN) no available node for " + m_target_host +
                " match=" + result.match_type, m_session_id);
        }
        do_connect_via_v2rayn();
    }
}

void session::do_resolve(const std::string& host, uint16_t port) {
    auto self(shared_from_this());
    m_resolver.async_resolve(host, std::to_string(port),
        [this, self, host](std::error_code ec, tcp::resolver::results_type results) {
            if (!ec) {
                if (!results.empty()) {
                    m_resolved_ip = results.begin()->endpoint().address().to_string();
                }
                do_connect_direct(results);
            } else {
                structured_logger::instance().error("session",
                    "DNS failed: " + host + " - " + ec.message(), m_session_id);
                if (m_is_http) {
                    auto response = std::make_shared<std::string>("HTTP/1.1 502 Bad Gateway\r\n\r\n");
                    asio::async_write(m_client_socket, asio::buffer(*response),
                        [this, self, response](std::error_code, std::size_t) {
                            close_session("dns failed");
                        });
                } else {
                    auto reply = std::make_shared<std::vector<uint8_t>>(
                        std::initializer_list<uint8_t>{
                            LanProxy::SOCKS_VERSION, 0x04, 0x00, LanProxy::ATYP_IPV4,
                            0, 0, 0, 0, 0, 0});
                    asio::async_write(m_client_socket, asio::buffer(*reply),
                        [this, self, reply](std::error_code, std::size_t) {
                            close_session("dns failed");
                        });
                }
            }
        });
}

void session::do_connect_direct(const tcp::resolver::results_type& endpoints) {
    auto self(shared_from_this());
    asio::async_connect(m_remote_socket, endpoints,
        [this, self](std::error_code ec, tcp::endpoint endpoint) {
            if (!ec) {
                m_tunnel_active = true;
                connection_registry::instance().update_state(m_session_id, connection_state::active);
                structured_logger::instance().info("session",
                    "Direct tunnel -> " + m_target_host + ":" + std::to_string(m_target_port) +
                    " remote=" + endpoint.address().to_string() + ":" + std::to_string(endpoint.port()),
                    m_session_id);
                if (m_is_http) {
                    auto response = std::make_shared<std::string>("HTTP/1.1 200 Connection established\r\n\r\n");
                    asio::async_write(m_client_socket, asio::buffer(*response),
                        [this, self, response](std::error_code ec, std::size_t) {
                            if (!ec) do_stream();
                            else close_session("200 write error");
                        });
                } else {
                    auto bound_ep = m_remote_socket.local_endpoint();
                    auto bound_addr = bound_ep.address().to_v4();
                    auto bound_bytes = bound_addr.to_bytes();
                    uint16_t bound_port = bound_ep.port();
                    auto response = std::make_shared<std::vector<uint8_t>>(
                        std::initializer_list<uint8_t>{
                            LanProxy::SOCKS_VERSION, 0x00, 0x00, LanProxy::ATYP_IPV4,
                            bound_bytes[0], bound_bytes[1], bound_bytes[2], bound_bytes[3],
                            (uint8_t)(bound_port >> 8), (uint8_t)(bound_port & 0xFF)});
                    asio::async_write(m_client_socket, asio::buffer(*response),
                        [this, self, response](std::error_code ec, std::size_t) {
                            if (!ec) do_stream();
                            else close_session("socks5 reply error");
                        });
                }
            } else {
                structured_logger::instance().error("session",
                    "Direct connect failed: " + m_target_host + " - " + ec.message(),
                    m_session_id);
                if (m_is_http) {
                    auto response = std::make_shared<std::string>("HTTP/1.1 502 Bad Gateway\r\n\r\n");
                    asio::async_write(m_client_socket, asio::buffer(*response),
                        [this, self, response](std::error_code, std::size_t) {
                            close_session("connect failed");
                        });
                } else {
                    auto reply = std::make_shared<std::vector<uint8_t>>(
                        std::initializer_list<uint8_t>{
                            LanProxy::SOCKS_VERSION, 0x05, 0x00, LanProxy::ATYP_IPV4,
                            0, 0, 0, 0, 0, 0});
                    asio::async_write(m_client_socket, asio::buffer(*reply),
                        [this, self, reply](std::error_code, std::size_t) {
                            close_session("connect failed");
                        });
                }
            }
        });
}

void session::do_connect_via_v2rayn() {
    auto self(shared_from_this());
    auto& cfg = config_loader::instance().config();

    m_resolver.async_resolve("127.0.0.1",
        std::to_string(cfg.v2rayn_local_socks_port),
        [this, self](std::error_code ec, tcp::resolver::results_type results) {
            if (!ec) {
                asio::async_connect(m_remote_socket, results,
                    [this, self](std::error_code ec, tcp::endpoint) {
                        if (!ec) {
                            do_socks5_client_handshake(m_target_host, m_target_port);
                        } else {
                            structured_logger::instance().error("session",
                                "v2rayN connect failed: " + ec.message(), m_session_id);
                            if (m_is_http) {
                                auto response = std::make_shared<std::string>(
                                    "HTTP/1.1 502 Bad Gateway\r\n\r\n");
                                asio::async_write(m_client_socket, asio::buffer(*response),
                                    [this, self, response](std::error_code, std::size_t) {
                                        close_session("v2rayn connect failed");
                                    });
                            } else {
                                auto reply = std::make_shared<std::vector<uint8_t>>(
                                    std::initializer_list<uint8_t>{
                                        LanProxy::SOCKS_VERSION, 0x05, 0x00, LanProxy::ATYP_IPV4,
                                        0, 0, 0, 0, 0, 0});
                                asio::async_write(m_client_socket, asio::buffer(*reply),
                                    [this, self, reply](std::error_code, std::size_t) {
                                        close_session("v2rayn connect failed");
                                    });
                            }
                        }
                    });
            } else {
                close_session("v2rayn resolve failed");
            }
        });
}

void session::do_socks5_client_handshake(const std::string& host, uint16_t port) {
    auto self(shared_from_this());

    auto greeting = std::make_shared<std::vector<uint8_t>>(
        std::initializer_list<uint8_t>{LanProxy::SOCKS_VERSION, 0x01, LanProxy::AUTH_NONE});

    asio::async_write(m_remote_socket, asio::buffer(*greeting),
        [this, self, greeting, host, port](std::error_code ec, std::size_t) {
            if (ec) {
                close_session("v2rayn socks5 greeting write error");
                return;
            }

            m_remote_socket.async_read_some(asio::buffer(m_remote_buffer),
                [this, self, host, port](std::error_code ec, std::size_t length) {
                    if (ec || length < 2) {
                        close_session("v2rayn socks5 greeting read error");
                        return;
                    }

                    if (m_remote_buffer[0] != LanProxy::SOCKS_VERSION ||
                        m_remote_buffer[1] != LanProxy::AUTH_NONE) {
                        close_session("v2rayn socks5 auth rejected");
                        return;
                    }

                    std::vector<uint8_t> connect_req;
                    connect_req.push_back(LanProxy::SOCKS_VERSION);
                    connect_req.push_back(LanProxy::CMD_CONNECT);
                    connect_req.push_back(0x00);
                    connect_req.push_back(LanProxy::ATYP_DOMAIN);

                    uint8_t domain_len = static_cast<uint8_t>(host.length());
                    connect_req.push_back(domain_len);
                    connect_req.insert(connect_req.end(), host.begin(), host.end());
                    connect_req.push_back(static_cast<uint8_t>(port >> 8));
                    connect_req.push_back(static_cast<uint8_t>(port & 0xFF));

                    auto req = std::make_shared<std::vector<uint8_t>>(std::move(connect_req));

                    asio::async_write(m_remote_socket, asio::buffer(*req),
                        [this, self, req](std::error_code ec, std::size_t) {
                            if (ec) {
                                close_session("v2rayn socks5 connect write error");
                                return;
                            }

                            m_remote_socket.async_read_some(asio::buffer(m_remote_buffer),
                                [this, self](std::error_code ec, std::size_t length) {
                                    if (ec || length < 2) {
                                        close_session("v2rayn socks5 connect read error");
                                        return;
                                    }

                                    if (m_remote_buffer[1] != 0x00) {
                                        close_session("v2rayn socks5 connect rejected");
                                        return;
                                    }

                                    m_tunnel_active = true;
                                    connection_registry::instance().update_state(m_session_id, connection_state::active);
                                    if (!m_selected_node_tag.empty()) {
                                        connection_registry::instance().update_node(m_session_id, m_selected_node_tag);
                                    }
                                    structured_logger::instance().info("session",
                                        "v2rayN tunnel -> " + m_target_host + ":" +
                                        std::to_string(m_target_port), m_session_id);

                                    if (m_is_http) {
                                        auto response = std::make_shared<std::string>(
                                            "HTTP/1.1 200 Connection established\r\n\r\n");
                                        asio::async_write(m_client_socket, asio::buffer(*response),
                                            [this, self, response](std::error_code ec, std::size_t) {
                                                if (!ec) do_stream();
                                                else close_session("200 write error");
                                            });
                                    } else {
                                        auto bound_ep = m_remote_socket.local_endpoint();
                                        auto bound_addr = bound_ep.address().to_v4();
                                        auto bound_bytes = bound_addr.to_bytes();
                                        uint16_t bound_port = bound_ep.port();
                                        auto response = std::make_shared<std::vector<uint8_t>>(
                                            std::initializer_list<uint8_t>{
                                                LanProxy::SOCKS_VERSION, 0x00, 0x00, LanProxy::ATYP_IPV4,
                                                bound_bytes[0], bound_bytes[1], bound_bytes[2], bound_bytes[3],
                                                (uint8_t)(bound_port >> 8), (uint8_t)(bound_port & 0xFF)});
                                        asio::async_write(m_client_socket, asio::buffer(*response),
                                            [this, self, response](std::error_code ec, std::size_t) {
                                                if (!ec) do_stream();
                                                else close_session("socks5 reply error");
                                            });
                                    }
                                });
                        });
                });
        });
}

void session::do_stream() {
    do_read_client();
    do_read_remote();
}

void session::do_read_client() {
    auto self(shared_from_this());
    m_client_socket.async_read_some(asio::buffer(m_client_buffer),
        [this, self](std::error_code ec, std::size_t length) {
            if (!ec) {
                m_bytes_up += length;
                connection_registry::instance().mark_active(m_session_id);
                do_write_remote(length);
            } else {
                close_session("client disconnect");
            }
        });
}

void session::do_write_remote(std::size_t length) {
    auto self(shared_from_this());
    asio::async_write(m_remote_socket, asio::buffer(m_client_buffer, length),
        [this, self](std::error_code ec, std::size_t) {
            if (!ec) do_read_client();
            else close_session("remote write error");
        });
}

void session::do_read_remote() {
    auto self(shared_from_this());
    m_remote_socket.async_read_some(asio::buffer(m_remote_buffer),
        [this, self](std::error_code ec, std::size_t length) {
            if (!ec) {
                m_bytes_down += length;
                connection_registry::instance().mark_active(m_session_id);
                do_write_client(length);
            } else {
                close_session("remote disconnect");
            }
        });
}

void session::do_write_client(std::size_t length) {
    auto self(shared_from_this());
    asio::async_write(m_client_socket, asio::buffer(m_remote_buffer, length),
        [this, self](std::error_code ec, std::size_t) {
            if (!ec) do_read_remote();
            else close_session("client write error");
        });
}

// ============ proxy_server ============

proxy_server::proxy_server(asio::io_context& io_context, short port,
                             route_engine& router, user_manager& users)
    : m_acceptor(io_context, tcp::endpoint(tcp::v4(), port)),
      m_router(router),
      m_users(users) {
    structured_logger::instance().info("proxy_server", "Listening on port " + std::to_string(port));
    do_accept();
}

void proxy_server::stop() {
    m_running = false;
    std::error_code ec;
    m_acceptor.close(ec);
    structured_logger::instance().info("proxy_server", "Stopped");
}

void proxy_server::do_accept() {
    m_acceptor.async_accept(
        [this](std::error_code ec, tcp::socket socket) {
            if (!ec && m_running) {
                std::make_shared<session>(std::move(socket), m_router, m_users)->start();
            }
            if (m_running) do_accept();
        });
}

// ============ server_app ============

void server_app::start(int port, int thread_count) {
    if (m_running) return;
    m_port = port;

    m_io_context = std::make_unique<asio::io_context>();
    m_work_guard.emplace(asio::make_work_guard(*m_io_context));
    m_server = std::make_unique<proxy_server>(*m_io_context, m_port,
        route_engine::instance(), user_manager::instance());

    for (int i = 0; i < thread_count; ++i) {
        m_threads.emplace_back([this]() {
            m_io_context->run();
        });
    }
    m_running = true;
    structured_logger::instance().info("server_app",
        "Started on port " + std::to_string(m_port) +
        " with " + std::to_string(thread_count) + " threads");
}

void server_app::stop() {
    if (!m_running) return;
    m_running = false;
    if (m_server) m_server->stop();
    m_work_guard.reset();
    if (m_io_context) m_io_context->stop();
    for (auto& t : m_threads) {
        if (t.joinable()) t.join();
    }
    m_threads.clear();
    m_server.reset();
    m_io_context.reset();
    structured_logger::instance().info("server_app", "Shutdown complete");
}
