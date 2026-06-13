#include "v2rayn_manager/vm_link_codec.h"
#include <openssl/evp.h>
#include <regex>

std::string vm_link_codec::base64_encode(const std::string& input) {
    int encoded_len = 4 * ((input.size() + 2) / 3);
    std::string output(encoded_len, '\0');
    EVP_EncodeBlock(reinterpret_cast<unsigned char*>(&output[0]),
                    reinterpret_cast<const unsigned char*>(input.data()),
                    static_cast<int>(input.size()));
    while (!output.empty() && output.back() == '\0') output.pop_back();
    while (!output.empty() && output.back() == '=') output.pop_back();
    return output;
}

std::string vm_link_codec::base64_decode(const std::string& input) {
    std::string padded = input;
    while (padded.size() % 4 != 0) padded += '=';

    int decoded_len = 3 * padded.size() / 4;
    std::string output(decoded_len, '\0');
    int actual = EVP_DecodeBlock(reinterpret_cast<unsigned char*>(&output[0]),
                                  reinterpret_cast<const unsigned char*>(padded.data()),
                                  static_cast<int>(padded.size()));
    if (actual < 0) return "";
    output.resize(actual);
    while (!output.empty() && output.back() == '\0') output.pop_back();
    return output;
}

std::string vm_link_codec::base64_urlsafe_decode(const std::string& input) {
    std::string converted = input;
    for (auto& c : converted) {
        if (c == '-') c = '+';
        else if (c == '_') c = '/';
    }
    return base64_decode(converted);
}

std::optional<vm_node> vm_link_codec::decode(const std::string& vmess_link, std::string& error_msg) {
    const std::string prefix = "vmess://";
    if (vmess_link.size() < prefix.size() ||
        vmess_link.substr(0, prefix.size()) != prefix) {
        error_msg = "无效的vmess链接：缺少vmess://前缀";
        return std::nullopt;
    }

    std::string b64_part = vmess_link.substr(prefix.size());
    std::string json_str = base64_urlsafe_decode(b64_part);
    if (json_str.empty()) {
        json_str = base64_decode(b64_part);
    }
    if (json_str.empty()) {
        error_msg = "vmess链接Base64解码失败";
        return std::nullopt;
    }

    nlohmann::json j;
    try {
        j = nlohmann::json::parse(json_str);
    } catch (const nlohmann::json::exception& e) {
        error_msg = std::string("vmess链接JSON解析失败：") + e.what();
        return std::nullopt;
    }

    vm_node node;
    node.remark = j.value("ps", "");
    node.address = j.value("add", "");
    node.user_id = j.value("id", "");
    node.security = j.value("scy", "auto");
    node.network = j.value("net", "tcp");
    node.tls = j.value("tls", "");
    if (node.tls.empty() || node.tls == "none") node.tls = "none";
    else if (node.tls == "tls") node.tls = "tls";

    try {
        if (j.contains("port")) {
            if (j["port"].is_number()) node.port = j["port"].get<uint16_t>();
            else if (j["port"].is_string()) node.port = static_cast<uint16_t>(std::stoi(j["port"].get<std::string>()));
        }
    } catch (...) { node.port = 443; }

    try {
        if (j.contains("aid")) {
            if (j["aid"].is_number()) node.alter_id = j["aid"].get<int>();
            else if (j["aid"].is_string()) node.alter_id = std::stoi(j["aid"].get<std::string>());
        }
    } catch (...) { node.alter_id = 0; }

    std::string header_type = j.value("type", "none");
    std::string host = j.value("host", "");
    std::string path = j.value("path", "");
    std::string sni = j.value("sni", "");

    if (node.network == "tcp") {
        node.tcp_cfg.header_type = header_type;
    } else if (node.network == "ws") {
        node.ws_cfg.path = path.empty() ? "/" : path;
        node.ws_cfg.host = host;
    } else if (node.network == "h2") {
        node.h2_cfg.path = path.empty() ? "/" : path;
        if (!host.empty()) {
            std::string h = host;
            std::string delim = ",";
            size_t start = 0, end = h.find(delim);
            while (end != std::string::npos) {
                node.h2_cfg.host.push_back(h.substr(start, end - start));
                start = end + delim.length();
                end = h.find(delim, start);
            }
            node.h2_cfg.host.push_back(h.substr(start));
        }
    } else if (node.network == "quic") {
        node.quic_cfg.security = host.empty() ? "none" : host;
        node.quic_cfg.key = path;
        node.quic_cfg.header_type = header_type;
    } else if (node.network == "kcp") {
        node.kcp_cfg.header_type = header_type;
    }

    if (node.tls == "tls" && !sni.empty()) {
        node.tls_cfg.server_name = sni;
    }

    return node;
}

std::string vm_link_codec::encode(const vm_node& node) {
    nlohmann::json j;
    j["v"] = "2";
    j["ps"] = node.remark;
    j["add"] = node.address;
    j["port"] = std::to_string(node.port);
    j["id"] = node.user_id;
    j["aid"] = std::to_string(node.alter_id);
    j["scy"] = node.security;
    j["net"] = node.network;
    j["tls"] = (node.tls == "tls") ? "tls" : "";

    if (node.network == "tcp") {
        j["type"] = node.tcp_cfg.header_type;
        j["host"] = "";
        j["path"] = "";
    } else if (node.network == "ws") {
        j["type"] = "none";
        j["host"] = node.ws_cfg.host;
        j["path"] = node.ws_cfg.path;
    } else if (node.network == "h2") {
        j["type"] = "none";
        if (!node.h2_cfg.host.empty()) {
            std::string hosts;
            for (size_t i = 0; i < node.h2_cfg.host.size(); ++i) {
                if (i > 0) hosts += ",";
                hosts += node.h2_cfg.host[i];
            }
            j["host"] = hosts;
        } else {
            j["host"] = "";
        }
        j["path"] = node.h2_cfg.path;
    } else if (node.network == "quic") {
        j["type"] = node.quic_cfg.header_type;
        j["host"] = node.quic_cfg.security;
        j["path"] = node.quic_cfg.key;
    } else if (node.network == "kcp") {
        j["type"] = node.kcp_cfg.header_type;
        j["host"] = "";
        j["path"] = "";
    }

    j["sni"] = (node.tls == "tls") ? node.tls_cfg.server_name : "";

    std::string json_str = j.dump();
    return "vmess://" + base64_encode(json_str);
}
