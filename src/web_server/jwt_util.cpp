#include "web_server/jwt_util.h"
#include <openssl/hmac.h>
#include <openssl/crypto.h>
#include <nlohmann/json.hpp>
#include <cstring>
#include <vector>
#include <chrono>

std::string jwt_util::m_secret;

void jwt_util::set_secret(const std::string& secret) {
    m_secret = secret;
}

std::string jwt_util::base64_encode(const unsigned char* data, size_t len) {
    static const char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve((len + 2) / 3 * 4);

    for (size_t i = 0; i < len; i += 3) {
        uint32_t n = (uint32_t)data[i] << 16;
        if (i + 1 < len) n |= (uint32_t)data[i + 1] << 8;
        if (i + 2 < len) n |= data[i + 2];

        result.push_back(table[(n >> 18) & 0x3F]);
        result.push_back(table[(n >> 12) & 0x3F]);
        result.push_back((i + 1 < len) ? table[(n >> 6) & 0x3F] : '=');
        result.push_back((i + 2 < len) ? table[n & 0x3F] : '=');
    }

    while (!result.empty() && result.back() == '=') result.pop_back();
    return result;
}

std::string jwt_util::base64_decode(const std::string& input) {
    static bool initialized = false;
    static int table[256];
    if (!initialized) {
        for (int i = 0; i < 256; i++) table[i] = -1;
        for (int i = 0; i < 26; i++) { table['A'+i] = i; table['a'+i] = i+26; }
        for (int i = 0; i < 10; i++) table['0'+i] = i+52;
        table['+'] = 62; table['-'] = 62; table['/'] = 63; table['_'] = 63;
        initialized = true;
    }

    std::vector<uint8_t> result;
    int val = 0, valb = -8;
    for (unsigned char c : input) {
        if (c == '=') continue;
        int idx = table[c];
        if (idx == -1) continue;
        val = (val << 6) + idx;
        valb += 6;
        if (valb >= 0) {
            result.push_back(uint8_t((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return std::string(result.begin(), result.end());
}

std::string jwt_util::hmac_sha256(const std::string& data, const std::string& key) {
    unsigned char result[EVP_MAX_MD_SIZE];
    unsigned int result_len = 0;

    HMAC(EVP_sha256(), key.c_str(), (int)key.length(),
         reinterpret_cast<const unsigned char*>(data.c_str()), data.length(),
         result, &result_len);

    return std::string(reinterpret_cast<char*>(result), result_len);
}

std::string jwt_util::sign(const std::string& username, const std::string& role, int expire_minutes) {
    nlohmann::json header;
    header["alg"] = "HS256";
    header["typ"] = "JWT";

    nlohmann::json payload;
    payload["username"] = username;
    payload["role"] = role;
    payload["exp"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() + expire_minutes * 60;

    std::string header_b64 = base64_encode(
        reinterpret_cast<const unsigned char*>(header.dump().c_str()),
        header.dump().length());
    std::string payload_b64 = base64_encode(
        reinterpret_cast<const unsigned char*>(payload.dump().c_str()),
        payload.dump().length());

    std::string signing_input = header_b64 + "." + payload_b64;
    std::string sig = hmac_sha256(signing_input, m_secret);
    std::string sig_b64 = base64_encode(
        reinterpret_cast<const unsigned char*>(sig.c_str()), sig.length());

    return signing_input + "." + sig_b64;
}

bool jwt_util::verify(const std::string& token) {
    auto dot1 = token.find('.');
    auto dot2 = token.find('.', dot1 + 1);
    if (dot1 == std::string::npos || dot2 == std::string::npos) {
        return false;
    }

    std::string header_b64 = token.substr(0, dot1);
    std::string payload_b64 = token.substr(dot1 + 1, dot2 - dot1 - 1);
    std::string sig_b64 = token.substr(dot2 + 1);

    std::string signing_input = header_b64 + "." + payload_b64;
    std::string expected_sig = hmac_sha256(signing_input, m_secret);
    std::string expected_sig_b64 = base64_encode(
        reinterpret_cast<const unsigned char*>(expected_sig.c_str()),
        expected_sig.length());

    if (sig_b64.size() != expected_sig_b64.size()) {
        return false;
    }
    if (CRYPTO_memcmp(sig_b64.data(), expected_sig_b64.data(), sig_b64.size()) != 0) {
        return false;
    }

    try {
        std::string payload_json = base64_decode(payload_b64);
        auto payload = nlohmann::json::parse(payload_json);
        int64_t exp = payload.value("exp", (int64_t)0);
        auto now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        return now < exp;
    } catch (...) {
        return false;
    }
}

std::string jwt_util::get_username(const std::string& token) {
    if (!verify(token)) return "";

    auto dot1 = token.find('.');
    auto dot2 = token.find('.', dot1 + 1);
    if (dot1 == std::string::npos || dot2 == std::string::npos) return "";

    try {
        std::string payload_b64 = token.substr(dot1 + 1, dot2 - dot1 - 1);
        std::string payload_json = base64_decode(payload_b64);
        auto payload = nlohmann::json::parse(payload_json);
        return payload.value("username", "");
    } catch (...) {
        return "";
    }
}

std::string jwt_util::get_role(const std::string& token) {
    if (!verify(token)) return "";

    auto dot1 = token.find('.');
    auto dot2 = token.find('.', dot1 + 1);
    if (dot1 == std::string::npos || dot2 == std::string::npos) return "user";

    try {
        std::string payload_b64 = token.substr(dot1 + 1, dot2 - dot1 - 1);
        std::string payload_json = base64_decode(payload_b64);
        auto payload = nlohmann::json::parse(payload_json);
        return payload.value("role", "user");
    } catch (...) {
        return "user";
    }
}
