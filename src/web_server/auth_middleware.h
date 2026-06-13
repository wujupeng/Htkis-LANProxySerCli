#pragma once
#include <string>
#include <crow.h>

class auth_check {
public:
    static bool is_public_path(const std::string& path);
    static bool require_auth(const crow::request& req, crow::response& res);
    static std::string extract_token(const crow::request& req);
};
