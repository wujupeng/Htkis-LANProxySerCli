#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <nlohmann/json.hpp>
#include <cstring>
#include <vector>
#include <sstream>
#include <iostream>
#include <chrono>
#include <fstream>

static std::string m_secret;

static std::string base64_encode(const unsigned char* data, size_t len) {
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

static std::string base64url_encode(const unsigned char* data, size_t len) {
    static const char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
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

static std::string base64_decode(const std::string& input) {
    static const int table[256] = { []{
        int t[256]{};
        for (auto& v : t) v = -1;
        for (int i = 0; i < 26; i++) { t['A'+i] = i; t['a'+i] = i+26; }
        for (int i = 0; i < 10; i++) t['0'+i] = i+52;
        t['+'] = 62; t['-'] = 62; t['/'] = 63; t['_'] = 63;
        int arr[256]; std::memcpy(arr, t, sizeof(t)); return *arr;
    }() };
    std::vector<uint8_t> result;
    int val = 0, valb = -8;
    for (unsigned char c : input) {
        if (c == '=' || c == '-' || c == '_') continue;
        int idx = table[c];
        if (idx == -1) break;
        val = (val << 6) + idx;
        valb += 6;
        if (valb >= 0) {
            result.push_back(uint8_t((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return std::string(result.begin(), result.end());
}

static std::string hmac_sha256(const std::string& data, const std::string& key) {
    unsigned char result[EVP_MAX_MD_SIZE];
    unsigned int result_len = 0;
    HMAC(EVP_sha256(), key.c_str(), (int)key.length(),
         reinterpret_cast<const unsigned char*>(data.c_str()), data.length(),
         result, &result_len);
    return std::string(reinterpret_cast<char*>(result), result_len);
}

static std::string sign_standard(const std::string& username, int expire_minutes) {
    nlohmann::json header;
    header["alg"] = "HS256";
    header["typ"] = "JWT";
    nlohmann::json payload;
    payload["username"] = username;
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

static std::string sign_base64url(const std::string& username, int expire_minutes) {
    nlohmann::json header;
    header["alg"] = "HS256";
    header["typ"] = "JWT";
    nlohmann::json payload;
    payload["username"] = username;
    payload["exp"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() + expire_minutes * 60;
    std::string header_b64 = base64url_encode(
        reinterpret_cast<const unsigned char*>(header.dump().c_str()),
        header.dump().length());
    std::string payload_b64 = base64url_encode(
        reinterpret_cast<const unsigned char*>(payload.dump().c_str()),
        payload.dump().length());
    std::string signing_input = header_b64 + "." + payload_b64;
    std::string sig = hmac_sha256(signing_input, m_secret);
    std::string sig_b64 = base64url_encode(
        reinterpret_cast<const unsigned char*>(sig.c_str()), sig.length());
    return signing_input + "." + sig_b64;
}

static bool verify_token(const std::string& token) {
    auto dot1 = token.find('.');
    auto dot2 = token.find('.', dot1 + 1);
    if (dot1 == std::string::npos || dot2 == std::string::npos) return false;
    std::string header_b64 = token.substr(0, dot1);
    std::string payload_b64 = token.substr(dot1 + 1, dot2 - dot1 - 1);
    std::string sig_b64 = token.substr(dot2 + 1);
    std::string signing_input = header_b64 + "." + payload_b64;
    std::string expected_sig = hmac_sha256(signing_input, m_secret);
    std::string expected_sig_b64 = base64_encode(
        reinterpret_cast<const unsigned char*>(expected_sig.c_str()),
        expected_sig.length());
    std::cout << "  Token sig_b64:      " << sig_b64 << std::endl;
    std::cout << "  Expected sig_b64:   " << expected_sig_b64 << std::endl;
    std::cout << "  Match: " << (sig_b64 == expected_sig_b64 ? "YES" : "NO") << std::endl;
    return sig_b64 == expected_sig_b64;
}

static bool verify_token_base64url(const std::string& token) {
    auto dot1 = token.find('.');
    auto dot2 = token.find('.', dot1 + 1);
    if (dot1 == std::string::npos || dot2 == std::string::npos) return false;
    std::string header_b64 = token.substr(0, dot1);
    std::string payload_b64 = token.substr(dot1 + 1, dot2 - dot1 - 1);
    std::string sig_b64 = token.substr(dot2 + 1);
    std::string signing_input = header_b64 + "." + payload_b64;
    std::string expected_sig = hmac_sha256(signing_input, m_secret);
    std::string expected_sig_b64 = base64url_encode(
        reinterpret_cast<const unsigned char*>(expected_sig.c_str()),
        expected_sig.length());
    std::cout << "  Token sig_b64:      " << sig_b64 << std::endl;
    std::cout << "  Expected sig_b64:   " << expected_sig_b64 << std::endl;
    std::cout << "  Match: " << (sig_b64 == expected_sig_b64 ? "YES" : "NO") << std::endl;
    return sig_b64 == expected_sig_b64;
}

int main() {
    m_secret = "a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2";
    std::cout << "=== JWT Standalone Test ===" << std::endl;
    std::cout << "Secret: " << m_secret << " (len=" << m_secret.length() << ")" << std::endl;

    std::cout << "\n--- Test 1: Sign+Verify with standard base64 (current code) ---" << std::endl;
    std::string token1 = sign_standard("admin", 30);
    std::cout << "Token: " << token1 << std::endl;
    bool v1 = verify_token(token1);
    std::cout << "Verify result: " << (v1 ? "PASS" : "FAIL") << std::endl;

    std::cout << "\n--- Test 2: Sign+Verify with base64url (JWT standard) ---" << std::endl;
    std::string token2 = sign_base64url("admin", 30);
    std::cout << "Token: " << token2 << std::endl;
    bool v2 = verify_token_base64url(token2);
    std::cout << "Verify result: " << (v2 ? "PASS" : "FAIL") << std::endl;

    std::cout << "\n--- Test 3: Cross-check - verify standard token with base64url verify ---" << std::endl;
    bool v3 = verify_token_base64url(token1);
    std::cout << "Verify result: " << (v3 ? "PASS" : "FAIL") << std::endl;

    std::cout << "\n--- Test 4: Cross-check - verify base64url token with standard verify ---" << std::endl;
    bool v4 = verify_token(token2);
    std::cout << "Verify result: " << (v4 ? "PASS" : "FAIL") << std::endl;

    std::cout << "\n--- Test 5: Decode payload from standard token ---" << std::endl;
    {
        auto dot1 = token1.find('.');
        auto dot2 = token1.find('.', dot1 + 1);
        std::string payload_b64 = token1.substr(dot1 + 1, dot2 - dot1 - 1);
        std::string payload_json = base64_decode(payload_b64);
        std::cout << "Payload JSON: " << payload_json << std::endl;
        try {
            auto payload = nlohmann::json::parse(payload_json);
            std::cout << "username: " << payload.value("username", "") << std::endl;
            std::cout << "exp: " << payload.value("exp", (int64_t)0) << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Parse error: " << e.what() << std::endl;
        }
    }

    std::cout << "\n--- Test 6: Verify token contains '+' or '/' (non-base64url chars) ---" << std::endl;
    bool has_plus = token1.find('+') != std::string::npos;
    bool has_slash_char = token1.find('/') != std::string::npos;
    std::cout << "Contains '+': " << (has_plus ? "YES" : "NO") << std::endl;
    std::cout << "Contains '/': " << (has_slash_char ? "YES" : "NO") << std::endl;

    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Standard base64 sign+verify: " << (v1 ? "PASS" : "FAIL") << std::endl;
    std::cout << "Base64url sign+verify: " << (v2 ? "PASS" : "FAIL") << std::endl;

    return 0;
}
