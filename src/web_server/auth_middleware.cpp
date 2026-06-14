#include "web_server/auth_middleware.h"
#include "web_server/jwt_util.h"
#include <string>

bool auth_check::is_public_path(const std::string& path) {
    if (path == "/api/auth/login" || path == "/health") return true;
    if (path == "/" || path == "/index.html") return true;
    if (path.find("/web/") == 0) return true;

    auto dot_pos = path.rfind('.');
    if (dot_pos == std::string::npos) return false;
    auto ext = path.substr(dot_pos);
    return ext == ".js" || ext == ".css" || ext == ".ico" ||
           ext == ".png" || ext == ".svg" || ext == ".woff" ||
           ext == ".woff2" || ext == ".ttf" || ext == ".map";
}

std::string auth_check::extract_token(const crow::request& req) {
    std::string token;
    if (req.headers.count("Authorization")) {
        std::string auth = req.get_header_value("Authorization");
        if (auth.length() > 7 && auth.substr(0, 7) == "Bearer ") {
            token = auth.substr(7);
        }
    }

    if (token.empty() && req.headers.count("Cookie")) {
        std::string cookie = req.get_header_value("Cookie");
        auto pos = cookie.find("token=");
        if (pos != std::string::npos) {
            auto end = cookie.find(';', pos);
            token = cookie.substr(pos + 6, end == std::string::npos ? end : end - pos - 6);
        }
    }

    return token;
}

bool auth_check::require_auth(const crow::request& req, crow::response& res) {
    if (is_public_path(req.url)) return true;

    std::string token = extract_token(req);
    if (token.empty() || !jwt_util::verify(token)) {
        res.code = 401;
        res.write("{\"error\":\"unauthorized\"}");
        res.end();
        return false;
    }
    return true;
}

bool auth_check::require_admin(const crow::request& req, crow::response& res) {
    if (!require_auth(req, res)) return false;

    std::string token = extract_token(req);
    std::string role = jwt_util::get_role(token);
    if (role != "admin") {
        res.code = 403;
        res.write("{\"error\":\"admin_required\"}");
        res.end();
        return false;
    }
    return true;
}
