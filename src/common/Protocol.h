#pragma once
#include <cstdint>
#include <string>

namespace LanProxy {

    constexpr int DEFAULT_SERVER_PORT = 10800;
    constexpr int DEFAULT_CLIENT_PORT = 10801;
    
    // SOCKS5 constants
    constexpr uint8_t SOCKS_VERSION = 0x05;
    
    // Auth methods
    constexpr uint8_t AUTH_NONE = 0x00;
    constexpr uint8_t AUTH_USERPASS = 0x02;
    constexpr uint8_t AUTH_NO_ACCEPTABLE = 0xFF;

    // Commands
    constexpr uint8_t CMD_CONNECT = 0x01;
    
    // Address types
    constexpr uint8_t ATYP_IPV4 = 0x01;
    constexpr uint8_t ATYP_DOMAIN = 0x03;
    constexpr uint8_t ATYP_IPV6 = 0x04;

}
