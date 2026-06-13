#include <iostream>
#include <cstring>
#include <vector>
#include <string>
#include <nlohmann/json.hpp>

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

int main() {
    std::string token = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJleHAiOjE3ODEzMzg3OTAsInVzZXJuYW1lIjoiYWRtaW4ifQ.Et6bOa9HhyjUgA/0sojl3Ce2LsH9OBOuY0jtqgQCpDk";
    
    auto dot1 = token.find('.');
    auto dot2 = token.find('.', dot1 + 1);
    
    std::string header_b64 = token.substr(0, dot1);
    std::string payload_b64 = token.substr(dot1 + 1, dot2 - dot1 - 1);
    
    std::cout << "header_b64:  " << header_b64 << std::endl;
    std::cout << "payload_b64: " << payload_b64 << std::endl;
    
    std::string header_decoded = base64_decode(header_b64);
    std::string payload_decoded = base64_decode(payload_b64);
    
    std::cout << "header_decoded:  [" << header_decoded << "]" << std::endl;
    std::cout << "header_decoded len: " << header_decoded.length() << std::endl;
    std::cout << "payload_decoded: [" << payload_decoded << "]" << std::endl;
    std::cout << "payload_decoded len: " << payload_decoded.length() << std::endl;
    
    std::cout << "\n--- Hex dump of header_decoded ---" << std::endl;
    for (size_t i = 0; i < header_decoded.length(); i++) {
        printf("%02x ", (unsigned char)header_decoded[i]);
        if ((i+1) % 16 == 0) printf("\n");
    }
    printf("\n");
    
    std::cout << "\n--- Hex dump of payload_decoded ---" << std::endl;
    for (size_t i = 0; i < payload_decoded.length(); i++) {
        printf("%02x ", (unsigned char)payload_decoded[i]);
        if ((i+1) % 16 == 0) printf("\n");
    }
    printf("\n");

    std::cout << "\n--- Try parse as JSON ---" << std::endl;
    try {
        auto hdr = nlohmann::json::parse(header_decoded);
        std::cout << "Header JSON OK: " << hdr.dump() << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Header parse FAILED: " << e.what() << std::endl;
    }
    
    try {
        auto pl = nlohmann::json::parse(payload_decoded);
        std::cout << "Payload JSON OK: " << pl.dump() << std::endl;
        std::cout << "username: " << pl.value("username", "") << std::endl;
        std::cout << "exp: " << pl.value("exp", (int64_t)0) << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Payload parse FAILED: " << e.what() << std::endl;
    }

    std::cout << "\n--- Test with explicit known value ---" << std::endl;
    std::string known = "eyJleHAiOjE3ODEzMzg3OTAsInVzZXJuYW1lIjoiYWRtaW4ifQ";
    std::string known_decoded = base64_decode(known);
    std::cout << "known_decoded: [" << known_decoded << "]" << std::endl;
    try {
        auto j = nlohmann::json::parse(known_decoded);
        std::cout << "known JSON OK: " << j.dump() << std::endl;
    } catch (const std::exception& e) {
        std::cout << "known parse FAILED: " << e.what() << std::endl;
    }
    
    return 0;
}
