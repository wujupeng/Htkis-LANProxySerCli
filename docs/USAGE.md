# Htkis-LANProxySerCli 使用手册

> **版本**: v1.0.0-debian-server | **平台**: Debian 13 (Trixie)

---

## 1. 概述

Htkis-LANProxySerCli 是一个局域网透明代理网关，集成 v2rayN 核心，提供：

- **SOCKS5 代理**: 带用户认证的 SOCKS5 服务，支持远程 DNS 解析
- **智能路由**: 基于 GeoIP + 域名规则的自动分流（国内直连，国外代理）
- **多节点管理**: VMess 节点添加/删除/切换，活动节点动态选择
- **Web 管理界面**: 浏览器管理节点、用户、路由规则
- **用户认证**: 管理员 + 普通用户两级认证体系
- **并发调度**: 连接数限制、速率控制、过载保护

## 2. 架构概览

    客户端 → [SOCKS5 :10800] → 网关 → 路由引擎 → v2ray [10808] → VMess 服务器
                                                     ↓ (直连规则)
                                                   Freedom (直出)

    管理员 → [HTTP :8080] → Web UI / REST API

## 3. 首次登录

1. 浏览器访问 `http://SERVER_IP:8080`
2. 使用管理员账号登录（默认用户名 `admin`，密码在部署时设定）
3. 登录后获取 JWT Token，后续 API 调用需携带 `Authorization: Bearer <token>`

## 4. VMess 节点管理

### 4.1 查看节点列表

    GET /api/v2rayn/vmess/nodes

响应示例：

    {
        "active_node": "vmess-1",
        "nodes": [
            {"tag": "vmess-1", "address": "tokyo.example.com", "port": 443, "network": "ws", "tls": "tls", ...},
            {"tag": "vmess-2", "address": "la.example.com", "port": 443, "network": "ws", "tls": "tls", ...}
        ]
    }

### 4.2 通过 vmess:// 链接导入节点

    POST /api/v2rayn/vmess/import
    Content-Type: application/json

    {"vmess_link": "vmess://BASE64_ENCODED_JSON"}

### 4.3 解析 vmess:// 链接（不导入）

    POST /api/v2rayn/vmess/parse
    Content-Type: application/json

    {"vmess_link": "vmess://BASE64_ENCODED_JSON"}

### 4.4 手动添加节点

    POST /api/v2rayn/vmess/nodes
    Content-Type: application/json

    {
        "tag": "vmess-3",
        "address": "server.example.com",
        "port": 443,
        "user_id": "UUID",
        "alter_id": 0,
        "security": "auto",
        "network": "ws",
        "tls": "tls",
        "ws_config": {"path": "/ws", "host": "example.com"},
        "tls_config": {"allow_insecure": false, "server_name": ""},
        "remark": "备注名称"
    }

### 4.5 修改节点

    PUT /api/v2rayn/vmess/nodes/{old_tag}
    Content-Type: application/json

    {节点完整配置，同添加格式}

### 4.6 删除节点

    DELETE /api/v2rayn/vmess/nodes/{tag}

> 至少保留一个 VMess 节点。

### 4.7 导出 vmess:// 链接

    POST /api/v2rayn/vmess/export/{tag}

### 4.8 节点排序

    PUT /api/v2rayn/vmess/nodes/reorder
    Content-Type: application/json

    {"ordered_tags": ["vmess-3", "vmess-1", "vmess-2"]}

## 5. 活动节点管理

活动节点是 v2ray 路由规则中所有 VMess 流量默认使用的出站节点。

### 5.1 查询当前活动节点

    GET /api/v2rayn/vmess/active

响应：

    {"active_node": "vmess-1"}

### 5.2 切换活动节点

    POST /api/v2rayn/vmess/active
    Content-Type: application/json

    {"node_tag": "vmess-2"}

切换后自动：
1. 更新 v2ray 路由规则中所有 vmess-* 出站的 outboundTag
2. 保存配置到磁盘
3. 重启 v2ray 进程

### 5.3 切换并应用配置

    POST /api/v2rayn/vmess/apply
    Content-Type: application/json

    {"node_tag": "vmess-3"}   // node_tag 可选，不传则使用当前活动节点

## 6. 用户管理

### 6.1 用户体系

| 角色 | 来源 | 认证方式 |
|------|------|----------|
| 管理员 (admin) | `config/default_system.json` | bcrypt 哈希验证 |
| 普通用户 | `data/users.json` | bcrypt 哈希验证 |

管理员和普通用户均可通过 SOCKS5 代理认证。

### 6.2 查看用户列表

    GET /api/users

### 6.3 添加用户

    POST /api/users
    Content-Type: application/json

    {"username": "user1", "password": "plaintext", "days": 30}

- `days`: 有效天数，0 表示永久有效

### 6.4 修改用户密码

    PUT /api/users/{username}/password
    Content-Type: application/json

    {"new_password": "new_plaintext"}

### 6.5 启用/禁用用户

    PUT /api/users/{username}/status
    Content-Type: application/json

    {"active": true}

### 6.6 删除用户

    DELETE /api/users/{username}

## 7. 路由规则

### 7.1 内置规则

系统预置 31 条分流规则，涵盖：
- `.cn` 域名直连
- 国内主要网站直连（baidu.com, qq.com, taobao.com 等）
- 常见国外域名走代理（google.com, github.com, youtube.com 等）

查看内置规则：

    GET /api/routes/builtin

### 7.2 自定义规则

    GET /api/routes/custom          # 查看自定义规则
    POST /api/routes/custom         # 添加规则
    DELETE /api/routes/custom/{id}  # 删除规则

