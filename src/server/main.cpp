#include "Server.h"
#include "UserManager.h"
#include "Protocol.h"
#include <iostream>
#include <thread>
#include <string>

int main(int argc, char* argv[]) {
    try {
        // Load users
        UserManager::getInstance().load("users.txt");

        // Start server in a separate thread
        std::thread server_thread([](){
            try {
                asio::io_context io_context;
                Server s(io_context, LanProxy::DEFAULT_SERVER_PORT);
                std::cout << "Server starting on port " << LanProxy::DEFAULT_SERVER_PORT << "..." << std::endl;
                io_context.run();
            } catch (std::exception& e) {
                std::cerr << "Server Exception: " << e.what() << "\n";
            }
        });

        // CLI for management
        std::cout << "LanProxy Server Management Console\n";
        std::cout << "Commands: add, del, list, active, deactive, save, help, exit\n";

        std::string command;
        while (true) {
            std::cout << "> ";
            if (!(std::cin >> command)) break;

            if (command == "exit") {
                // Ideally we should stop the server gracefully, but for now just exit
                // io_context.stop() needs to be accessible. 
                // Since we run in detached thread style for simplicity here, we just force exit.
                // In production, use signals or proper shutdown.
                UserManager::getInstance().save();
                std::cout << "Saving and exiting...\n";
                // Detach thread to allow OS cleanup or join if we had a stop mechanism
                server_thread.detach(); 
                break;
            } else if (command == "add") {
                std::string user, pass;
                int days;
                std::cout << "Username: "; std::cin >> user;
                std::cout << "Password: "; std::cin >> pass;
                std::cout << "Days valid: "; std::cin >> days;
                if (UserManager::getInstance().addUser(user, pass, days)) {
                    std::cout << "User added.\n";
                    UserManager::getInstance().save();
                } else {
                    std::cout << "User already exists.\n";
                }
            } else if (command == "del") {
                std::string user;
                std::cout << "Username: "; std::cin >> user;
                if (UserManager::getInstance().removeUser(user)) {
                    std::cout << "User removed.\n";
                    UserManager::getInstance().save();
                } else {
                    std::cout << "User not found.\n";
                }
            } else if (command == "list") {
                UserManager::getInstance().listUsers();
            } else if (command == "active") {
                std::string user;
                std::cout << "Username: "; std::cin >> user;
                if (UserManager::getInstance().setStatus(user, true)) {
                    std::cout << "User activated.\n";
                    UserManager::getInstance().save();
                } else {
                    std::cout << "User not found.\n";
                }
            } else if (command == "deactive") {
                std::string user;
                std::cout << "Username: "; std::cin >> user;
                if (UserManager::getInstance().setStatus(user, false)) {
                    std::cout << "User deactivated.\n";
                    UserManager::getInstance().save();
                } else {
                    std::cout << "User not found.\n";
                }
            } else if (command == "save") {
                UserManager::getInstance().save();
                std::cout << "Saved.\n";
            } else if (command == "help") {
                std::cout << "Commands: add, del, list, active, deactive, save, help, exit\n";
            } else {
                std::cout << "Unknown command.\n";
            }
        }

    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}
