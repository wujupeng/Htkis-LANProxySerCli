#ifndef SOCKS5_H
#define SOCKS5_H

#include <string>
#include <vector>
#include <algorithm>

namespace lanproxy {
namespace socks5 {

    const uint8_t VER_SOCKS5 = 0x05;
    const uint8_t METHOD_NO_AUTH = 0x00;
    const uint8_t CMD_CONNECT = 0x01;
    const uint8_t ATYP_IPV4 = 0x01;
    const uint8_t ATYP_DOMAIN = 0x03;
    const uint8_t ATYP_IPV6 = 0x04;

    struct Request {
        uint8_t ver;
        uint8_t cmd;
        uint8_t rsv;
        uint8_t atyp;
        std::string domain; // If atyp == 0x03
        std::vector<uint8_t> ip; // If atyp == 0x01 or 0x04
        uint16_t port;
    };

    // Smart Routing Logic
    class Router {
    public:
        static bool is_domestic(const std::string& host) {
            // Simple heuristic for demo:
            // Check for common CN suffixes or keywords
            std::string h = host;
            std::transform(h.begin(), h.end(), h.begin(), ::tolower);

            if (ends_with(h, ".cn")) return true;
            if (ends_with(h, "baidu.com")) return true;
            if (ends_with(h, "qq.com")) return true;
            if (ends_with(h, "163.com")) return true;
            if (ends_with(h, "taobao.com")) return true;
            if (ends_with(h, "jd.com")) return true;
            if (ends_with(h, "aliyun.com")) return true;
            if (ends_with(h, "zhihu.com")) return true;
            if (ends_with(h, "bilibili.com")) return true;
            
            // Localhost
            if (h == "localhost" || h == "127.0.0.1") return true;

            return false;
        }

    private:
        static bool ends_with(const std::string& value, const std::string& ending) {
            if (ending.size() > value.size()) return false;
            return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
        }
    };

}
}

#endif // SOCKS5_H
