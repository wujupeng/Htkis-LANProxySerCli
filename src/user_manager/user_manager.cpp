#include "user_manager/user_manager.h"
#include "user_manager/password_hasher.h"
#include "config_manager/config_loader.h"
#include "log_monitor/structured_logger.h"
#include <fstream>
#include <algorithm>

user_manager& user_manager::instance() {
    static user_manager inst;
    return inst;
}

bool user_manager::load(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_filepath = filepath;
    m_users.clear();

    std::ifstream file(filepath);
    if (!file.is_open()) return false;

    try {
        nlohmann::json j;
        file >> j;

        if (j.contains("users") && j["users"].is_array()) {
            for (const auto& u : j["users"]) {
                user_info info;
                info.username = u.value("username", "");
                info.password_hash = u.value("password_hash", "");
                info.expire_time = u.value("expire_time", (std::time_t)0);
                info.active = u.value("active", true);
                if (!info.username.empty()) {
                    m_users[info.username] = info;
                }
            }
        }
        return true;
    } catch (const nlohmann::json::exception& e) {
        structured_logger::instance().error("user_manager",
            "Load users error: " + std::string(e.what()));
        return false;
    }
}

bool user_manager::save() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_filepath.empty()) return false;

    nlohmann::json j;
    j["version"] = 1;
    j["users"] = nlohmann::json::array();

    for (const auto& [name, info] : m_users) {
        nlohmann::json u;
        u["username"] = info.username;
        u["password_hash"] = info.password_hash;
        u["expire_time"] = info.expire_time;
        u["active"] = info.active;
        j["users"].push_back(u);
    }

    std::string tmp = m_filepath + ".tmp";
    {
        std::ofstream file(tmp);
        if (!file.is_open()) return false;
        file << j.dump(4);
    }
    return std::rename(tmp.c_str(), m_filepath.c_str()) == 0;
}

bool user_manager::add_user(const std::string& username,
                              const std::string& password, int days) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_users.find(username) != m_users.end()) return false;

    user_info info;
    info.username = username;
    info.password_hash = password_hasher::hash(password);
    if (days > 0) {
        info.expire_time = std::time(nullptr) + days * 24 * 3600;
    } else {
        info.expire_time = 0;
    }
    info.active = true;

    m_users[username] = info;
    structured_logger::instance().info("user_manager", "User added: " + username);
    return true;
}

bool user_manager::remove_user(const std::string& username) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_users.erase(username) > 0) {
        structured_logger::instance().info("user_manager", "User removed: " + username);
        return true;
    }
    return false;
}

bool user_manager::authenticate(const std::string& username,
                                  const std::string& password) {
    auto& cfg = config_loader::instance().config();
    if (username == cfg.admin_username) {
        return password_hasher::verify(password, cfg.admin_password_hash);
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_users.find(username);
    if (it == m_users.end()) return false;

    if (!it->second.active) return false;

    if (it->second.expire_time > 0 &&
        std::time(nullptr) > it->second.expire_time) {
        return false;
    }

    return password_hasher::verify(password, it->second.password_hash);
}

bool user_manager::set_status(const std::string& username, bool active) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_users.find(username);
    if (it == m_users.end()) return false;
    it->second.active = active;
    return true;
}

bool user_manager::update_password(const std::string& username,
                                     const std::string& new_password) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_users.find(username);
    if (it == m_users.end()) return false;
    it->second.password_hash = password_hasher::hash(new_password);
    return true;
}

std::vector<user_info> user_manager::get_all_users() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<user_info> result;
    result.reserve(m_users.size());
    for (const auto& [name, info] : m_users) {
        result.push_back(info);
    }
    return result;
}

std::vector<user_info> user_manager::list_users_paginated(int page,
                                                            int page_size) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<user_info> all;
    all.reserve(m_users.size());
    for (const auto& [name, info] : m_users) {
        all.push_back(info);
    }

    int start = (page - 1) * page_size;
    if (start >= (int)all.size()) return {};

    int end = std::min(start + page_size, (int)all.size());
    return std::vector<user_info>(all.begin() + start, all.begin() + end);
}

user_info user_manager::get_user_info(const std::string& username) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_users.find(username);
    if (it == m_users.end()) return {};
    return it->second;
}
