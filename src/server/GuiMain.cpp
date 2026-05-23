#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <objidl.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
#include "resource.h"
#endif

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>

#include "Server.h"
#include "UserManager.h"
#include "Protocol.h"
#include "Localization.h"
#include "Logger.h"
#include <iostream>
#include <vector>
#include <ctime>

// ===== Global State =====
static GLFWwindow* g_window = nullptr;
static bool g_shouldExit = false;

#ifdef _WIN32
// ===== System Tray =====
#define WM_TRAYICON (WM_USER + 1)
static NOTIFYICONDATAW g_nid = {};
static HWND g_hwndTray = nullptr;
static HICON g_hIcon = nullptr;
static ULONG_PTR g_gdiplusToken = 0;

static std::wstring UTF8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
    std::wstring result(size - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], size);
    return result;
}

static std::wstring GetExeDir() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::wstring dir(exePath);
    size_t pos = dir.find_last_of(L"\\/");
    if (pos != std::wstring::npos) dir = dir.substr(0, pos);
    return dir;
}

// Load Gdiplus::Bitmap from embedded resource, fallback to file
static Gdiplus::Bitmap* LoadBitmapFromResourceOrFile(const std::wstring& fallbackPath) {
    // Try embedded resource first
    HRSRC hRes = FindResourceA(NULL, MAKEINTRESOURCEA(IDR_PNG_LOGO), RT_RCDATA);
    if (hRes) {
        HGLOBAL hData = LoadResource(NULL, hRes);
        if (hData) {
            DWORD size = SizeofResource(NULL, hRes);
            void* pData = LockResource(hData);
            if (pData && size > 0) {
                // Create IStream from resource data
                HGLOBAL hCopy = GlobalAlloc(GMEM_MOVEABLE, size);
                if (hCopy) {
                    void* pCopy = GlobalLock(hCopy);
                    memcpy(pCopy, pData, size);
                    GlobalUnlock(hCopy);
                    IStream* pStream = NULL;
                    if (CreateStreamOnHGlobal(hCopy, TRUE, &pStream) == S_OK) {
                        Gdiplus::Bitmap* bitmap = Gdiplus::Bitmap::FromStream(pStream);
                        pStream->Release();
                        if (bitmap && bitmap->GetLastStatus() == Gdiplus::Ok) {
                            return bitmap;
                        }
                        delete bitmap;
                    }
                    GlobalFree(hCopy);
                }
            }
        }
    }
    // Fallback: load from file
    Gdiplus::Bitmap* bitmap = Gdiplus::Bitmap::FromFile(fallbackPath.c_str());
    if (bitmap && bitmap->GetLastStatus() == Gdiplus::Ok) {
        return bitmap;
    }
    delete bitmap;
    return nullptr;
}

static HICON LoadIconFromPNG(const std::wstring& path, int targetSize) {
    Gdiplus::Bitmap* original = LoadBitmapFromResourceOrFile(path);
    if (!original) return nullptr;
    Gdiplus::Bitmap* resized = new Gdiplus::Bitmap(targetSize, targetSize, PixelFormat32bppARGB);
    Gdiplus::Graphics g(resized);
    g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    g.DrawImage(original, 0, 0, targetSize, targetSize);
    HICON hIcon = nullptr;
    resized->GetHICON(&hIcon);
    delete original;
    delete resized;
    return hIcon;
}

static bool SetGLFWWindowIcon(const std::wstring& path, GLFWwindow* window) {
    Gdiplus::Bitmap* bitmap = LoadBitmapFromResourceOrFile(path);
    if (!bitmap) return false;
    int w = (int)bitmap->GetWidth();
    int h = (int)bitmap->GetHeight();
    Gdiplus::Rect rect(0, 0, w, h);
    Gdiplus::BitmapData bd;
    if (bitmap->LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bd) != Gdiplus::Ok) {
        delete bitmap;
        return false;
    }
    // Convert BGRA -> RGBA
    std::vector<uint8_t> rgba(w * h * 4);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint8_t* src = (uint8_t*)bd.Scan0 + y * bd.Stride + x * 4;
            uint8_t* dst = rgba.data() + (y * w + x) * 4;
            dst[0] = src[2]; // R
            dst[1] = src[1]; // G
            dst[2] = src[0]; // B
            dst[3] = src[3]; // A
        }
    }
    bitmap->UnlockBits(&bd);
    delete bitmap;

    GLFWimage image;
    image.width = w;
    image.height = h;
    image.pixels = rgba.data();
    glfwSetWindowIcon(window, 1, &image);
    return true;
}