规则格式：

    {
        "pattern": "*.example.com",
        "action": "direct",    // "direct" 或 "proxy"
        "type": "domain"       // "domain" 或 "ip"
    }

### 7.3 GeoIP 分流

系统加载 MaxMind GeoLite2-Country 数据库，对域名解析后的 IP 自动判断：
- 中国 IP → 直连
- 其他 IP → 走代理（如活动节点为 VMess）

## 8. 客户端配置

### 8.1 浏览器代理设置

以 Firefox 为例：

1. 设置 → 网络设置 → 手动代理配置
2. SOCKS 主机: `SERVER_IP`，端口: `10800`，选择 SOCKS v5
3. 勾选 "使用 SOCKS v5 代理 DNS"

### 8.2 命令行使用

    # SOCKS5 代理（推荐 socks5h 远程 DNS 解析）
    curl -x 'socks5h://admin:PASSWORD@SERVER_IP:10800' http://www.google.com

    # 普通用户认证
    curl -x 'socks5h://username:PASSWORD@SERVER_IP:10800' http://www.google.com

> **重要**: 使用 `socks5h://` 而非 `socks5://`，`h` 表示 DNS 查询也通过代理解析（防止 DNS 泄漏）。

### 8.3 系统全局代理

Linux 环境变量：

    export http_proxy='socks5h://admin:PASSWORD@SERVER_IP:10800'
    export https_proxy='socks5h://admin:PASSWORD@SERVER_IP:10800'
    export ALL_PROXY='socks5h://admin:PASSWORD@SERVER_IP:10800'

### 8.4 Windows 客户端

使用 Proxifier / Sockscap64 等工具，配置 SOCKS5 代理指向网关地址。

## 9. 系统监控

### 9.1 系统信息

    GET /api/system/info

### 9.2 并发连接统计

    GET /api/concurrency/stats

响应：

    {
        "active_connections": 42,
        "total_connections": 1050,
        "queued_connections": 0,
        "max_connections": 3000,
        "is_overloaded": false
    }

### 9.3 节点健康状态

    GET /api/concurrency/nodes

### 9.4 健康检查

    GET /health

## 10. 服务管理

    # 启动
    sudo systemctl start lan-proxy-gateway

    # 停止（同时停止 v2ray 子进程）
    sudo systemctl stop lan-proxy-gateway

    # 重启
    sudo systemctl restart lan-proxy-gateway

    # 查看状态
    sudo systemctl status lan-proxy-gateway

    # 查看日志
    journalctl -u lan-proxy-gateway -f

    # 查看网关应用日志
    tail -f /opt/lan-proxy-gateway/logs/*.log

    # 查看 v2ray 日志
    tail -f /opt/lan-proxy-gateway/logs/v2ray.log

## 11. 常见问题

### Q: SOCKS5 认证失败？

确认用户名密码正确。管理员账号存储在 `config/default_system.json`，普通用户在 `data/users.json`。密码以 bcrypt 哈希存储。

### Q: 切换节点后代理不生效？

切换活动节点会自动重启 v2ray，需等待 3-5 秒。如仍未生效，检查：

    ps aux | grep v2ray

确认只有一个 v2ray 进程。多个 v2ray 进程会导致端口冲突。

### Q: 代理返回 000？

1. 检查 v2ray 是否运行: `ss -tlnp | grep 10808`
2. 测试 v2ray 直连: `curl -x 'socks5h://127.0.0.1:10808' http://www.google.com`
3. 如 v2ray 直连正常但网关不通，检查用户认证

### Q: 日志文件清空后不再写入？

spdlog 持有旧文件句柄，使用 `> file` 清空日志会导致新日志无法写入。解决方法：重启服务。

### Q: 如何修改代理端口？

编辑 `config/default_system.json` 中的 `proxy_port` 字段，然后重启服务。

## 12. API 快速参考

| 方法 | 路径 | 说明 |
|------|------|------|
| POST | /api/auth/login | 登录获取 Token |
| GET | /api/auth/status | 认证状态 |
| GET | /api/v2rayn/vmess/nodes | 节点列表 |
| POST | /api/v2rayn/vmess/nodes | 添加节点 |
| PUT | /api/v2rayn/vmess/nodes/{tag} | 修改节点 |
| DELETE | /api/v2rayn/vmess/nodes/{tag} | 删除节点 |
| PUT | /api/v2rayn/vmess/nodes/reorder | 节点排序 |
| GET | /api/v2rayn/vmess/active | 查询活动节点 |
| POST | /api/v2rayn/vmess/active | 切换活动节点 |
| POST | /api/v2rayn/vmess/apply | 应用配置 |
| POST | /api/v2rayn/vmess/import | 导入 vmess:// 链接 |
| POST | /api/v2rayn/vmess/parse | 解析 vmess:// 链接 |
| POST | /api/v2rayn/vmess/export/{tag} | 导出 vmess:// 链接 |
| GET | /api/users | 用户列表 |
| POST | /api/users | 添加用户 |
| DELETE | /api/users/{username} | 删除用户 |
| GET | /api/routes/builtin | 内置路由规则 |
| GET | /api/routes/custom | 自定义路由规则 |
| GET | /api/concurrency/stats | 并发统计 |
| GET | /api/concurrency/nodes | 节点健康状态 |
| GET | /health | 健康检查 |

> 所有 `/api/*` 端点（除 `/api/auth/login`）需携带 `Authorization: Bearer <token>` 请求头。
