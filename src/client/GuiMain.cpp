#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> 

#include "Client.h"
#include "Protocol.h"
#include "Localization.h"
#include <iostream>

// Helper to load fonts (Duplicate code, ideally should be in a common helper file)
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

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

int main(int, char**)
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    GLFWwindow* window = glfwCreateWindow(600, 400, Localization::getInstance().get("APP_TITLE_CLIENT").c_str(), NULL, NULL);
    if (window == NULL)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    LoadFonts();

    // State
    static char server_ip[128] = "127.0.0.1";
    static int server_port = LanProxy::DEFAULT_SERVER_PORT;
    static char username[128] = "testuser";
    static char password[128] = "123";
    static int local_port = LanProxy::DEFAULT_CLIENT_PORT;
    static bool show_about = false;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        glfwSetWindowTitle(window, Localization::getInstance().get("APP_TITLE_CLIENT").c_str());

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

        {
            ImGui::Begin(loc.get("CLIENT_CONNECTION").c_str());

            ImGui::InputText(loc.get("SERVER_IP").c_str(), server_ip, sizeof(server_ip));
            ImGui::InputInt(loc.get("SERVER_PORT").c_str(), &server_port);
            ImGui::InputText(loc.get("USERNAME").c_str(), username, sizeof(username));
            ImGui::InputText(loc.get("PASSWORD").c_str(), password, sizeof(password), ImGuiInputTextFlags_Password);
            ImGui::Separator();
            ImGui::InputInt(loc.get("LOCAL_PORT").c_str(), &local_port);
            
            ImGui::Separator();
            
            if (ClientApp::getInstance().isRunning()) {
                ImGui::TextColored(ImVec4(0,1,0,1), "%s", loc.get("STATUS_CONNECTED").c_str());
                ImGui::Text("%s: %d", loc.get("LOCAL_PORT").c_str(), local_port);
                ImGui::Text("Forwarding to: %s:%d", server_ip, server_port);
                
                if (ImGui::Button(loc.get("DISCONNECT").c_str())) {
                    ClientApp::getInstance().stop();
                }
            } else {
                ImGui::TextColored(ImVec4(1,0,0,1), "%s", loc.get("STATUS_DISCONNECTED").c_str());
                if (ImGui::Button(loc.get("CONNECT").c_str())) {
                    ClientConfig config;
                    config.server_ip = server_ip;
                    config.server_port = (uint16_t)server_port;
                    config.username = username;
                    config.password = password;
                    config.local_port = (uint16_t)local_port;
                    
                    ClientApp::getInstance().start(config);
                }
            }

            ImGui::Separator();
            ImGui::TextWrapped(loc.get("CLIENT_HINT").c_str(), local_port);

            ImGui::End();
        }

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ClientApp::getInstance().stop();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