static LRESULT CALLBACK TrayWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_TRAYICON) {
        auto& loc = Localization::getInstance();
        if (LOWORD(lp) == WM_RBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);
            HMENU hMenu = CreatePopupMenu();
            std::wstring wShow = UTF8ToWide(loc.get("SHOW_WINDOW"));
            std::wstring wExit = UTF8ToWide(loc.get("EXIT"));
            AppendMenuW(hMenu, MF_STRING, 1, wShow.c_str());
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hMenu, MF_STRING, 2, wExit.c_str());
            SetForegroundWindow(hwnd);
            int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, NULL);
            DestroyMenu(hMenu);
            if (cmd == 1) {
                glfwShowWindow(g_window);
                glfwFocusWindow(g_window);
            } else if (cmd == 2) {
                g_shouldExit = true;
                glfwPostEmptyEvent();
            }
        } else if (LOWORD(lp) == WM_LBUTTONDBLCLK) {
            glfwShowWindow(g_window);
            glfwFocusWindow(g_window);
        }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void InitTray(HICON hIcon) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = TrayWndProc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"HtkisProTrayClass";
    RegisterClassW(&wc);

    g_hwndTray = CreateWindowW(L"HtkisProTrayClass", L"", 0, 0, 0, 0, 0,
        HWND_MESSAGE, NULL, GetModuleHandleW(NULL), NULL);

    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = g_hwndTray;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = hIcon;
    wcscpy_s(g_nid.szTip, L"HtkisPro");
    Shell_NotifyIconW(NIM_ADD, &g_nid);
}

static void CleanupTray() {
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
    if (g_hwndTray) DestroyWindow(g_hwndTray);
    if (g_hIcon) DestroyIcon(g_hIcon);
}

