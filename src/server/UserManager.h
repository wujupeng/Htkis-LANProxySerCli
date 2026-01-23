#pragma once
#include <string>
#include <unordered_map>
#include <mutex>
#include <ctime>

struct UserInfo {
    std::string password;
    std::time_t expire_time; // 0 means no limit? Or just far future.
    bool active;
};

class UserManager {
public:
    static UserManager& getInstance();

    void load(const std::string& filepath);
    void save();

    bool addUser(const std::string& username, const std::string& password, int days);
    bool removeUser(const std::string& username);
    bool authenticate(const std::string& username, const std::string& password);
    void listUsers();
    bool setStatus(const std::string& username, bool active);
    
    // For GUI
    std::vector<std::pair<std::string, UserInfo> > getAllUsers();

private:
    UserManager() = default;
    ~UserManager() = default;
    
    std::string m_filepath;
    std::unordered_map<std::string, UserInfo> m_users;
    std::mutex m_mutex;
};
