#include "web_server/api_router.h"
#include "web_server/auth_middleware.h"
#include "web_server/jwt_util.h"
#include "web_server/ws_handler.h"

#define AUTH_GUARD(req, res) if (!auth_check::require_auth(req, res)) return;
#include "config_manager/config_loader.h"
#include "user_manager/user_manager.h"
#include "user_manager/password_hasher.h"
#include "route_decision/route_engine.h"
#include "route_decision/rule_manager.h"
#include "v2rayn_manager/v2rayn_process.h"
#include "v2rayn_manager/v2rayn_config.h"
#include "v2rayn_manager/vm_node_manager.h"
#include "v2rayn_manager/vm_link_codec.h"
#include "v2rayn_manager/vm_node_validator.h"
#include "log_monitor/structured_logger.h"
#include "log_monitor/metrics_collector.h"
#include "log_monitor/audit_logger.h"
#include "proxy_core/session.h"
#include "concurrency/load_balancer.h"
#include "concurrency/node_health_checker.h"
#include "concurrency/rate_limiter.h"
#include "concurrency/overload_protector.h"
#include "concurrency/connection_admitter.h"
#include "concurrency/connection_registry.h"
#include "concurrency/failover_handler.h"
#include "concurrency/timeout_scanner.h"
#include "concurrency/concurrency_metrics.h"
#include "concurrency/system_tuner.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>

static std::string read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static std::string get_mime_type(const std::string& path) {
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".css") return "text/css";
    if (path.size() >= 3 && path.substr(path.size() - 3) == ".js") return "application/javascript";
    if (path.size() >= 5 && path.substr(path.size() - 5) == ".json") return "application/json";
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".png") return "image/png";
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".svg") return "image/svg+xml";
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".ico") return "image/x-icon";
    if (path.size() >= 5 && path.substr(path.size() - 5) == ".woff") return "font/woff";
    if (path.size() >= 6 && path.substr(path.size() - 6) == ".woff2") return "font/woff2";
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".map") return "application/json";
    return "text/html";
}

static std::mutex g_login_mutex;
static std::unordered_map<std::string, int> g_login_fail_count;
static std::unordered_map<std::string, int64_t> g_login_lock_until;

static void cleanup_expired_login_records() {
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    for (auto it = g_login_lock_until.begin(); it != g_login_lock_until.end();) {
        if (now >= it->second) it = g_login_lock_until.erase(it);
        else ++it;
    }
    if (g_login_fail_count.size() > 1000) {
        g_login_fail_count.clear();
    }
}