static void ProcessTrayMessages() {
    MSG msg;
    while (PeekMessageW(&msg, g_hwndTray, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}
#endif // _WIN32

// ===== GLFW Callbacks =====
static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

static void window_close_callback(GLFWwindow* window) {
    // Hide to tray instead of closing
    glfwSetWindowShouldClose(window, GLFW_FALSE);
    glfwHideWindow(window);
}

// ===== UI Helpers =====
void LoadFonts() {
    ImGuiIO& io = ImGui::GetIO();

    static ImVector<ImWchar> ranges;
    ImFontGlyphRangesBuilder builder;
    builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
    builder.AddRanges(io.Fonts->GetGlyphRangesChineseFull());
    builder.AddRanges(io.Fonts->GetGlyphRangesThai());
    builder.AddRanges(io.Fonts->GetGlyphRangesVietnamese());
    builder.BuildRanges(&ranges);

    const char* font_candidates[] = {
#ifdef _WIN32
        "C:\\Windows\\Fonts\\msyh.ttc",
        "C:\\Windows\\Fonts\\simhei.ttf",
        "C:\\Windows\\Fonts\\simsun.ttc",
        "C:\\Windows\\Fonts\\arialuni.ttf",
#else
        "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
        "/System/Library/Fonts/PingFang.ttc",
        "/System/Library/Fonts/Supplemental/Thonburi.ttc",
#endif
    };

    bool loaded = false;
    for (const char* font_path : font_candidates) {
        FILE* f = fopen(font_path, "rb");
        if (f) {
            fclose(f);
            io.Fonts->AddFontFromFileTTF(font_path, 18.0f, NULL, ranges.Data);
            loaded = true;
            break;
        }
    }

    if (!loaded) {
        io.Fonts->AddFontDefault();
    }
}

void ApplyCustomStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    style.WindowRounding = 8.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.ScrollbarRounding = 6.0f;
    style.TabRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ChildRounding = 6.0f;

    style.WindowPadding = ImVec2(16, 16);
    style.FramePadding = ImVec2(8, 5);
    style.ItemSpacing = ImVec2(10, 6);
    style.ItemInnerSpacing = ImVec2(6, 4);

    colors[ImGuiCol_WindowBg]           = ImVec4(0.12f, 0.13f, 0.17f, 1.00f);
    colors[ImGuiCol_ChildBg]            = ImVec4(0.15f, 0.16f, 0.21f, 1.00f);
    colors[ImGuiCol_Border]             = ImVec4(0.25f, 0.27f, 0.33f, 0.50f);
    colors[ImGuiCol_FrameBg]            = ImVec4(0.20f, 0.21f, 0.27f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]     = ImVec4(0.26f, 0.28f, 0.35f, 1.00f);
    colors[ImGuiCol_FrameBgActive]      = ImVec4(0.30f, 0.32f, 0.40f, 1.00f);
    colors[ImGuiCol_TitleBg]            = ImVec4(0.10f, 0.11f, 0.15f, 1.00f);
    colors[ImGuiCol_TitleBgActive]      = ImVec4(0.14f, 0.15f, 0.20f, 1.00f);
    colors[ImGuiCol_MenuBarBg]          = ImVec4(0.14f, 0.15f, 0.20f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]        = ImVec4(0.10f, 0.11f, 0.15f, 0.60f);
    colors[ImGuiCol_ScrollbarGrab]      = ImVec4(0.30f, 0.32f, 0.40f, 0.60f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.38f, 0.40f, 0.50f, 0.70f);
    colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.45f, 0.47f, 0.58f, 0.80f);
    colors[ImGuiCol_CheckMark]          = ImVec4(0.40f, 0.70f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrab]         = ImVec4(0.40f, 0.70f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]   = ImVec4(0.55f, 0.80f, 1.00f, 1.00f);
    colors[ImGuiCol_Button]             = ImVec4(0.25f, 0.45f, 0.75f, 0.80f);
    colors[ImGuiCol_ButtonHovered]      = ImVec4(0.30f, 0.55f, 0.85f, 1.00f);
    colors[ImGuiCol_ButtonActive]       = ImVec4(0.20f, 0.38f, 0.65f, 1.00f);
    colors[ImGuiCol_Header]             = ImVec4(0.22f, 0.38f, 0.60f, 0.60f);
    colors[ImGuiCol_HeaderHovered]      = ImVec4(0.28f, 0.48f, 0.72f, 0.80f);
    colors[ImGuiCol_HeaderActive]       = ImVec4(0.22f, 0.40f, 0.65f, 1.00f);
    colors[ImGuiCol_Separator]          = ImVec4(0.28f, 0.30f, 0.37f, 0.60f);
    colors[ImGuiCol_TableHeaderBg]      = ImVec4(0.16f, 0.18f, 0.24f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]  = ImVec4(0.30f, 0.32f, 0.40f, 1.00f);
    colors[ImGuiCol_TableBorderLight]   = ImVec4(0.22f, 0.24f, 0.30f, 0.60f);
    colors[ImGuiCol_TableRowBg]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]      = ImVec4(0.06f, 0.06f, 0.10f, 0.40f);
    colors[ImGuiCol_Text]               = ImVec4(0.88f, 0.90f, 0.94f, 1.00f);
    colors[ImGuiCol_TextDisabled]       = ImVec4(0.50f, 0.52f, 0.58f, 1.00f);
    colors[ImGuiCol_PopupBg]            = ImVec4(0.14f, 0.15f, 0.20f, 0.96f);
}

void StatusIndicator(bool running) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float r = 6.0f;
    ImU32 color = running ? IM_COL32(80, 220, 100, 255) : IM_COL32(220, 60, 60, 255);
    draw_list->AddCircleFilled(ImVec2(p.x + r, p.y + r), r, color);
    ImGui::Dummy(ImVec2(r * 2 + 4, r * 2));
}

