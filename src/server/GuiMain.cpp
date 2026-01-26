#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#include "Server.h"
#include "UserManager.h"
#include "Protocol.h"
#include "Localization.h"
#include <iostream>
#include <vector>

// Helper to load fonts
void LoadFonts() {
    ImGuiIO& io = ImGui::GetIO();
    
    // Build glyph ranges to support all languages
    static ImVector<ImWchar> ranges;
    ImFontGlyphRangesBuilder builder;
    builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
    builder.AddRanges(io.Fonts->GetGlyphRangesChineseFull());
    builder.AddRanges(io.Fonts->GetGlyphRangesThai());
    builder.AddRanges(io.Fonts->GetGlyphRangesVietnamese());
    builder.BuildRanges(&ranges);

    // Font paths
    const char* font_arial_uni = "/System/Library/Fonts/Supplemental/Arial Unicode.ttf";
    const char* font_pingfang = "/System/Library/Fonts/PingFang.ttc";
    const char* font_thonburi = "/System/Library/Fonts/Supplemental/Thonburi.ttc";
    
    bool loaded = false;
    
    // 1. Try Arial Unicode first (Best coverage for all requested languages)
    FILE* f = fopen(font_arial_uni, "rb");
    if (f) {
        fclose(f);
        io.Fonts->AddFontFromFileTTF(font_arial_uni, 18.0f, NULL, ranges.Data);
        loaded = true;
    }
    
    // 2. Fallback: PingFang (Chinese) + Thonburi (Thai)
    if (!loaded) {
        f = fopen(font_pingfang, "rb");
        if (f) {
            fclose(f);
            io.Fonts->AddFontFromFileTTF(font_pingfang, 18.0f, NULL, ranges.Data);
            loaded = true;
            
            // Merge Thonburi for Thai
            f = fopen(font_thonburi, "rb");
            if (f) {
                fclose(f);
                ImFontConfig config;
                config.MergeMode = true;
                io.Fonts->AddFontFromFileTTF(font_thonburi, 18.0f, &config, io.Fonts->GetGlyphRangesThai());
            }
        }
    }
    
    if (!loaded) {
        io.Fonts->AddFontDefault();
    }
}

// Helper to display user list
void ShowUserList() {
    auto& loc = Localization::getInstance();
    static char username[128] = "";
    static char password[128] = "";
    static int days = 30;

    ImGui::Separator();
    ImGui::Text("%s", loc.get("USER_MANAGEMENT").c_str());

    // Add User Form
    ImGui::InputText(loc.get("USERNAME").c_str(), username, IM_ARRAYSIZE(username));
    ImGui::InputText(loc.get("PASSWORD").c_str(), password, IM_ARRAYSIZE(password), ImGuiInputTextFlags_Password);
    ImGui::InputInt(loc.get("DAYS").c_str(), &days);
    
    if (ImGui::Button(loc.get("ADD_USER").c_str())) {
        if (strlen(username) > 0 && strlen(password) > 0) {
            UserManager::getInstance().addUser(username, password, days);
            UserManager::getInstance().save();
            memset(username, 0, sizeof(username));
            memset(password, 0, sizeof(password));
        }
    }

    ImGui::Separator();
    
    static std::vector<std::pair<std::string, UserInfo>> users;
    if (ImGui::Button(loc.get("REFRESH_LIST").c_str())) {
        users = UserManager::getInstance().getAllUsers();
    }
    
    if (ImGui::BeginTable("Users", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn(loc.get("USERNAME").c_str());
        ImGui::TableSetupColumn(loc.get("ACTIVE").c_str());
        ImGui::TableSetupColumn(loc.get("EXPIRE_TIME").c_str());
        ImGui::TableSetupColumn(loc.get("ACTIONS").c_str());
        ImGui::TableHeadersRow();

        for (const auto& user : users) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", user.first.c_str());
            
            ImGui::TableSetColumnIndex(1);
            if (user.second.active) {
                ImGui::TextColored(ImVec4(0,1,0,1), "%s", loc.get("YES").c_str());
            } else {
                ImGui::TextColored(ImVec4(1,0,0,1), "%s", loc.get("NO").c_str());
            }
            
            ImGui::TableSetColumnIndex(2);
            std::string timeStr = std::ctime(&user.second.expire_time);
            if (!timeStr.empty()) timeStr.pop_back();
            ImGui::Text("%s", timeStr.c_str());

            ImGui::TableSetColumnIndex(3);
            std::string btnLabel = std::string(user.second.active ? loc.get("DEACTIVATE") : loc.get("ACTIVATE")) + "##" + user.first;
            if (ImGui::Button(btnLabel.c_str())) {
                UserManager::getInstance().setStatus(user.first, !user.second.active);
                UserManager::getInstance().save();
                users = UserManager::getInstance().getAllUsers(); // Refresh
            }
            ImGui::SameLine();
            std::string delLabel = loc.get("DELETE") + "##" + user.first;
            if (ImGui::Button(delLabel.c_str())) {
                UserManager::getInstance().removeUser(user.first);
                UserManager::getInstance().save();
                users = UserManager::getInstance().getAllUsers(); // Refresh
            }
        }
        ImGui::EndTable();
    }
}

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

