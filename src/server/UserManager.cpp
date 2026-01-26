#include "UserManager.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <vector>

UserManager& UserManager::getInstance() {
    static UserManager instance;
    return instance;
}

void UserManager::load(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_filepath = filepath;
    m_users.clear();

    std::ifstream file(filepath);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string user, pass;
        time_t expire;
        bool active;
        if (ss >> user >> pass >> expire >> active) {
            m_users[user] = {pass, expire, active};
        }
    }
}

void UserManager::save() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_filepath.empty()) return;

    std::ofstream file(m_filepath);
    for (const auto& [name, info] : m_users) {
        file << name << " " << info.password << " " << info.expire_time << " " << info.active << "\n";
    }
}

bool UserManager::addUser(const std::string& username, const std::string& password, int days) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_users.find(username) != m_users.end()) {
        return false;
    }
    
    time_t now = time(nullptr);
    time_t expire = now + days * 24 * 3600;
    
    m_users[username] = {password, expire, true};
    return true; // Changes are saved explicitly by calling save() or auto-save if implemented
}

bool UserManager::removeUser(const std::string& username) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_users.erase(username) > 0;
}

bool UserManager::authenticate(const std::string& username, const std::string& password) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_users.find(username);
    if (it == m_users.end()) return false;
    
    if (!it->second.active) return false;
    if (it->second.password != password) return false;
    
    time_t now = time(nullptr);
    if (now > it->second.expire_time) return false;

    return true;
}

void UserManager::listUsers() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::cout << std::left << std::setw(15) << "Username" 
              << std::setw(10) << "Active" 
              << std::setw(25) << "Expire Time" << "\n";
    std::cout << std::string(50, '-') << "\n";
    
    for (const auto& [name, info] : m_users) {
        std::string timeStr = std::ctime(&info.expire_time);
        if (!timeStr.empty()) timeStr.pop_back(); // remove newline
        
        std::cout << std::left << std::setw(15) << name 
                  << std::setw(10) << (info.active ? "Yes" : "No") 
                  << timeStr << "\n";
    }
}

bool UserManager::setStatus(const std::string& username, bool active) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_users.find(username);
    if (it == m_users.end()) return false;
    it->second.active = active;
    return true;
}

std::vector<std::pair<std::string, UserInfo>> UserManager::getAllUsers() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::pair<std::string, UserInfo>> users;
    users.reserve(m_users.size());
    for (const auto& kv : m_users) {
        users.push_back(kv);
    }
    return users;
}
