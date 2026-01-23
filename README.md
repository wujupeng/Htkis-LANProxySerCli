# Htkis-LANProxySerCli

Htkis-LANProxySerCli 是一个高性能、跨平台的 SOCKS5 代理服务器与客户端应用，配备了现代化的图形用户界面 (GUI)。本项目旨在提供简单易用的局域网代理解决方案，支持多用户管理、流量转发及多语言界面。

## ✨ 主要功能 (Features)

*   **SOCKS5 协议支持**：完整支持 SOCKS5 代理协议，确保高效、安全的网络转发。
*   **图形用户界面 (GUI)**：基于 Dear ImGui 构建的现代化界面，操作直观便捷。
*   **多语言支持 (Multi-language)**：
    *   🇺🇸 English (英语)
    *   🇨🇳 简体中文 (Simplified Chinese)
    *   🇻🇳 Tiếng Việt (越南语)
    *   🇹🇭 ไทย (泰语)
    *   🇲🇽 Español (墨西哥西班牙语)
    *   🇭🇺 Magyar (匈牙利语)
    *   支持动态语言切换，无需重启。
*   **强大的字体渲染**：针对 macOS 优化，支持 CJK (中日韩)、泰语、越南语等多种字符集的完美显示。
*   **用户管理系统**：
    *   添加/删除用户
    *   启用/禁用用户账户
    *   设置账户有效期
    *   实时刷新用户列表
*   **跨平台设计**：支持 macOS (已验证), Linux, Windows。

## 🛠️ 构建与安装 (Build & Install)

本项目使用 CMake 进行构建管理。

### 前置要求
*   C++17 兼容的编译器 (Clang, GCC, MSVC)
*   CMake 3.14+
*   OpenGL 3.2+
*   macOS (推荐): 系统自带的 Cocoa, IOKit, CoreVideo 库

### 构建步骤

```bash
# 1. 克隆仓库
git clone https://github.com/wujupeng/Htkis-LANProxySerCli.git
cd Htkis-LANProxySerCli

# 2. 创建构建目录
mkdir build && cd build

# 3. 配置项目
cmake ..

# 4. 编译
cmake --build .
```

编译完成后，你将在 `build` 目录下找到 `proxy_server_gui` 和 `proxy_client_gui` 可执行文件。

## 📖 用户使用说明书 (User Manual)

### 1. 服务端 (Server)

**启动程序**：运行 `proxy_server_gui`。

**界面概览**：
*   **语言切换**：点击顶部菜单栏的 `Language` (语言)，选择你偏好的语言。
*   **关于**：点击 `About` (关于) 查看开发者信息。
*   **用户管理 (User Management)**：
    *   **添加用户**：输入用户名 (Username)、密码 (Password) 和有效天数 (Days)，点击 `Add User` (添加用户)。
    *   **用户列表**：点击 `Refresh List` (刷新列表) 查看当前所有用户。
    *   **操作**：
        *   `Activate`/`Deactivate`：启用或停用某个用户。
        *   `Delete`：永久删除用户。

**默认配置**：服务端默认监听端口通常为 `1080` (可在代码或配置文件中修改)。

### 2. 客户端 (Client)

**启动程序**：运行 `proxy_client_gui`。

**连接设置**：
*   **Server IP (服务器 IP)**：输入服务端的 IP 地址 (如本机测试可用 `127.0.0.1`)。
*   **Server Port (服务器端口)**：输入服务端监听的端口 (默认 1080)。
*   **Username (用户名)**：输入在服务端创建的用户名。
*   **Password (密码)**：输入对应的密码。
*   **Local Port (本地端口)**：设置本地 SOCKS5 代理监听端口 (默认 1081)。

**操作**：
*   点击 **Connect (连接)** 按钮启动代理。
*   若连接成功，状态将显示为绿色 `CONNECTED`。
*   此时，请配置您的浏览器或系统代理，指向 `127.0.0.1:[Local Port]` (SOCKS5)。

## 👨‍💻 开发者信息 (Developer)

*   **Developer**: Hunt
*   **Email**: admin@cii.sh.cn
*   **Project**: Htkis-LANProxySerCli

## 📄 许可证 (License)

MIT License
