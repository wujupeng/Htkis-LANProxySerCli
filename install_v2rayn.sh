#!/bin/bash
set -euo pipefail

V2RAYN_DIR="${1:-/home/debian/LanProxySerCli/v2rayn}"
V2RAY_VERSION="v5.16.1"

echo "=== Installing v2ray-core ${V2RAY_VERSION} ==="

mkdir -p "${V2RAYN_DIR}"

echo "Downloading v2ray-core..."
curl -sL "https://github.com/v2fly/v2ray-core/releases/download/${V2RAY_VERSION}/v2ray-linux-64.zip" \
    -o /tmp/v2ray.zip

echo "Extracting..."
unzip -o /tmp/v2ray.zip -d "${V2RAYN_DIR}"
chmod +x "${V2RAYN_DIR}/v2ray"

if [[ ! -f "${V2RAYN_DIR}/config.json" ]]; then
    echo "Generating default v2ray config..."
    cat > "${V2RAYN_DIR}/config.json" << 'V2RAYCONF'
{
    "log": { "loglevel": "warning" },
    "inbounds": [
        {
            "tag": "socks-in",
            "protocol": "socks",
            "listen": "127.0.0.1",
            "port": 10808,
            "settings": { "auth": "noauth", "udp": false }
        },
        {
            "tag": "http-in",
            "protocol": "http",
            "listen": "127.0.0.1",
            "port": 10809
        }
    ],
    "outbounds": [
        {
            "protocol": "freedom",
            "tag": "direct"
        }
    ]
}
V2RAYCONF
fi

rm -f /tmp/v2ray.zip

echo "v2ray-core installed to ${V2RAYN_DIR}"
echo "Configure your outbound servers in ${V2RAYN_DIR}/config.json"
