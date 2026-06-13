# Htkis-LANProxySerCli 部署教程

> **版本**: v1.0.0-debian-server | **平台**: Debian 13 (Trixie) | **架构**: amd64

---

## 1. 系统要求

| 项目 | 最低要求 |
|------|----------|
| 操作系统 | Debian 13 (Trixie) 或兼容 Linux 发行版 |
| CPU | 2 核 |
| 内存 | 2 GB |
| 磁盘 | 1 GB 可用空间（含 v2ray + GeoIP 数据库） |
| 网络 | 能访问目标 VMess 服务器 |

## 2. 依赖安装

### 2.1 编译工具链与系统依赖

    sudo apt update
    sudo apt install -y build-essential cmake git pkg-config \
        libssl-dev libmaxminddb-dev libspdlog-dev \
        nlohmann-json3-dev python3 python3-pip

### 2.2 Python bcrypt（用于密码哈希）

    pip3 install bcrypt --break-system-packages

> **说明**: 网关使用 Python bcrypt 模块生成管理员密码哈希。Debian 13 的 `crypt()` 不支持 bcrypt（`$2a$`/`$2b$` 前缀返回 `*0`），因此通过 hex 编码调用 Python 生成哈希。

### 2.3 v2ray 核心

    sudo mkdir -p /opt/lan-proxy-gateway/v2rayn
    cd /opt/lan-proxy-gateway/v2rayn

下载 v2ray-core (v5.x)：

    # 从 https://github.com/v2fly/v2ray-core/releases 下载 linux-amd64 版本
    # 解压后将 v2ray 可执行文件放到 /opt/lan-proxy-gateway/v2rayn/
    chmod +x v2ray

验证：

    ./v2ray version

### 2.4 GeoIP 数据库

