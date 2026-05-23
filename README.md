<div align="center">

# HtkisPro

**HTTP CONNECT + SOCKS5 Dual-Protocol Proxy Server**

[![Platform](https://img.shields.io/badge/platform-Windows-blue.svg)](https://github.com/wujupeng/HtkisPro)
[![License](https://img.shields.io/badge/license-Private-red.svg)](LICENSE)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/std/the-standard)

A lightweight, secure proxy server with user authentication, real-time logging, and a modern dark-themed GUI. Designed for LAN email proxy and general-purpose proxy use cases.

</div>

---

## Features

- **Dual Protocol** - Auto-detects HTTP CONNECT and SOCKS5 proxy requests on the same port
- **User Authentication** - Username/password auth with configurable expiration dates
- **Modern GUI** - Dark-themed ImGui interface with real-time log panel
- **System Tray** - Minimize to tray on window close; exit only via tray menu
- **Full Logging** - Complete session lifecycle: connect, auth, DNS, tunnel, close with traffic stats
- **Embedded Icon** - Application icon compiled into the exe, no external files needed
- **Multi-Language** - Supports 6 languages: English, 简体中文, Tiếng Việt, ไทย, Español, Magyar

## Screenshots

> Main interface with service running, user management, and real-time log panel.

## Quick Start

### Download

Download the latest release from [Releases](https://github.com/wujupeng/HtkisPro/releases).

### Usage

1. Run `proxy_server.exe`
2. Set the proxy port in the left panel, click **Start Service**
3. Add users in the **User Management** panel (username, password, validity days)
4. Configure proxy in your browser or email client:
   - **Address**: Server IP + Port
   - **Type**: HTTP or SOCKS5
   - **Auth**: Username and Password

### System Tray

- Click window **X** button → Hide to system tray (service keeps running)
- Double-click tray icon → Show main window
- Right-click tray icon → Show window / Exit program

## Build from Source

### Prerequisites

- CMake 3.14+
- Visual Studio 2019+ (MSVC, with C++17 support)
- Git (for FetchContent dependencies)

Dependencies are automatically downloaded via CMake FetchContent:
- [asio](https://github.com/chriskohlhoff/asio) (standalone)
- [GLFW](https://github.com/glfw/glfw) 3.4
- [Dear ImGui](https://github.com/ocornut/imgui) v1.91.6

### Build

```bash
# In VS Developer PowerShell
cmake -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release
```

Output: `build/Release/proxy_server.exe`

## Project Structure

```
src/
├── server/
│   ├── Server.cpp/h        # Core proxy engine (HTTP CONNECT + SOCKS5)
│   ├── GuiMain.cpp          # ImGui GUI with tray support
│   ├── UserManager.cpp/h    # User auth & management
│   ├── Logger.h             # Thread-safe logging system
│   ├── resource.h/rc        # Windows resources (embedded icon)
│   └── Protocol.h           # Protocol constants
└── common/
    └── Localization.cpp/h   # Multi-language support (6 languages)
```

## Log Format

Full session lifecycle logging:

```
[CONN]   New connection from 192.168.2.40:50506
[SOCKS5] Auth success user=hunt from=192.168.2.40:50506
[SOCKS5] CONNECT smtp.example.com:465 user=hunt from=192.168.2.40:50506
[DNS]    Resolving smtp.example.com ...
[DNS]    Resolved smtp.example.com -> 2 endpoint(s)
[SOCKS5] Tunnel established -> smtp.example.com:465 user=hunt remote=10.0.0.1:465
[CLOSE]  192.168.2.40:50506 user=hunt target=smtp.example.com:465 reason=client disconnect up=1.2 KB down=8.5 KB duration=2m 35s
```

## Supported Languages

| Language | Name |
|----------|------|
| English | English |
| Chinese | 简体中文 |
| Vietnamese | Tiếng Việt |
| Thai | ไทย |
| Spanish | Español (México) |
| Hungarian | Magyar |

## License

Private - All rights reserved.