int main(int, char**) {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;

    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    auto& loc = Localization::getInstance();

    GLFWwindow* window = glfwCreateWindow(960, 720, "HtkisPro", NULL, NULL);
    if (!window) return 1;
    g_window = window;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // Set close callback - hide to tray instead of closing
    glfwSetWindowCloseCallback(window, window_close_callback);

#ifdef _WIN32
    // Initialize GDI+
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL);

    // Load icon from PNG
    std::wstring exeDir = GetExeDir();
    std::wstring pngPath = exeDir + L"\\HtkisPro.png";

    // Try exe directory first, then current directory
    DWORD attrs = GetFileAttributesW(pngPath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        pngPath = L"HtkisPro.png";
    }

    // Set GLFW window icon (title bar + taskbar)
    SetGLFWWindowIcon(pngPath, window);

    // Create tray icon (32x32)
    g_hIcon = LoadIconFromPNG(pngPath, 32);
    InitTray(g_hIcon ? g_hIcon : LoadIcon(NULL, IDI_APPLICATION));
#endif

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.IniFilename = NULL;

    ApplyCustomStyle();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    LoadFonts();

    UserManager::getInstance().load("users.txt");

    // State
    static int port = LanProxy::DEFAULT_SERVER_PORT;
    static bool show_about = false;
    static char username_buf[128] = {};
    static char password_buf[128] = {};
    static int days = 30;
    static std::vector<std::pair<std::string, UserInfo>> users;
    static std::vector<LogEntry> logEntries;
    static bool autoScroll = true;

    // ===== Main Loop =====
    while (!g_shouldExit) {
        glfwPollEvents();

#ifdef _WIN32
        ProcessTrayMessages();
#endif

        if (g_shouldExit) break;

        // When window is hidden, sleep and skip rendering
        if (!glfwGetWindowAttrib(window, GLFW_VISIBLE)) {
#ifdef _WIN32
            Sleep(50);
#else
            usleep(50000);
#endif
            continue;
        }

        glfwSetWindowTitle(window, "HtkisPro");

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ===== Menu Bar =====
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

        // ===== About Modal =====
        if (show_about) {
            ImGui::OpenPopup(loc.get("ABOUT").c_str());
            show_about = false;
        }
        if (ImGui::BeginPopupModal(loc.get("ABOUT").c_str(), NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("HtkisPro");
            ImGui::Separator();
            ImGui::Text("%s: Hunt", loc.get("DEVELOPER").c_str());
            ImGui::Text("%s: admin@cii.sh.cn", loc.get("EMAIL").c_str());
            ImGui::Spacing();
            if (ImGui::Button("OK", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // ===== Main Window =====
        {
            int display_w, display_h;
            glfwGetFramebufferSize(window, &display_w, &display_h);
            ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetFrameHeight()));
            ImGui::SetNextWindowSize(ImVec2((float)display_w, (float)display_h - ImGui::GetFrameHeight()));

            ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

            ImGui::Begin("##Main", NULL, flags);

            auto& app = ServerApp::getInstance();

            // ===== Left Panel =====
            float panelWidth = 280.0f;
            ImGui::BeginChild("##LeftPanel", ImVec2(panelWidth, 0), true);

            // Status
            ImGui::TextColored(ImVec4(0.6f, 0.7f, 0.85f, 1.0f), "%s", loc.get("SERVICE_STATUS").c_str());
            ImGui::Spacing();

            StatusIndicator(app.isRunning());
            ImGui::SameLine();
            if (app.isRunning()) {
                ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "%s", loc.get("STATUS_RUNNING").c_str());
            } else {
                ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "%s", loc.get("STATUS_STOPPED").c_str());
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Port config
            ImGui::TextColored(ImVec4(0.6f, 0.7f, 0.85f, 1.0f), "%s", loc.get("PROXY_CONFIG").c_str());
            ImGui::Spacing();

            if (!app.isRunning()) {
                ImGui::SetNextItemWidth(-1);
                ImGui::InputInt("##port", &port);
                if (port < 1) port = 1;
                if (port > 65535) port = 65535;
            } else {
                ImGui::Text("%s: %d", loc.get("SERVER_PORT").c_str(), app.getPort());
            }

            ImGui::Spacing();

            // Start/Stop button
            if (app.isRunning()) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 0.9f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.25f, 0.25f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.15f, 0.15f, 1.0f));
                if (ImGui::Button(loc.get("STOP_SERVER").c_str(), ImVec2(-1, 36))) {
                    app.stop();
                }
                ImGui::PopStyleColor(3);
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.55f, 0.3f, 0.9f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.65f, 0.35f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.45f, 0.25f, 1.0f));
                if (ImGui::Button(loc.get("START_SERVER").c_str(), ImVec2(-1, 36))) {
                    app.start(port);
                }
                ImGui::PopStyleColor(3);
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Protocol info
            ImGui::TextColored(ImVec4(0.6f, 0.7f, 0.85f, 1.0f), "%s", loc.get("PROTOCOL_INFO").c_str());
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.55f, 0.6f, 0.7f, 1.0f), "HTTP CONNECT");
            ImGui::TextColored(ImVec4(0.55f, 0.6f, 0.7f, 1.0f), "SOCKS5");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Usage hint
            ImGui::TextWrapped("%s", loc.get("USAGE_HINT").c_str());

            ImGui::EndChild();

            // ===== Right Panel =====
            ImGui::SameLine();
            ImGui::BeginChild("##RightPanel", ImVec2(0, 0), false);

            // ---- Top: User Management ----
            float rightHeight = ImGui::GetContentRegionAvail().y;
            float userPanelHeight = rightHeight * 0.45f;
            float logPanelHeight = rightHeight - userPanelHeight;

            ImGui::BeginChild("##UserPanel", ImVec2(0, userPanelHeight), true);

            ImGui::TextColored(ImVec4(0.6f, 0.7f, 0.85f, 1.0f), "%s", loc.get("USER_MANAGEMENT").c_str());
            ImGui::Spacing();

            // Add user form
            float formWidth = ImGui::GetContentRegionAvail().x;
            float fieldWidth = (formWidth - ImGui::GetStyle().ItemSpacing.x * 2) / 3.0f;

            ImGui::SetNextItemWidth(fieldWidth);
            ImGui::InputText(loc.get("USERNAME").c_str(), username_buf, IM_ARRAYSIZE(username_buf));
            ImGui::SameLine();
            ImGui::SetNextItemWidth(fieldWidth);
            ImGui::InputText(loc.get("PASSWORD").c_str(), password_buf, IM_ARRAYSIZE(password_buf), ImGuiInputTextFlags_Password);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(fieldWidth);
            ImGui::InputInt(loc.get("DAYS").c_str(), &days);
            if (days < 1) days = 1;

            ImGui::Spacing();

            float btnWidth = 120.0f;
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.45f, 0.7f, 0.9f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.55f, 0.82f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.38f, 0.6f, 1.0f));
            if (ImGui::Button(loc.get("ADD_USER").c_str(), ImVec2(btnWidth, 28))) {
                if (strlen(username_buf) > 0 && strlen(password_buf) > 0) {
                    UserManager::getInstance().addUser(username_buf, password_buf, days);
                    UserManager::getInstance().save();
                    username_buf[0] = '\0';
                    password_buf[0] = '\0';
                    users = UserManager::getInstance().getAllUsers();
                }
            }
            ImGui::PopStyleColor(3);

            ImGui::SameLine();
            if (ImGui::Button(loc.get("REFRESH_LIST").c_str(), ImVec2(btnWidth, 28))) {
                users = UserManager::getInstance().getAllUsers();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Users Table
            if (ImGui::BeginTable("##Users", 4,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                ImGuiTableFlags_Resizable,
                ImVec2(0, ImGui::GetContentRegionAvail().y))) {

                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn(loc.get("USERNAME").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.3f);
                ImGui::TableSetupColumn(loc.get("ACTIVE").c_str(), ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableSetupColumn(loc.get("EXPIRE_TIME").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.4f);
                ImGui::TableSetupColumn(loc.get("ACTIONS").c_str(), ImGuiTableColumnFlags_WidthFixed, 140.0f);
                ImGui::TableHeadersRow();

                for (const auto& user : users) {
                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%s", user.first.c_str());

                    ImGui::TableSetColumnIndex(1);
                    if (user.second.active) {
                        ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "O");
                        ImGui::SameLine();
                        ImGui::Text("%s", loc.get("YES").c_str());
                    } else {
                        ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "X");
                        ImGui::SameLine();
                        ImGui::Text("%s", loc.get("NO").c_str());
                    }

                    ImGui::TableSetColumnIndex(2);
                    char timeBuf[64] = {};
                    struct tm tm_buf;
#ifdef _WIN32
                    localtime_s(&tm_buf, &user.second.expire_time);
#else
                    localtime_r(&user.second.expire_time, &tm_buf);
#endif
                    strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M", &tm_buf);
                    ImGui::Text("%s", timeBuf);

                    ImGui::TableSetColumnIndex(3);
                    std::string toggleLabel = std::string(user.second.active ? loc.get("DEACTIVATE") : loc.get("ACTIVATE")) + "##" + user.first;
                    if (ImGui::SmallButton(toggleLabel.c_str())) {
                        UserManager::getInstance().setStatus(user.first, !user.second.active);
                        UserManager::getInstance().save();
                        users = UserManager::getInstance().getAllUsers();
                        break;
                    }
                    ImGui::SameLine();
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.15f, 0.15f, 0.8f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.75f, 0.2f, 0.2f, 1.0f));
                    std::string delLabel = std::string(loc.get("DELETE").c_str()) + "##" + user.first;
                    if (ImGui::SmallButton(delLabel.c_str())) {
                        UserManager::getInstance().removeUser(user.first);
                        UserManager::getInstance().save();
                        users = UserManager::getInstance().getAllUsers();
                        break;
                    }
                    ImGui::PopStyleColor(2);
                }
                ImGui::EndTable();
            }

            ImGui::EndChild();

            // ---- Bottom: Log Panel ----
            ImGui::BeginChild("##LogPanel", ImVec2(0, logPanelHeight), true);

            ImGui::TextColored(ImVec4(0.6f, 0.7f, 0.85f, 1.0f), "%s", loc.get("LOG_PANEL").c_str());
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60);
            if (ImGui::SmallButton(loc.get("CLEAR").c_str())) {
                Logger::getInstance().clear();
                logEntries.clear();
            }
            ImGui::SameLine();
            ImGui::Checkbox(loc.get("AUTO_SCROLL").c_str(), &autoScroll);

            ImGui::Separator();

            // Fetch new logs
            if (Logger::getInstance().hasNewEntries()) {
                logEntries = Logger::getInstance().getEntries();
                Logger::getInstance().markRead();
            }

            ImGui::BeginChild("##LogScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

            for (const auto& entry : logEntries) {
                char timeBuf[32] = {};
                struct tm tm_buf;
#ifdef _WIN32
                localtime_s(&tm_buf, &entry.time);
#else
                localtime_r(&entry.time, &tm_buf);
#endif
                strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &tm_buf);

                ImVec4 color;
                const char* prefix;
                switch (entry.level) {
                case LogLevel::Info:  color = ImVec4(0.6f, 0.8f, 1.0f, 1.0f); prefix = "INFO";  break;
                case LogLevel::Warn:  color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f); prefix = "WARN";  break;
                case LogLevel::Error: color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f); prefix = "ERR ";  break;
                default:              color = ImVec4(0.7f, 0.7f, 0.7f, 1.0f); prefix = "????";  break;
                }

                ImGui::TextDisabled("%s", timeBuf);
                ImGui::SameLine();
                ImGui::TextColored(color, "[%s]", prefix);
                ImGui::SameLine();
                ImGui::Text("%s", entry.message.c_str());
            }

            if (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                ImGui::SetScrollHereY(1.0f);
            }

            ImGui::EndChild();
            ImGui::EndChild();

            ImGui::EndChild(); // RightPanel

            ImGui::End(); // Main
        }

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.10f, 0.11f, 0.14f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    // ===== Cleanup =====
    ServerApp::getInstance().stop();
    UserManager::getInstance().save();

#ifdef _WIN32
    CleanupTray();
    Gdiplus::GdiplusShutdown(g_gdiplusToken);
#endif

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