int main(int, char**)
{
    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac

    // Create window with graphics context
    GLFWwindow* window = glfwCreateWindow(900, 700, Localization::getInstance().get("APP_TITLE_SERVER").c_str(), NULL, NULL);
    if (window == NULL)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    
    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Load Fonts
    LoadFonts();

    // Load users
    UserManager::getInstance().load("users.txt");

    // State
    static int port = LanProxy::DEFAULT_SERVER_PORT;
    static bool show_about = false;
    
    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // Update title if language changed
        glfwSetWindowTitle(window, Localization::getInstance().get("APP_TITLE_SERVER").c_str());

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        auto& loc = Localization::getInstance();

        // Main Menu Bar
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu(loc.get("LANGUAGE").c_str())) {
                for (const auto& lang : loc.getAvailableLanguages()) {
                    if (ImGui::MenuItem(lang.name.c_str(), NULL, loc.getLanguage() == lang.code)) {
                        loc.setLanguage(lang.code);
                        // Reload fonts might be needed for some languages if we optimize font loading
                        // For now we loaded full Chinese range which covers most.
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem(loc.get("ABOUT").c_str())) {
                show_about = true;
            }
            ImGui::EndMainMenuBar();
        }

        // About Window
        if (show_about) {
            ImGui::Begin(loc.get("ABOUT").c_str(), &show_about);
            ImGui::Text("Htkis-LANProxySerCli");
            ImGui::Separator();
            ImGui::Text("%s: Hunt", loc.get("DEVELOPER").c_str());
            ImGui::Text("%s: admin@cii.sh.cn", loc.get("EMAIL").c_str());
            ImGui::End();
        }

        // Window logic
        {
            ImGui::Begin(loc.get("SERVER_CONTROL_PANEL").c_str());

            ImGui::Text("Status: ");
            ImGui::SameLine();
            if (ServerApp::getInstance().isRunning()) {
                ImGui::TextColored(ImVec4(0,1,0,1), "%s", loc.get("STATUS_RUNNING").c_str());
                ImGui::Text("%s: %d", loc.get("SERVER_PORT").c_str(), ServerApp::getInstance().getPort());
                if (ImGui::Button(loc.get("STOP_SERVER").c_str())) {
                    ServerApp::getInstance().stop();
                }
            } else {
                ImGui::TextColored(ImVec4(1,0,0,1), "%s", loc.get("STATUS_STOPPED").c_str());
                ImGui::InputInt(loc.get("SERVER_PORT").c_str(), &port);
                if (ImGui::Button(loc.get("START_SERVER").c_str())) {
                    ServerApp::getInstance().start(port);
                }
            }
            
            ShowUserList();

            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    ServerApp::getInstance().stop();
    UserManager::getInstance().save();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
