#ifndef UI_H
#define UI_H

#include <iostream>
#include <string>
#include <mutex>
#include <chrono>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#endif

namespace lanproxy {
namespace ui {

    // ANSI Colors
    const std::string RESET = "\033[0m";
    const std::string RED = "\033[31m";
    const std::string GREEN = "\033[32m";
    const std::string YELLOW = "\033[33m";
    const std::string BLUE = "\033[34m";
    const std::string MAGENTA = "\033[35m";
    const std::string CYAN = "\033[36m";
    const std::string WHITE = "\033[37m";
    const std::string BOLD = "\033[1m";

    static std::mutex log_mutex;

    inline void enable_ansi() {
#ifdef _WIN32
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD dwMode = 0;
        GetConsoleMode(hOut, &dwMode);
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, dwMode);
#endif
    }

    inline void print_banner(const std::string& role) {
        std::cout << CYAN << BOLD;
        std::cout << R"(
    __  __      __   _          __    ___    _   __    ____  ____  ____  _  __  __  __
   / / / /_  __/ /__(_)____    / /   /   |  / | / /   / __ \/ __ \/ __ \| |/ /  \ \/ /
  / /_/ / / / / __/ / / ___/   / /   / /| | /  |/ /   / /_/ / /_/ / / / /   /    \  / 
 / __  / /_/ / /_/ / (__  )   / /___/ ___ |/ /|  /   / ____/ _, _/ /_/ /   |     / /  
/_/ /_/\__,_/\__/_/_/____/   /_____/_/  |_/_/ |_/   /_/   /_/ |_|\____/_/|_|    /_/   
                                                                                      
)" << RESET << std::endl;
        std::cout << "    " << GREEN << ":: SYSTEM :: " << role << " NODE ::" << RESET << "\n" << std::endl;
        std::cout << "    " << WHITE << "--------------------------------------------------------" << RESET << std::endl;
    }

    inline void log(const std::string& level, const std::string& msg, const std::string& color = WHITE) {
        std::lock_guard<std::mutex> lock(log_mutex);
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        
        std::cout << " " << color << "[" << level << "]" << RESET << " " 
                  << std::put_time(std::localtime(&time), "%H:%M:%S") << " | " 
                  << msg << std::endl;
    }

    inline void info(const std::string& msg) {
        log("INFO", msg, CYAN);
    }

    inline void success(const std::string& msg) {
        log("OK  ", msg, GREEN);
    }

    inline void warn(const std::string& msg) {
        log("WARN", msg, YELLOW);
    }

    inline void error(const std::string& msg) {
        log("ERR ", msg, RED);
    }

    inline void traffic(const std::string& type, const std::string& target, bool tunneled) {
        std::string icon = tunneled ? ">>>" : "---";
        std::string mode = tunneled ? (MAGENTA + "TUNNEL") : (BLUE + "DIRECT");
        std::string msg = icon + " " + type + " " + target + " [" + mode + RESET + "]";
        log("TRFC", msg, WHITE);
    }

}
}

#endif // UI_H
