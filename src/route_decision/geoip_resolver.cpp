#include "route_decision/geoip_resolver.h"
#include "log_monitor/structured_logger.h"

geoip_resolver& geoip_resolver::instance() {
    static geoip_resolver inst;
    return inst;
}

void geoip_resolver::init(const std::string& db_path) {
    int status = MMDB_open(db_path.c_str(), MMDB_MODE_MMAP, &m_mmdb);
    if (status != MMDB_SUCCESS) {
        m_available = false;
        structured_logger::instance().warn("geoip_resolver",
            "GeoIP DB load failed: " + std::string(MMDB_strerror(status)) +
            " path=" + db_path + ". All traffic will use default route.");
    } else {
        m_available = true;
        structured_logger::instance().info("geoip_resolver", "GeoIP DB loaded: " + db_path);
    }
}

bool geoip_resolver::is_china(const std::string& ip) {
    if (!m_available) return false;

    int gai_error = 0, mmdb_error = 0;
    MMDB_lookup_result_s result = MMDB_lookup_string(&m_mmdb, ip.c_str(),
                                                       &gai_error, &mmdb_error);

    if (gai_error != 0 || mmdb_error != MMDB_SUCCESS || !result.found_entry) {
        return false;
    }

    MMDB_entry_data_s entry_data;
    int status = MMDB_get_value(&result.entry, &entry_data,
                                 "country", "iso_code", NULL);
    if (status != MMDB_SUCCESS || !entry_data.has_data ||
        entry_data.type != MMDB_DATA_TYPE_UTF8_STRING) {
        return false;
    }

    std::string iso_code(entry_data.utf8_string,
                          entry_data.utf8_string + entry_data.data_size);
    return iso_code == "CN";
}
