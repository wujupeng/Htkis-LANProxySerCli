#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>
#include <vector>
#include <string>

namespace lanproxy {

    enum MessageType : uint8_t {
        MSG_AUTH = 0x01,
        MSG_AUTH_RESP = 0x02,
        MSG_HEARTBEAT = 0x03,
        MSG_CONNECT = 0x04, // Server tells client to connect
        MSG_PROXY_HANDSHAKE = 0x05 // Client connects to server for data
    };

    #pragma pack(push, 1)
    struct Header {
        uint32_t length; // Body length
        uint8_t type;
    };
    #pragma pack(pop)

    // Helper to serialize/deserialize integers (Big Endian usually preferred for network)
    // For simplicity in this demo, we might rely on system endianness if same arch, 
    // but correct way is htonl/ntohl.
    
}

#endif // PROTOCOL_H
