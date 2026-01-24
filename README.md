# Htkis-LANProxySerCli (C++ Version)

Htkis LAN Proxy Server & Client implemented in C++.

## Introduction
这是一个内网穿透工具的服务端与客户端项目，使用 C++14 和 Asio 网络库开发。
This is a LAN proxy server and client project implemented in C++.

## Project Structure
- `src/server`: 服务端源码 (Server source code)
- `src/client`: 客户端源码 (Client source code)
- `src/common`: 公共协议定义 (Common protocol definitions)
- `third_party`: 第三方依赖 (Third-party dependencies)

## Dependencies
本项目依赖 **Asio** (Standalone version) 进行网络编程。
This project depends on **Asio** (Standalone version) for networking.

### How to setup Asio
1. 下载 Asio (C++11/14/17/20 适用版本): [Download Asio](https://think-async.com/Asio/Download.html)
2. 解压并将 `include` 目录放入 `third_party/asio/`。
   (Extract and place the `include` directory into `third_party/asio/`)
   
   Expected structure:
   ```
   Htkis-LANProxySerCli/
   ├── third_party/
   │   └── asio/
   │       └── include/
   │           └── asio.hpp
   ```

## Build Instructions

### Prerequisites
- CMake 3.10+
- C++ Compiler (GCC, Clang, or MSVC) supporting C++14

### Build Steps
```bash
mkdir build
cd build
cmake ..
cmake --build .
```

## Usage
### Server
```bash
./lanproxy-server
```
Server listens on:
- Port 4900: Client Control Connection
- Port 8080: User Proxy Connection

### Client
```bash
./lanproxy-client <server_ip> <server_port> <local_ip> <local_port>
```
Example:
```bash
./lanproxy-client 127.0.0.1 4900 127.0.0.1 80
```
