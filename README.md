# Htkis-LANProxySerCli (C++ Version)

Htkis LAN Proxy Server & Client implemented in C++.
高性能局域网代理服务端与客户端，支持 SOCKS5/HTTP 协议智能分流与上游代理转发。

## ✨ 主要功能 (Features)

*   **双协议支持**：客户端同时支持 **SOCKS5** 和 **HTTP CONNECT** 代理协议，完美适配 Windows 系统代理设置。
*   **智能分流**：内置智能路由策略：
    *   🇨🇳 国内网站 (CN/Baidu/QQ 等) -> **直连** (Direct)，速度快且不消耗代理流量。
    *   🌍 国外网站 -> **代理转发** (Proxy) -> 服务端。
*   **上游代理支持 (Upstream Proxy)**：服务端支持将流量转发给上游 SOCKS5 代理（如 v2rayN），实现科学上网。
*   **高性能**：基于 **Asio** 异步网络库开发，支持高并发连接。
*   **跨平台**：支持 Windows (MinGW/MSVC), Linux, macOS。

## 🛠️ 构建与安装 (Build & Install)

本项目使用 CMake 进行构建管理。

### 前置要求 (Prerequisites)
*   CMake 3.10+
*   C++ Compiler (GCC, Clang, or MSVC) supporting C++14/17
*   [Asio](https://think-async.com/Asio/) (Header-only, automatically handled by `setup_env.ps1` on Windows)

### 快速构建 (Windows PowerShell)
我们提供了一键环境配置脚本，自动下载 MinGW、CMake 和 Asio：

```powershell
# 1. 运行环境配置脚本
./setup_env.ps1

# 2. 设置环境变量 (临时)
$env:PATH = "$PWD\env\cmake\bin;$PWD\env\mingw64\bin;$env:PATH"

# 3. 编译项目
cmake -B build
cmake --build build --config Release
```

编译完成后，可执行文件位于 `build/` 目录：
*   `lanproxy-server.exe`
*   `lanproxy-client.exe`

## 📖 使用说明 (Usage)

### 1. 服务端 (Server)

服务端负责接收客户端流量，并可选择转发给上游代理（如 v2rayN）。

**交互式启动：**
直接运行程序，根据提示配置上游代理：
```bash
./build/lanproxy-server.exe
```
*   程序会询问是否启用上游代理 (Use Upstream Proxy?)。
*   若启用 (输入 `y`)，需提供上游 SOCKS5 代理的 IP (默认 127.0.0.1) 和端口 (默认 10808)。

**命令行启动：**
```bash
# 格式: lanproxy-server <port> [upstream_ip] [upstream_port]

# 仅作为普通 LAN 代理 (无上游转发)
./build/lanproxy-server.exe 4900

# 启用上游转发 (例如转发给 v2rayN)
./build/lanproxy-server.exe 4900 127.0.0.1 10808
```

### 2. 客户端 (Client)

客户端运行在用户电脑上，为浏览器或系统提供 SOCKS5/HTTP 代理入口。

**交互式启动：**
直接运行程序，可通过菜单修改配置：
```bash
./build/lanproxy-client.exe
```

**命令行启动：**
```bash
# 格式: lanproxy-client <server_ip> <server_port> <local_port>

# 连接到本地服务端 127.0.0.1:4900，在本地 1080 端口开启代理
./build/lanproxy-client.exe 127.0.0.1 4900 1080
```

### 3. 浏览器/系统配置

启动客户端后，请在系统或浏览器中设置代理：
*   **协议**：SOCKS5 或 HTTP
*   **地址**：127.0.0.1
*   **端口**：1080 (或您设置的 local_port)

## � 项目结构 (Project Structure)
- `src/server`: 服务端源码 (Server source code)
- `src/client`: 客户端源码 (Client source code)
- `src/common`: 公共协议与工具 (Protocol & Utils)
- `third_party`: 第三方依赖 (Asio)

## 📄 许可证 (License)
MIT License
