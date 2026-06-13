#include "user_manager/password_hasher.h"
#include <openssl/evp.h>
#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <array>
#include <cstdio>

static std::string bytes_to_hex(const std::string& bytes) {
    std::ostringstream oss;
    for (unsigned char c : bytes) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)c;
    }
    return oss.str();
}

static std::string generate_salt_bytes() {
    unsigned char salt[16];
    RAND_bytes(salt, sizeof(salt));
    return std::string(reinterpret_cast<char*>(salt), 16);
}

static std::string pbkdf2_hash(const std::string& password, const std::string& salt_hex, int iterations) {
    unsigned char derived[32];
    PKCS5_PBKDF2_HMAC(password.c_str(), (int)password.length(),
                       reinterpret_cast<const unsigned char*>(salt_hex.c_str()), (int)salt_hex.length(),
                       iterations, EVP_sha256(),
                       32, derived);
    
    std::ostringstream oss;
    for (int i = 0; i < 32; i++) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)derived[i];
    }
    return oss.str();
}

static std::string hex_encode(const std::string& s) {
    std::ostringstream oss;
    for (unsigned char c : s) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)c;
    }
    return oss.str();
}

static std::string hex_decode_str(const std::string& hex) {
    std::string result;
    for (size_t i = 0; i + 1 < hex.length(); i += 2) {
        int val;
        std::istringstream iss(hex.substr(i, 2));
        iss >> std::hex >> val;
        result.push_back((char)val);
    }
    return result;
}

static std::string python_bcrypt_hash(const std::string& password) {
    std::string pw_hex = hex_encode(password);
    std::string cmd = "python3 -c \"import bcrypt,binascii; "
        "pw=binascii.unhexlify('" + pw_hex + "').decode(); "
        "print(bcrypt.hashpw(pw.encode(), bcrypt.gensalt(10)).decode())\" 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    
    std::array<char, 128> buf;
    std::string result;
    while (fgets(buf.data(), buf.size(), pipe)) {
        result += buf.data();
    }
    pclose(pipe);
    
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
        result.pop_back();
    return result;
}

static bool python_bcrypt_verify(const std::string& password, const std::string& hash_str) {
    std::string pw_hex = hex_encode(password);
    std::string hash_hex = hex_encode(hash_str);
    std::string cmd = "python3 -c \"import bcrypt,binascii; "
        "pw=binascii.unhexlify('" + pw_hex + "').decode(); "
        "h=binascii.unhexlify('" + hash_hex + "').decode(); "
        "print('ok' if bcrypt.checkpw(pw.encode(), h.encode()) else 'fail')\" 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return false;
    
    std::array<char, 128> buf;
    std::string result;
    while (fgets(buf.data(), buf.size(), pipe)) {
        result += buf.data();
    }
    pclose(pipe);
    
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
        result.pop_back();
    return result == "ok";
}

std::string password_hasher::hash(const std::string& password) {
    std::string bcrypt_result = python_bcrypt_hash(password);
    if (!bcrypt_result.empty() && bcrypt_result.size() > 10 &&
        (bcrypt_result.substr(0, 3) == "$2a" || bcrypt_result.substr(0, 3) == "$2b")) {
        return bcrypt_result;
    }
    
    std::string salt = generate_salt_bytes();
    std::string salt_hex = bytes_to_hex(salt);
    std::string derived = pbkdf2_hash(password, salt_hex, 100000);
    return "$pbkdf2-sha256$100000$" + salt_hex + "$" + derived;
}

bool password_hasher::verify(const std::string& password, const std::string& hash_str) {
    if (hash_str.empty()) return false;
    
    if (hash_str.substr(0, 3) == "$2a" || hash_str.substr(0, 3) == "$2b") {
        return python_bcrypt_verify(password, hash_str);
    }
    
    if (hash_str.substr(0, 15) == "$pbkdf2-sha256$") {
        size_t d1 = hash_str.find('$', 1);
        if (d1 == std::string::npos) return false;
        size_t d2 = hash_str.find('$', d1 + 1);
        if (d2 == std::string::npos) return false;
        size_t d3 = hash_str.find('$', d2 + 1);
        if (d3 == std::string::npos) return false;
        
        std::string iter_str = hash_str.substr(d1 + 1, d2 - d1 - 1);
        std::string salt_hex = hash_str.substr(d2 + 1, d3 - d2 - 1);
        std::string stored_derived = hash_str.substr(d3 + 1);
        
        int iterations = std::stoi(iter_str);
        std::string computed = pbkdf2_hash(password, salt_hex, iterations);
        
        if (computed.size() != stored_derived.size()) return false;
        return CRYPTO_memcmp(computed.data(), stored_derived.data(), computed.size()) == 0;
    }
    
    return false;
}