从 [MaxMind](https://dev.maxmind.com/geoip/geolite2-free-geolocation-data) 下载 `GeoLite2-Country.mmdb`，放置到：

    /opt/lan-proxy-gateway/data/GeoLite2-Country.mmdb

## 3. 编译

### 3.1 获取源码

    git clone https://github.com/wujupeng/Htkis-LANProxySerCli.git
    cd Htkis-LANProxySerCli

### 3.2 CMake 构建

    mkdir build && cd build
    cmake -DCMAKE_BUILD_TYPE=Release ..
    make -j$(nproc)

编译产物: `build/lan_proxy_gateway`

### 3.3 自定义安装路径

如需修改默认安装路径 `/opt/lan-proxy-gateway`，编译前修改以下文件中的路径：

- `CMakeLists.txt` 中的 `CROW_STATIC_DIRECTORY`
- `config/default_system.json.template` 中所有路径字段

## 4. 部署

### 4.1 创建目录结构

    sudo mkdir -p /opt/lan-proxy-gateway/{config,data,logs,v2rayn}

### 4.2 安装文件

    sudo cp build/lan_proxy_gateway /opt/lan-proxy-gateway/
    sudo cp config/default_system.json.template /opt/lan-proxy-gateway/config/default_system.json
    sudo cp data/builtin_rules.json /opt/lan-proxy-gateway/data/
    sudo cp data/custom_rules.json /opt/lan-proxy-gateway/data/
    sudo cp data/default_users.json /opt/lan-proxy-gateway/data/

### 4.3 生成管理员密码

首次启动前，需要为 `admin` 用户生成密码哈希并写入配置：

    python3 -c "
import bcrypt
password = 'YOUR_ADMIN_PASSWORD'
hashed = bcrypt.hashpw(password.encode(), bcrypt.gensalt(10)).decode()
print(f'Password hash: {hashed}')
"

将输出的哈希值填入 `/opt/lan-proxy-gateway/config/default_system.json` 的 `admin_password_hash` 字段。

### 4.4 配置文件说明

编辑 `/opt/lan-proxy-gateway/config/default_system.json`：

| 字段 | 默认值 | 说明 |
|------|--------|------|
| `proxy_port` | 10800 | SOCKS5/HTTP 代理监听端口 |
| `web_ui_port` | 8080 | Web 管理界面端口 |
| `proxy_thread_count` | 4 | 代理工作线程数 |
| `v2rayn_executable_path` | `/opt/lan-proxy-gateway/v2rayn/v2ray` | v2ray 可执行文件路径 |
| `v2rayn_config_path` | `/opt/lan-proxy-gateway/v2rayn/config.json` | v2ray 配置文件路径 |
| `v2rayn_local_socks_port` | 10808 | v2ray 本地 SOCKS5 端口 |
| `v2rayn_local_http_port` | 10809 | v2ray 本地 HTTP 代理端口 |
| `admin_username` | admin | 管理员用户名 |
| `admin_password_hash` | (必填) | bcrypt 哈希，空则启动失败 |
| `jwt_secret` | (自动生成) | JWT 签名密钥，首次启动自动生成 |

> **安全提示**: `jwt_secret` 和 `admin_password_hash` 在运行时会被写入配置文件，确保该文件权限为 `600`：

    sudo chmod 600 /opt/lan-proxy-gateway/config/default_system.json

### 4.5 配置 v2ray 初始节点

创建 `/opt/lan-proxy-gateway/v2rayn/config.json`，至少包含一个 VMess outbound：

    {
        "log": {"loglevel": "warning"},
        "inbounds": [
            {
                "tag": "socks-in",
                "port": 10808,
                "listen": "127.0.0.1",
                "protocol": "socks",
                "settings": {"auth": "noauth", "udp": true},
                "sniffing": {"enabled": true, "destOverride": ["http", "tls"]}
            }
        ],
        "outbounds": [
            {
                "tag": "vmess-1",
                "protocol": "vmess",
                "settings": {
                    "vnext": [{"address": "YOUR_SERVER", "port": 443, "users": [{"id": "YOUR_UUID", "alterId": 0, "security": "auto"}]}]
                },
                "streamSettings": {"network": "ws", "security": "tls", "wsSettings": {"path": "/path"}}
            },
            {"protocol": "freedom", "tag": "direct"},
            {"protocol": "blackhole", "tag": "block"}
        ],
        "routing": {
            "rules": [
                {"type": "field", "outboundTag": "vmess-1", "network": "tcp,udp"}
            ]
        }
    }

> **替代方式**: 启动网关后，通过 Web UI 或 API 导入 vmess:// 链接，自动生成配置。

### 4.6 配置 systemd 服务

创建 `/etc/systemd/system/lan-proxy-gateway.service`：

    [Unit]
    Description=LAN Proxy Gateway - Transparent Proxy with v2rayN Integration
    After=network-online.target
    Wants=network-online.target

    [Service]
    Type=simple
    User=debian
    Group=debian
    WorkingDirectory=/opt/lan-proxy-gateway
    ExecStartPre=/bin/bash -c 'pkill -9 v2ray 2>/dev/null || true'
    ExecStart=/opt/lan-proxy-gateway/lan_proxy_gateway
    ExecStop=/bin/bash -c 'pkill -9 v2ray 2>/dev/null || true'
    Restart=on-failure
    RestartSec=5
    LimitNOFILE=65535
    LimitNPROC=65535
    Environment=PATH=/usr/local/bin:/usr/bin:/bin
    Environment=HOME=/home/debian

    [Install]
    WantedBy=multi-user.target

> **注意**: 将 `User`/`Group` 替换为实际运行用户，确保该用户对 `/opt/lan-proxy-gateway/` 有读写权限。

启用并启动：

    sudo systemctl daemon-reload
    sudo systemctl enable lan-proxy-gateway
    sudo systemctl start lan-proxy-gateway

验证：

    sudo systemctl status lan-proxy-gateway
    ss -tlnp | grep -E '10800|8080'

## 5. 防火墙配置

### 5.1 nftables（Debian 13 默认）

    sudo nft add table inet filter
    sudo nft add chain inet filter input '{ type filter hook input priority 0 ; policy accept ; }'
    sudo nft add rule inet filter input tcp dport 8080 accept
    sudo nft add rule inet filter input tcp dport 10800 accept

### 5.2 iptables

    sudo iptables -A INPUT -p tcp --dport 8080 -j ACCEPT
    sudo iptables -A INPUT -p tcp --dport 10800 -j ACCEPT

### 5.3 ufw

    sudo ufw allow 8080/tcp
    sudo ufw allow 10800/tcp

> **安全建议**: 仅允许内网 IP 段访问 10800 端口（SOCKS5 代理），8080（Web UI）可视需求开放。

## 6. 验证部署

### 6.1 检查服务状态

    sudo systemctl status lan-proxy-gateway

### 6.2 测试 Web UI

浏览器访问 `http://SERVER_IP:8080`，使用管理员账号登录。

### 6.3 测试 SOCKS5 代理

    curl -x 'socks5h://admin:YOUR_PASSWORD@127.0.0.1:10800' http://www.google.com -o /dev/null -w '%{http_code}'

期望返回 `302` 或 `301`。

### 6.4 测试直连

    curl -x 'socks5h://admin:YOUR_PASSWORD@127.0.0.1:10800' http://www.baidu.com -o /dev/null -w '%{http_code}'

期望返回 `200`（.cn 域名走直连）。

## 7. 升级

    cd Htkis-LANProxySerCli
    git pull
    cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && make -j$(nproc)
    sudo systemctl stop lan-proxy-gateway
    sudo cp build/lan_proxy_gateway /opt/lan-proxy-gateway/
    sudo systemctl start lan-proxy-gateway

## 8. 故障排查

| 问题 | 检查方法 |
|------|----------|
| 服务启动失败 | `journalctl -u lan-proxy-gateway -n 50` |
| SOCKS5 认证失败 | 检查 `admin_password_hash` 是否正确填入 |
| 无法代理 | 检查 v2ray 进程：`ps aux \| grep v2ray` |
| 僵尸 v2ray 进程 | `pkill -9 v2ray && sudo systemctl restart lan-proxy-gateway` |
| 日志不写入 | 重启服务（spdlog 持有旧文件句柄） |
| 端口被占用 | `ss -tlnp \| grep PORT` |

## 9. 卸载

    sudo systemctl stop lan-proxy-gateway
    sudo systemctl disable lan-proxy-gateway
    sudo rm /etc/systemd/system/lan-proxy-gateway.service
    sudo systemctl daemon-reload
    sudo rm -rf /opt/lan-proxy-gateway
