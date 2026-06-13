#pragma once
#include <string>
#include <optional>
#include "v2rayn_manager/vm_node_manager.h"

class vm_link_codec {
public:
    static std::optional<vm_node> decode(const std::string& vmess_link, std::string& error_msg);
    static std::string encode(const vm_node& node);

private:
    static std::string base64_encode(const std::string& input);
    static std::string base64_decode(const std::string& input);
    static std::string base64_urlsafe_decode(const std::string& input);
};
