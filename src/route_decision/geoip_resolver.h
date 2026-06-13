#pragma once
#include <string>
#include <maxminddb.h>

class geoip_resolver {
public:
    static geoip_resolver& instance();

    void init(const std::string& db_path);
    bool is_china(const std::string& ip);
    bool available() const { return m_available; }

private:
    geoip_resolver() = default;
    ~geoip_resolver() { if (m_available) MMDB_close(&m_mmdb); }

    MMDB_s m_mmdb{};
    bool m_available{false};
};
