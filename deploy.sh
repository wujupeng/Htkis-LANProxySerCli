#!/bin/bash
set -euo pipefail

DEPLOY_DIR="/home/debian/LanProxySerCli"
REPO_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "=== LAN Proxy Gateway Deployment Script ==="
echo "Target: ${DEPLOY_DIR}"

if [[ "$(id -u)" -ne 0 ]]; then
    echo "Please run as root (sudo)"
    exit 1
fi

echo "[1/8] Installing system dependencies..."
apt-get update
apt-get install -y build-essential cmake git libssl-dev libmaxminddb-dev pkg-config curl

echo "[2/8] Creating deployment directory..."
mkdir -p "${DEPLOY_DIR}"/{src,config,data,logs,web,v2rayn,systemd}

echo "[3/8] Building C++ backend..."
cd "${REPO_DIR}"
if [[ -d build ]]; then rm -rf build; fi
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j"$(nproc)"
cp lan_proxy_gateway "${DEPLOY_DIR}/"

echo "[4/8] Deploying configuration files..."
cp -r config/* "${DEPLOY_DIR}/config/"
cp -r data/* "${DEPLOY_DIR}/data/"
cp -r systemd/* "${DEPLOY_DIR}/systemd/"

echo "[5/8] Installing v2rayN..."
bash "${REPO_DIR}/install_v2rayn.sh" "${DEPLOY_DIR}/v2rayn"

echo "[6/8] Downloading GeoIP database..."
if [[ ! -f "${DEPLOY_DIR}/data/GeoLite2-Country.mmdb" ]]; then
    echo "Downloading GeoLite2-Country.mmdb..."
    curl -sL "https://git.io/GeoLite2-Country.mmdb" -o "${DEPLOY_DIR}/data/GeoLite2-Country.mmdb" || \
    echo "WARNING: GeoIP DB download failed. You need to manually place GeoLite2-Country.mmdb in ${DEPLOY_DIR}/data/"
fi

echo "[7/8] Generating self-signed certificate..."
if [[ ! -f "${DEPLOY_DIR}/config/cert.pem" ]]; then
    openssl req -x509 -newkey rsa:2048 -keyout "${DEPLOY_DIR}/config/key.pem" \
        -out "${DEPLOY_DIR}/config/cert.pem" -days 365 -nodes \
        -subj "/CN=lan-proxy-gateway" 2>/dev/null
    echo "Self-signed certificate generated. Replace with production cert if needed."
fi

echo "[8/8] Installing systemd service..."
cp "${DEPLOY_DIR}/systemd/lan-proxy-gateway.service" /etc/systemd/system/
systemctl daemon-reload
systemctl enable lan-proxy-gateway

echo ""
echo "=== Deployment Complete ==="
echo "Service:  sudo systemctl start lan-proxy-gateway"
echo "Status:   sudo systemctl status lan-proxy-gateway"
echo "Logs:     sudo journalctl -u lan-proxy-gateway -f"
echo "Web UI:   https://192.168.2.97:8080"
echo "Proxy:    SOCKS5/HTTP on port 10800"
echo ""
echo "IMPORTANT: Set admin password in ${DEPLOY_DIR}/config/system.json"
