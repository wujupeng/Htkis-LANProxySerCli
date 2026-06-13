#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <ctime>
#include <nlohmann/json.hpp>

struct user_info {
    std::string username;
    std::string password_hash;
    std::time_t expire_time{0};
    bool active{true};
};

class user_manager {
public:
    static user_manager& instance();

    bool load(const std::string& filepath);
    bool save();

    bool add_user(const std::string& username, const std::string& password, int days);
    bool remove_user(const std::string& username);
    bool authenticate(const std::string& username, const std::string& password);
    bool set_status(const std::string& username, bool active);
    bool update_password(const std::string& username, const std::string& new_password);

    std::vector<user_info> get_all_users() const;
    std::vector<user_info> list_users_paginated(int page, int page_size) const;
    user_info get_user_info(const std::string& username) const;

private:
    user_manager() = default;

    std::string m_filepath;
    std::unordered_map<std::string, user_info> m_users;
    mutable std::mutex m_mutex;
};
