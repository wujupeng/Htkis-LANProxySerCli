#pragma once
#include <string>
#include <map>
#include <mutex>
#include <chrono>

class jwt_util {
public:
    static void set_secret(const std::string& secret);
    static std::string sign(const std::string& username, const std::string& role = "user", int expire_minutes = 30);
    static bool verify(const std::string& token);
    static std::string get_username(const std::string& token);
    static std::string get_role(const std::string& token);

private:
    static std::string m_secret;
    static std::string base64_encode(const unsigned char* data, size_t len);
    static std::string base64_decode(const std::string& input);
    static std::string hmac_sha256(const std::string& data, const std::string& key);
};
