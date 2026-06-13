#pragma once
#include <string>

class password_hasher {
public:
    static std::string hash(const std::string& password);
    static bool verify(const std::string& password, const std::string& hash_str);
};