void register_api_routes(crow::SimpleApp& app) {

    CROW_ROUTE(app, "/api/auth/login").methods("POST"_method)
    ([](const crow::request& req) {
        try {
            auto body = nlohmann::json::parse(req.body);
            std::string username = body.value("username", "");
            std::string password = body.value("password", "");

            std::lock_guard<std::mutex> lock(g_login_mutex);
            cleanup_expired_login_records();
            auto it = g_login_lock_until.find(username);
            if (it != g_login_lock_until.end()) {
                auto now = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                if (now < it->second) {
                    return crow::response(429, "{\"error\":\"locked\",\"remaining\":" +
                        std::to_string(it->second - now) + "}");
                }
            }

            auto& cfg = config_loader::instance().config();
            if (username == cfg.admin_username &&
                password_hasher::verify(password, cfg.admin_password_hash)) {
                g_login_fail_count.erase(username);
                g_login_lock_until.erase(username);

                std::string token = jwt_util::sign(username);
                audit_logger::instance().log(username, "login", "system");

                nlohmann::json resp;
                resp["token"] = token;
                resp["username"] = username;
                return crow::response(resp.dump());
            }

            if (username != cfg.admin_username &&
                user_manager::instance().authenticate(username, password)) {
                auto info = user_manager::instance().get_user_info(username);
                if (!info.active) {
                    return crow::response(403, "{\"error\":\"account_disabled\"}");
                }
                auto now_sec = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                if (info.expire_time > 0 && now_sec >= info.expire_time) {
                    return crow::response(403, "{\"error\":\"account_expired\"}");
                }

                g_login_fail_count.erase(username);
                g_login_lock_until.erase(username);

                std::string token = jwt_util::sign(username);
                audit_logger::instance().log(username, "login", "user");

                nlohmann::json resp;
                resp["token"] = token;
                resp["username"] = username;
                return crow::response(resp.dump());
            }

            g_login_fail_count[username]++;
            if (g_login_fail_count[username] >= 5) {
                auto now = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                g_login_lock_until[username] = now + 900;
                g_login_fail_count.erase(username);
            }

            return crow::response(401, "{\"error\":\"invalid_credentials\"}");
        } catch (...) {
            return crow::response(400, "{\"error\":\"bad_request\"}");
        }
    });

    CROW_ROUTE(app, "/api/auth/logout").methods("POST"_method)
    ([](const crow::request& req, crow::response& res) {
        AUTH_GUARD(req, res);
        std::string token;
        if (req.headers.count("Authorization")) {
            token = req.get_header_value("Authorization");
            if (token.substr(0, 7) == "Bearer ") token = token.substr(7);
        }
        std::string username = jwt_util::get_username(token);
        audit_logger::instance().log(username, "logout", "system");
        res.write("{\"ok\":true}");
        res.end();
    });

    CROW_ROUTE(app, "/api/dashboard/stats")
    ([](const crow::request& req, crow::response& res) {
        AUTH_GUARD(req, res);
        auto metrics = metrics_collector::instance().snapshot();
        metrics["proxy_running"] = server_app::instance().is_running();
        metrics["proxy_port"] = server_app::instance().get_port();
        metrics["v2rayn_status"] = v2rayn_process::instance().status() == v2rayn_status::running
                                    ? "running" : "stopped";
        res.write(metrics.dump());
        res.end();
    });

    CROW_ROUTE(app, "/api/dashboard/recent_logs")
    ([](const crow::request& req, crow::response& res) {
        AUTH_GUARD(req, res);
        int count = 50;
        if (req.url_params.get("count")) count = std::stoi(req.url_params.get("count"));
        auto logs = structured_logger::instance().get_recent_logs(count);
        res.write(logs.dump());
        res.end();
    });

    CROW_ROUTE(app, "/api/users").methods("GET"_method)
    ([](const crow::request& req, crow::response& res) {
        AUTH_GUARD(req, res);
        int page = 1, page_size = 20;
        if (req.url_params.get("page")) page = std::stoi(req.url_params.get("page"));
        if (req.url_params.get("page_size")) page_size = std::stoi(req.url_params.get("page_size"));

        auto users = user_manager::instance().list_users_paginated(page, page_size);
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& u : users) {
            nlohmann::json j;
            j["username"] = u.username;
            j["expire_time"] = (int64_t)u.expire_time;
            j["active"] = u.active;
            arr.push_back(j);
        }
        nlohmann::json resp;
        resp["users"] = arr;
        resp["page"] = page;
        resp["page_size"] = page_size;
        res.write(resp.dump());
        res.end();
    });

    CROW_ROUTE(app, "/api/users").methods("POST"_method)
    ([](const crow::request& req, crow::response& res) {
        AUTH_GUARD(req, res);
        try {
            auto body = nlohmann::json::parse(req.body);
            std::string username = body.value("username", "");
            std::string password = body.value("password", "");
            int days = body.value("days", 0);

            if (user_manager::instance().add_user(username, password, days)) {
                user_manager::instance().save();
                std::string token = req.headers.count("Authorization")
                    ? req.get_header_value("Authorization") : "";
                audit_logger::instance().log("admin", "add_user", username);
                res.write("{\"ok\":true}");
                res.end();
                return;
            }
            res.code = 409;
            res.write("{\"error\":\"username_exists\"}");
            res.end();
        } catch (...) {
            res.code = 400;
            res.write("{\"error\":\"bad_request\"}");
            res.end();
        }
    });

    CROW_ROUTE(app, "/api/users/<string>").methods("PUT"_method)
    ([](const crow::request& req, crow::response& res, const std::string& username) {
        AUTH_GUARD(req, res);
        try {
            auto body = nlohmann::json::parse(req.body);
            if (body.contains("active")) {
                user_manager::instance().set_status(username, body["active"]);
            }
            if (body.contains("password")) {
                user_manager::instance().update_password(username, body["password"]);
            }
            user_manager::instance().save();
            audit_logger::instance().log("admin", "update_user", username);
            res.write("{\"ok\":true}");
            res.end();
        } catch (...) {
            res.code = 400;
            res.write("{\"error\":\"bad_request\"}");
            res.end();
        }
    });

    CROW_ROUTE(app, "/api/users/<string>").methods("DELETE"_method)
    ([](const crow::request& req, crow::response& res, const std::string& username) {
        AUTH_GUARD(req, res);
        if (user_manager::instance().remove_user(username)) {
            user_manager::instance().save();
            audit_logger::instance().log("admin", "delete_user", username);
            res.write("{\"ok\":true}");
            res.end();
        } else {
            res.code = 404;
            res.write("{\"error\":\"not_found\"}");
            res.end();
        }
    });

    CROW_ROUTE(app, "/api/rules")
    ([](const crow::request& req, crow::response& res) {
        AUTH_GUARD(req, res);
        auto rules = rule_manager().get_rules();
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& r : rules) {
            nlohmann::json j;
            j["rule_id"] = r.rule_id;
            j["rule_type"] = r.rule_type;
            j["pattern"] = r.pattern;
            j["action"] = (r.action == route_action::direct) ? "direct" : "proxy";
            j["priority"] = r.priority;
            j["enabled"] = r.enabled;
            j["is_builtin"] = r.is_builtin;
            arr.push_back(j);
        }
        res.write(arr.dump());
        res.end();
    });

    CROW_ROUTE(app, "/api/rules").methods("POST"_method)
    ([](const crow::request& req, crow::response& res) {
        AUTH_GUARD(req, res);
        try {
            auto body = nlohmann::json::parse(req.body);
            route_rule rule;
            rule.rule_id = body.value("rule_id", "");
            rule.rule_type = body.value("rule_type", "");
            rule.pattern = body.value("pattern", "");
            rule.action = (body.value("action", "direct") == "direct")
                          ? route_action::direct : route_action::proxy;
            rule.priority = body.value("priority", 0);
            rule.enabled = body.value("enabled", true);

            rule_manager().add_rule(rule, config_loader::instance().config().custom_rules_path);
            res.write("{\"ok\":true}");
            res.end();
        } catch (...) {
            res.code = 400;
            res.write("{\"error\":\"bad_request\"}");
            res.end();
        }
    });

    CROW_ROUTE(app, "/api/rules/reload").methods("POST"_method)
    ([](const crow::request& req, crow::response& res) {
        AUTH_GUARD(req, res);
        route_engine::instance().reload_rules();
        res.write("{\"ok\":true}");
        res.end();
    });

    CROW_ROUTE(app, "/api/v2rayn/status")
    ([](const crow::request& req, crow::response& res) {
        AUTH_GUARD(req, res);
        auto& vp = v2rayn_process::instance();
        nlohmann::json j;
        j["status"] = vp.status() == v2rayn_status::running ? "running" :
                      vp.status() == v2rayn_status::starting ? "starting" :
                      vp.status() == v2rayn_status::failed ? "failed" : "stopped";
        j["pid"] = vp.pid();
        j["crash_count"] = vp.crash_count();
        res.write(j.dump());
        res.end();
    });

    CROW_ROUTE(app, "/api/v2rayn/start").methods("POST"_method)
    ([](const crow::request& req, crow::response& res) {
        AUTH_GUARD(req, res);
        auto& cfg = config_loader::instance().config();
        v2rayn_process::instance().start(
            cfg.v2rayn_executable_path, cfg.v2rayn_config_path,
            cfg.log_file_path + "v2rayn.log");
        res.write("{\"ok\":true}");
        res.end();
    });

    CROW_ROUTE(app, "/api/v2rayn/stop").methods("POST"_method)
    ([](const crow::request& req, crow::response& res) {
        AUTH_GUARD(req, res);
        v2rayn_process::instance().stop();
        res.write("{\"ok\":true}");
        res.end();
    });

    CROW_ROUTE(app, "/api/v2rayn/restart").methods("POST"_method)
    ([](const crow::request& req, crow::response& res) {
        AUTH_GUARD(req, res);
        v2rayn_process::instance().restart();
        res.write("{\"ok\":true}");
        res.end();
    });

    CROW_ROUTE(app, "/api/v2rayn/config")
    ([](const crow::request& req, crow::response& res) {
        AUTH_GUARD(req, res);
        auto& cfg = config_loader::instance().config();
        auto config = v2rayn_config::load(cfg.v2rayn_config_path);
        auto desensitized = v2rayn_config::desensitize(config);
        res.write(desensitized.dump(4));
        res.end();
    });

    CROW_ROUTE(app, "/api/v2rayn/config").methods("PUT"_method)
    ([](const crow::request& req, crow::response& res) {
        AUTH_GUARD(req, res);
        try {
            auto config = nlohmann::json::parse(req.body);
            if (!v2rayn_config::validate(config)) {
                res.code = 400;
                res.write("{\"error\":\"invalid_config\"}");
                res.end();
                return;
            }
            auto& cfg = config_loader::instance().config();
            v2rayn_config::save(cfg.v2rayn_config_path, config);
            res.write("{\"ok\":true}");
            res.end();
        } catch (...) {
            res.code = 400;
            res.write("{\"error\":\"bad_request\"}");
            res.end();
        }
    });

    CROW_ROUTE(app, "/api/settings")
    ([](const crow::request& req, crow::response& res) {
        AUTH_GUARD(req, res);
        auto& cfg = config_loader::instance().config();
        nlohmann::json j;
        j["proxy_port"] = cfg.proxy_port;
        j["web_ui_port"] = cfg.web_ui_port;
        j["web_ui_https_enabled"] = cfg.web_ui_https_enabled;
        j["default_route_action"] = cfg.default_route_action;
        j["proxy_thread_count"] = cfg.proxy_thread_count;
        j["v2rayn_executable_path"] = cfg.v2rayn_executable_path;
        j["v2rayn_config_path"] = cfg.v2rayn_config_path;
        j["v2rayn_local_socks_port"] = cfg.v2rayn_local_socks_port;
        j["v2rayn_local_http_port"] = cfg.v2rayn_local_http_port;
        j["admin_username"] = cfg.admin_username;
        j["geoip_db_path"] = cfg.geoip_db_path;
        j["log_file_path"] = cfg.log_file_path;
        j["user_data_path"] = cfg.user_data_path;
        res.write(j.dump());
        res.end();
    });

    CROW_ROUTE(app, "/api/settings").methods("PUT"_method)
    ([](const crow::request& req, crow::response& res) {
        AUTH_GUARD(req, res);
        try {
            auto body = nlohmann::json::parse(req.body);
            auto& cfg = config_loader::instance().config();

            bool need_restart = false;
            if (body.contains("proxy_port") && body["proxy_port"] != cfg.proxy_port)
                need_restart = true;
            if (body.contains("web_ui_port") && body["web_ui_port"] != cfg.web_ui_port)
                need_restart = true;

            if (body.contains("proxy_port")) cfg.proxy_port = body["proxy_port"];
            if (body.contains("web_ui_port")) cfg.web_ui_port = body["web_ui_port"];
            if (body.contains("default_route_action")) {
                cfg.default_route_action = body["default_route_action"];
                route_engine::instance().set_default_action(cfg.default_route_action);
            }
            if (body.contains("proxy_thread_count")) cfg.proxy_thread_count = body["proxy_thread_count"];
            if (body.contains("v2rayn_executable_path")) cfg.v2rayn_executable_path = body["v2rayn_executable_path"];
            if (body.contains("v2rayn_local_socks_port")) cfg.v2rayn_local_socks_port = body["v2rayn_local_socks_port"];
            if (body.contains("v2rayn_local_http_port")) cfg.v2rayn_local_http_port = body["v2rayn_local_http_port"];

            config_loader::instance().save();

            nlohmann::json resp;
            resp["ok"] = true;
            resp["need_restart"] = need_restart;
            res.write(resp.dump());
            res.end();
        } catch (...) {
            res.code = 400;
            res.write("{\"error\":\"bad_request\"}");
            res.end();
        }
    });

    CROW_ROUTE(app, "/api/logs")
    ([](const crow::request& req, crow::response& res) {
        AUTH_GUARD(req, res);
        int count = 100;
        std::string level_filter;
        if (req.url_params.get("count")) count = std::stoi(req.url_params.get("count"));
        if (req.url_params.get("level")) level_filter = req.url_params.get("level");
        auto logs = structured_logger::instance().get_recent_logs(count, level_filter);
        res.write(logs.dump());
        res.end();
    });

    CROW_ROUTE(app, "/health")
    ([]() {
        nlohmann::json j;
        j["status"] = "ok";
        j["proxy_running"] = server_app::instance().is_running();
        j["v2rayn_running"] = v2rayn_process::instance().status() == v2rayn_status::running;
        return crow::response(j.dump());
    });

    CROW_WEBSOCKET_ROUTE(app, "/ws")
        .onopen([](crow::websocket::connection& conn) {
            ws_handler::instance().on_open(conn);
        })
        .onclose([](crow::websocket::connection& conn, const std::string& reason) {
            ws_handler::instance().on_close(conn, reason);
        })
        .onmessage([](crow::websocket::connection& conn, const std::string& data, bool is_binary) {
            ws_handler::instance().on_message(conn, data, is_binary);
        });

    CROW_ROUTE(app, "/")
    ([]() {
        std::string content = read_file(CROW_STATIC_DIRECTORY "index.html");
        if (content.empty()) return crow::response(404, "Not found");
        crow::response resp(content);
        resp.set_header("Content-Type", "text/html");
        return resp;
    });

    CROW_ROUTE(app, "/assets/<path>")
    ([](const std::string& file_path) {
        if (file_path.find("..") != std::string::npos) {
            return crow::response(403, "Forbidden");
        }
        std::string full_path = CROW_STATIC_DIRECTORY "assets/" + file_path;
        std::string content = read_file(full_path);
        if (content.empty()) return crow::response(404, "Not found");
        crow::response resp(content);
        resp.set_header("Content-Type", get_mime_type(file_path));
        return resp;
    });

    // === VMess Node Management APIs ===
    CROW_ROUTE(app, "/api/v2rayn/vmess/nodes").methods("GET"_method)
    ([](const crow::request& req, crow::response& res) {
        AUTH_GUARD(req, res);
        auto& mgr = vm_node_manager::instance();
        auto nodes = mgr.list_nodes(true);
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& n : nodes) {
            arr.push_back(mgr.node_to_json(n, false));
        }
        nlohmann::json resp;
        resp["nodes"] = arr;
        resp["active_node"] = mgr.get_active_node();
        res.write(resp.dump());
        res.end();
    });

    CROW_ROUTE(app, "/api/v2rayn/vmess/nodes/<string>").methods("GET"_method)
    ([](const crow::request& req, crow::response& res, const std::string& tag) {
        AUTH_GUARD(req, res);
        auto& mgr = vm_node_manager::instance();
        auto node = mgr.get_node(tag);
        if (!node.has_value()) {
            res.code = 404;
            res.write("{\"error\":\"not_found\"}");
            res.end();
            return;
        }
        res.write(mgr.node_to_json(*node, false).dump());
        res.end();
    });

    CROW_ROUTE(app, "/api/v2rayn/vmess/nodes").methods("POST"_method)
    ([](const crow::request& req, crow::response& res) {
        AUTH_GUARD(req, res);
        try {
            auto body = nlohmann::json::parse(req.body);
            auto& mgr = vm_node_manager::instance();
            vm_node node = mgr.json_to_node(body);
            std::string error_msg;
            if (mgr.add_node(node, error_msg)) {
                nlohmann::json resp;
                resp["ok"] = true;
                resp["tag"] = node.tag;
                res.write(resp.dump());
                res.end();
                return;
            }
            res.code = 400;
            res.write("{\"error\":\"" + error_msg + "\"}");
            res.end();
        } catch (...) {
            res.code = 400;
            res.write("{\"error\":\"bad_request\"}");
            res.end();
        }
    });

    CROW_ROUTE(app, "/api/v2rayn/vmess/nodes/<string>").methods("PUT"_method)
    ([](const crow::request& req, crow::response& res, const std::string& old_tag) {
        AUTH_GUARD(req, res);
        try {
            auto body = nlohmann::json::parse(req.body);
            auto& mgr = vm_node_manager::instance();
            vm_node node = mgr.json_to_node(body);
            std::string error_msg;
            if (mgr.update_node(old_tag, node, error_msg)) {
                res.write("{\"ok\":true}");
                res.end();
                return;
            }
            res.code = 400;
            res.write("{\"error\":\"" + error_msg + "\"}");
            res.end();
        } catch (...) {
            res.code = 400;
            res.write("{\"error\":\"bad_request\"}");
            res.end();
        }
    });

    CROW_ROUTE(app, "/api/v2rayn/vmess/nodes/<string>").methods("DELETE"_method)
    ([](const crow::request& req, crow::response& res, const std::string& tag) {
        AUTH_GUARD(req, res);
        auto& mgr = vm_node_manager::instance();
        std::string error_msg;
        if (mgr.remove_node(tag, error_msg)) {
            res.write("{\"ok\":true}");
            res.end();
            return;
        }
        res.code = 400;
        res.write("{\"error\":\"" + error_msg + "\"}");
        res.end();
    });

    CROW_ROUTE(app, "/api/v2rayn/vmess/nodes/reorder").methods("PUT"_method)
    ([](const crow::request& req, crow::response& res) {
        AUTH_GUARD(req, res);
        try {
            auto body = nlohmann::json::parse(req.body);
            std::vector<std::string> tags;
            for (const auto& t : body["ordered_tags"]) {
                tags.push_back(t.get<std::string>());
            }
            vm_node_manager::instance().reorder_nodes(tags);
            res.write("{\"ok\":true}");
            res.end();
        } catch (...) {
            res.code = 400;
            res.write("{\"error\":\"bad_request\"}");
            res.end();
        }
    });

    CROW_ROUTE(app, "/api/v2rayn/vmess/apply").methods("POST"_method)
    ([](const crow::request& req, crow::response& res) {
        AUTH_GUARD(req, res);
        auto& mgr = vm_node_manager::instance();
        std::string error_msg;

        try {
            auto body = nlohmann::json::parse(req.body);
            std::string node_tag = body.value("node_tag", "");
            if (!node_tag.empty()) {
                if (!mgr.set_active_node(node_tag, error_msg)) {
                    res.code = 400;
                    res.write("{\"error\":\"" + error_msg + "\"}");
                    res.end();
                    return;
                }
            }
        } catch (...) {}

        if (mgr.apply_config(error_msg)) {
            nlohmann::json resp;
            resp["ok"] = true;
            resp["active_node"] = mgr.get_active_node();
            res.write(resp.dump());
            res.end();
            return;
        }
        res.code = 500;
        res.write("{\"error\":\"" + error_msg + "\"}");
        res.end();
    });

    CROW_ROUTE(app, "/api/v2rayn/vmess/active").methods("GET"_method)
    ([](const crow::request& req, crow::response& res) {
        AUTH_GUARD(req, res);
        auto& mgr = vm_node_manager::instance();
        nlohmann::json resp;
        resp["active_node"] = mgr.get_active_node();
        res.write(resp.dump());
        res.end();
    });

    CROW_ROUTE(app, "/api/v2rayn/vmess/active").methods("POST"_method)
    ([](const crow::request& req, crow::response& res) {
        AUTH_GUARD(req, res);
        try {
            auto body = nlohmann::json::parse(req.body);
            std::string node_tag = body.value("node_tag", "");
            if (node_tag.empty()) {
                res.code = 400;
                res.write("{\"error\":\"node_tag不能为空\"}");
                res.end();
                return;
            }
            auto& mgr = vm_node_manager::instance();
            std::string error_msg;
            if (!mgr.set_active_node(node_tag, error_msg)) {
                res.code = 400;
                res.write("{\"error\":\"" + error_msg + "\"}");
                res.end();
                return;
            }
            if (!mgr.apply_config(error_msg)) {
                res.code = 500;
                res.write("{\"error\":\"" + error_msg + "\"}");
                res.end();
                return;
            }
            nlohmann::json resp;
            resp["ok"] = true;
            resp["active_node"] = mgr.get_active_node();
            res.write(resp.dump());
            res.end();
        } catch (...) {
            res.code = 400;
            res.write("{\"error\":\"bad_request\"}");
            res.end();
        }
    });

    CROW_ROUTE(app, "/api/v2rayn/vmess/parse").methods("POST"_method)
    ([](const crow::request& req, crow::response& res) {
        AUTH_GUARD(req, res);
        try {
            auto body = nlohmann::json::parse(req.body);
            std::string link = body.value("vmess_link", "");
            if (link.empty()) {
                res.code = 400;
                res.write("{\"error\":\"链接不能为空\"}");
                res.end();
                return;
            }
            std::string error_msg;
            auto node = vm_link_codec::decode(link, error_msg);
            if (node.has_value()) {
                nlohmann::json resp;
                resp["ok"] = true;
                resp["node"] = vm_node_manager::instance().node_to_json(*node, false);
                res.write(resp.dump());
                res.end();
                return;
            }
            res.code = 400;
            res.write("{\"error\":\"" + error_msg + "\"}");
            res.end();
        } catch (...) {
            res.code = 400;
            res.write("{\"error\":\"bad_request\"}");
            res.end();
        }
    });

    CROW_ROUTE(app, "/api/v2rayn/vmess/import").methods("POST"_method)
    ([](const crow::request& req, crow::response& res) {
        AUTH_GUARD(req, res);
        try {
            auto body = nlohmann::json::parse(req.body);
            std::string link = body.value("vmess_link", "");
            if (link.empty()) {
                res.code = 400;
                res.write("{\"error\":\"链接不能为空\"}");
                res.end();
                return;
            }
            auto& mgr = vm_node_manager::instance();
            std::string error_msg;
            auto node = mgr.import_from_link(link, error_msg);
            if (node.has_value()) {
                nlohmann::json resp;
                resp["ok"] = true;
                resp["node"] = mgr.node_to_json(*node, false);
                res.write(resp.dump());
                res.end();
                return;
            }
            res.code = 400;
            res.write("{\"error\":\"" + error_msg + "\"}");
            res.end();
        } catch (...) {
            res.code = 400;
            res.write("{\"error\":\"bad_request\"}");
            res.end();
        }
    });

    CROW_ROUTE(app, "/api/v2rayn/vmess/export/<string>").methods("POST"_method)
    ([](const crow::request& req, crow::response& res, const std::string& tag) {
        AUTH_GUARD(req, res);
        auto& mgr = vm_node_manager::instance();
        auto node = mgr.get_node(tag);
        if (!node.has_value()) {
            res.code = 404;
            res.write("{\"error\":\"not_found\"}");
            res.end();
            return;
        }
        nlohmann::json resp;
        resp["vmess_link"] = mgr.export_to_link(*node);
        res.write(resp.dump());
        res.end();
    });

    // ===== Concurrency Monitoring & Stats API =====

    CROW_ROUTE(app, "/api/concurrency/stats").methods("GET"_method)
    ([](const crow::request& req, crow::response& res) {
        AUTH_GUARD(req, res);
        auto stats = concurrency_metrics::instance().get_global_stats();
        nlohmann::json j;
        j["active_connections"] = stats.active_connections;
        j["total_connections"] = stats.total_connections;
        j["queued_connections"] = stats.queued_connections;
        j["max_connections"] = connection_admitter::instance().get_active_connections();
        j["is_overloaded"] = stats.is_overloaded;
        res.write(j.dump());
        res.end();
    });

    CROW_ROUTE(app, "/api/concurrency/nodes").methods("GET"_method)
    ([](const crow::request& req, crow::response& res) {
        AUTH_GUARD(req, res);
        auto nodes = concurrency_metrics::instance().get_all_node_metrics();
        nlohmann::json j = nlohmann::json::array();
        for (const auto& n : nodes) {
            nlohmann::json nj;
            nj["tag"] = n.tag;
            nj["active_connections"] = n.active_connections;
            nj["total_connections"] = n.total_connections;
            nj["bytes_up"] = n.bytes_up;
            nj["bytes_down"] = n.bytes_down;
            nj["latency_ms"] = n.latency_ms;
            nj["health"] = n.health;
            j.push_back(nj);
        }
        res.write(j.dump());
        res.end();
    });

    CROW_ROUTE(app, "/api/concurrency/users").methods("GET"_method)
    ([](const crow::request& req, crow::response& res) {
        AUTH_GUARD(req, res);
        int page = 1, size = 50;
        if (req.url_params.get("page")) page = std::stoi(req.url_params.get("page"));
        if (req.url_params.get("size")) size = std::stoi(req.url_params.get("size"));
        auto users = concurrency_metrics::instance().get_all_user_metrics();
        nlohmann::json j = nlohmann::json::array();
        int start = (page - 1) * size;
        int end = std::min(start + size, (int)users.size());
        for (int i = start; i < end; ++i) {
            nlohmann::json uj;
            uj["username"] = users[i].username;
            uj["active_connections"] = users[i].active_connections;
            uj["total_connections"] = users[i].total_connections;
            uj["bytes_up"] = users[i].bytes_up;
            uj["bytes_down"] = users[i].bytes_down;
            j.push_back(uj);
        }
        res.write(j.dump());
        res.end();
    });

    CROW_ROUTE(app, "/api/concurrency/connections").methods("GET"_method)
    ([](const crow::request& req, crow::response& res) {
        AUTH_GUARD(req, res);
        auto conns = connection_registry::instance().get_all();
        nlohmann::json j = nlohmann::json::array();
        std::string filter_node = req.url_params.get("node") ? req.url_params.get("node") : "";
        std::string filter_user = req.url_params.get("user") ? req.url_params.get("user") : "";
        int limit = req.url_params.get("limit") ? std::stoi(req.url_params.get("limit")) : 100;
        int count = 0;
        for (const auto& c : conns) {
            if (!filter_node.empty() && c.assigned_node_tag != filter_node) continue;
            if (!filter_user.empty() && c.username != filter_user) continue;
            if (count >= limit) break;
            nlohmann::json cj;
            cj["session_id"] = c.session_id;
            cj["client_ip"] = c.client_ip;
            cj["username"] = c.username;
            cj["node_tag"] = c.assigned_node_tag;
            cj["state"] = static_cast<int>(c.state);
            j.push_back(cj);
            count++;
        }
        res.write(j.dump());
        res.end();
    });

    CROW_ROUTE(app, "/api/concurrency/history").methods("GET"_method)
    ([](const crow::request& req, crow::response& res) {
        AUTH_GUARD(req, res);
        int minutes = req.url_params.get("minutes") ? std::stoi(req.url_params.get("minutes")) : 1440;
        auto history = concurrency_metrics::instance().get_history(minutes);
        nlohmann::json j = nlohmann::json::array();
        for (const auto& snap : history) {
            nlohmann::json sj;
            sj["active_connections"] = snap.active_connections;
            sj["queued_connections"] = snap.queued_connections;
            nlohmann::json nc;
            for (const auto& [tag, count] : snap.node_connections) {
                nc[tag] = count;
            }
            sj["node_connections"] = nc;
            j.push_back(sj);
        }
        res.write(j.dump());
        res.end();
    });

    // ===== Schedule Strategy API =====

    CROW_ROUTE(app, "/api/concurrency/schedule").methods("GET"_method)
    ([](const crow::request& req, crow::response& res) {
        AUTH_GUARD(req, res);
        auto& lb = load_balancer::instance();
        nlohmann::json j;
        j["strategy"] = load_balancer::strategy_to_string(lb.get_strategy());
        j["ip_affinity_enabled"] = lb.is_ip_affinity_enabled();
        j["node_capacity_threshold"] = lb.get_node_capacity_threshold();
        res.write(j.dump());
        res.end();
    });

    CROW_ROUTE(app, "/api/concurrency/schedule").methods("PUT"_method)
    ([](const crow::request& req, crow::response& res) {
        AUTH_GUARD(req, res);
        try {
            auto body = nlohmann::json::parse(req.body);
            if (!body.contains("strategy")) {
                res.code = 400;
                res.write("{\"error\":\"missing strategy\"}");
                res.end();
                return;
            }
            auto strategy = load_balancer::strategy_from_string(body["strategy"]);
            load_balancer::instance().set_strategy(strategy);
            res.write("{\"ok\":true}");
            res.end();
        } catch (...) {
            res.code = 400;
            res.write("{\"error\":\"bad_request\"}");
            res.end();
        }
    });

    CROW_ROUTE(app, "/api/concurrency/ip-affinity").methods("PUT"_method)
    ([](const crow::request& req, crow::response& res) {
        AUTH_GUARD(req, res);
        try {
            auto body = nlohmann::json::parse(req.body);
            if (body.contains("enabled")) {
                load_balancer::instance().set_ip_affinity_enabled(body["enabled"]);
            }
            res.write("{\"ok\":true}");
            res.end();
        } catch (...) {
            res.code = 400;
            res.write("{\"error\":\"bad_request\"}");
            res.end();
        }
    });

    CROW_ROUTE(app, "/api/concurrency/node-weight/<string>").methods("PUT"_method)
    ([](const crow::request& req, crow::response& res, const std::string& tag) {
        AUTH_GUARD(req, res);
        try {
            auto body = nlohmann::json::parse(req.body);
            if (!body.contains("weight")) {
                res.code = 400;
                res.write("{\"error\":\"missing weight\"}");
                res.end();
                return;
            }
            int weight = body["weight"];
            if (weight < 1 || weight > 100) {
                res.code = 400;
                res.write("{\"error\":\"weight must be 1-100\"}");
                res.end();
                return;
            }
            load_balancer::instance().set_node_weight(tag, weight);
            res.write("{\"ok\":true}");
            res.end();
        } catch (...) {
            res.code = 400;
            res.write("{\"error\":\"bad_request\"}");
            res.end();
        }
    });

    // ===== Health Check API =====

    CROW_ROUTE(app, "/api/concurrency/health").methods("GET"_method)
    ([](const crow::request& req, crow::response& res) {
        AUTH_GUARD(req, res);
        auto health_list = node_health_checker::instance().get_all_info();
        nlohmann::json j = nlohmann::json::array();
        for (const auto& h : health_list) {
            nlohmann::json hj;
            hj["tag"] = h.tag;
            hj["status"] = static_cast<int>(h.status);
            hj["latency_ms"] = h.latency_ms;
            hj["consecutive_failures"] = h.consecutive_failures;
            hj["total_checks"] = h.total_checks;
            hj["failed_checks"] = h.failed_checks;
            j.push_back(hj);
        }
        res.write(j.dump());
        res.end();
    });

    CROW_ROUTE(app, "/api/concurrency/health/check").methods("POST"_method)
    ([](const crow::request& req, crow::response& res) {
        AUTH_GUARD(req, res);
        auto nodes = vm_node_manager::instance().list_nodes(false);
        for (const auto& n : nodes) {
            node_health_checker::instance().check_now(n.tag);
        }
        res.write("{\"ok\":true,\"message\":\"health check triggered\"}");
        res.end();
    });

    // ===== Overload & Config API =====

    CROW_ROUTE(app, "/api/concurrency/overload").methods("GET"_method)
    ([](const crow::request& req, crow::response& res) {
        AUTH_GUARD(req, res);
        auto status = overload_protector::instance().get_status();
        nlohmann::json j;
        j["is_overloaded"] = status.is_overloaded;
        j["fd_usage_pct"] = status.fd_usage_pct;
        j["mem_usage_pct"] = status.mem_usage_pct;
        j["total_connections"] = status.total_connections;
        j["reason"] = status.reason;
        res.write(j.dump());
        res.end();
    });

    CROW_ROUTE(app, "/api/concurrency/config").methods("GET"_method)
    ([](const crow::request& req, crow::response& res) {
        AUTH_GUARD(req, res);
        auto& adm = connection_admitter::instance();
        nlohmann::json j;
        j["active_connections"] = adm.get_active_connections();
        j["queued_count"] = adm.get_queued_count();
        res.write(j.dump());
        res.end();
    });

    // ===== System Tuning API =====

    CROW_ROUTE(app, "/api/concurrency/tuning").methods("GET"_method)
    ([](const crow::request& req, crow::response& res) {
        AUTH_GUARD(req, res);
        auto startup = system_tuner::instance().run_startup_check();
        auto runtime = system_tuner::instance().run_runtime_check();
        nlohmann::json j;
        nlohmann::json startup_arr = nlohmann::json::array();
        for (const auto& r : startup) {
            nlohmann::json rj;
            rj["item"] = r.item;
            rj["current_value"] = r.current_value;
            rj["recommend_value"] = r.recommend_value;
            rj["is_ok"] = r.is_ok;
            rj["suggestion"] = r.suggestion;
            startup_arr.push_back(rj);
        }
        j["startup"] = startup_arr;
        nlohmann::json runtime_arr = nlohmann::json::array();
        for (const auto& r : runtime) {
            nlohmann::json rj;
            rj["item"] = r.item;
            rj["current_value"] = r.current_value;
            rj["recommend_value"] = r.recommend_value;
            rj["is_ok"] = r.is_ok;
            rj["suggestion"] = r.suggestion;
            runtime_arr.push_back(rj);
        }
        j["runtime"] = runtime_arr;
        res.write(j.dump());
        res.end();
    });

    CROW_ROUTE(app, "/api/concurrency/tuning/script").methods("GET"_method)
    ([](const crow::request& req, crow::response& res) {
        AUTH_GUARD(req, res);
        auto script = system_tuner::instance().generate_tune_script();
        res.set_header("Content-Type", "text/x-shellscript");
        res.write(script);
        res.end();
    });

    // ===== Rate Limits API =====

    CROW_ROUTE(app, "/api/concurrency/rate-limits").methods("GET"_method)
    ([](const crow::request& req, crow::response& res) {
        AUTH_GUARD(req, res);
        nlohmann::json j;
        auto& cfg = config_loader::instance().config();
        j["max_conn_per_ip_per_sec"] = cfg.max_conn_per_ip_per_sec;
        j["max_auth_fails_per_min"] = cfg.max_auth_fails_per_min;
        j["ban_duration_sec"] = cfg.ban_duration_sec;
        res.write(j.dump());
        res.end();
    });

}
