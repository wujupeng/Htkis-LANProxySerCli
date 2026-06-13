<div align="center">

# Htkis-LANProxySerCli

**LAN Transparent Proxy Gateway with v2rayN Integration**

[![Platform](https://img.shields.io/badge/platform-Debian%2013-red.svg)](https://www.debian.org/)
[![License](https://img.shields.io/badge/license-Private-red.svg)](LICENSE)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/std/the-standard)
[![Version](https://img.shields.io/badge/version-v1.0.0--debian--server-green.svg)]()

**Debian Server Edition** - v1.0.0

</div>

---

## Overview

Htkis-LANProxySerCli is a high-performance LAN transparent proxy gateway designed for Debian servers. It integrates v2rayN core to provide SOCKS5 proxy with user authentication, intelligent routing (GeoIP + domain rules), multi-node management, and a Web administration interface.

## Key Features

- **SOCKS5 Proxy with Auth** - Username/password authentication, remote DNS resolution (socks5h)
- **Intelligent Routing** - Auto-split: China domestic domains/IPs go direct, foreign traffic via VMess
- **Multi-Node Management** - Add, edit, delete, reorder VMess nodes; switch active node on-the-fly
- **Active Node Switching** - Dynamically change the default outbound VMess node without restart
- **Web Admin UI** - Browser-based management at port 8080
- **User Management** - Admin + normal users with bcrypt password hashing, expiration support
- **Concurrency Control** - Connection limiting, rate limiting, overload protection (3000+ concurrent)
- **Health Monitoring** - Node health checks, real-time metrics, WebSocket log streaming
- **Systemd Integration** - Auto-start on boot, proper v2ray lifecycle management

## Architecture

```
Client --> [SOCKS5 :10800] --> Gateway --> Route Engine --> v2ray [:10808] --> VMess Server
                                                             | (direct rule)
                                                           Freedom (direct)
Admin  --> [HTTP :8080] --> Web UI / REST API
```

## Quick Start

See [Deployment Guide](docs/DEPLOY.md) for full instructions.

### Prerequisites

- Debian 13 (Trixie) or compatible Linux
- v2ray-core v5.x
- GeoLite2-Country.mmdb

### Build

```bash
sudo apt install -y build-essential cmake pkg-config libssl-dev libmaxminddb-dev \
    libspdlog-dev nlohmann-json3-dev python3 python3-pip
pip3 install bcrypt --break-system-packages

git clone https://github.com/wujupeng/Htkis-LANProxySerCli.git
cd Htkis-LANProxySerCli
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

### Deploy

```bash
# Copy binary and configs
sudo cp build/lan_proxy_gateway /opt/lan-proxy-gateway/
sudo cp config/default_system.json.template /opt/lan-proxy-gateway/config/default_system.json

# Generate admin password hash
python3 -c "import bcrypt; print(bcrypt.hashpw(b'YOUR_PASSWORD', bcrypt.gensalt(10)).decode())"

# Edit config: fill admin_password_hash, adjust paths
sudo nano /opt/lan-proxy-gateway/config/default_system.json

# Install systemd service, start
sudo cp /etc/systemd/system/lan-proxy-gateway.service  # (see docs/DEPLOY.md)
sudo systemctl enable --now lan-proxy-gateway
```

### Verify

```bash
curl -x 'socks5h://admin:PASSWORD@127.0.0.1:10800' http://www.google.com -o /dev/null -w '%{http_code}'
# Expected: 301 or 302
```

## Documentation

| Document | Description |
|----------|-------------|
| [DEPLOY.md](docs/DEPLOY.md) | Full deployment guide with dependencies, configuration, systemd setup |
| [USAGE.md](docs/USAGE.md) | User manual: API reference, client configuration, troubleshooting |

## Project Structure

```
src/
├── proxy_core/          # SOCKS5 proxy engine (session, server)
├── route_decision/      # Routing engine (GeoIP, domain matching, DNS cache)
├── v2rayn_manager/      # v2ray process & config management
│   ├── vm_node_manager  # Node CRUD, active node tracking
│   ├── vm_config_builder# v2ray config generation
│   ├── vm_link_codec    # vmess:// link encode/decode
│   ├── vm_node_validator# Node validation
│   └── v2rayn_process   # Process lifecycle (fork/exec/signal)
├── web_server/          # Crow HTTP server, JWT auth, REST API
├── user_manager/        # User auth, bcrypt password hashing
├── concurrency/         # Connection limiting, rate limiter, load balancer, health checker
├── log_monitor/         # Structured logging, metrics, audit, WebSocket sink
└── config_manager/      # JSON config loader with auto-generation
config/
├── default_system.json.template  # Configuration template (no secrets)
data/
├── builtin_rules.json   # 31 pre-defined routing rules
├── custom_rules.json    # User-defined rules
└── default_users.json   # Empty user template
```

## API Overview

| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | /api/auth/login | Login, get JWT token |
| GET | /api/v2rayn/vmess/nodes | List VMess nodes + active node |
| POST | /api/v2rayn/vmess/import | Import vmess:// link |
| GET/POST | /api/v2rayn/vmess/active | Get/set active node |
| POST | /api/v2rayn/vmess/apply | Apply config (optional node_tag) |
| GET/POST | /api/users | User management |
| GET | /api/concurrency/stats | Connection statistics |
| GET | /health | Health check |

> All `/api/*` endpoints require `Authorization: Bearer <token>` header (except login).

## Client Configuration

### curl

```bash
curl -x 'socks5h://admin:PASSWORD@SERVER_IP:10800' http://www.google.com
```

### Browser (Firefox)

Settings → Network → Manual proxy → SOCKS5 host: `SERVER_IP:10800`, check "Proxy DNS"

### Environment Variables

```bash
export ALL_PROXY='socks5h://admin:PASSWORD@SERVER_IP:10800'
```

## Security Notes

- Admin password stored as bcrypt hash in config file
- JWT tokens with configurable secret (auto-generated on first run)
- No plaintext credentials in source code or Git repository
- Config files should have `chmod 600` permissions
- v2ray local ports (10808/10809) bind to 127.0.0.1 only
- Recommend restricting SOCKS5 port (10800) to internal network via firewall

## Version

| Version | Date | Description |
|---------|------|-------------|
| v1.0.0-debian-server | 2026-06-13 | Initial Debian Server Edition release |

## License

Private - All rights reserved.
