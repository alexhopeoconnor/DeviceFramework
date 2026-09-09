#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
tool="$project_dir/tools/device-ui-hardware"

bash -n "$tool" "$project_dir/tools/lib/device-ui-session.sh"

if "$tool" >/dev/null 2>&1; then
    echo "device-ui-hardware accepted an empty command" >&2
    exit 1
fi

if "$tool" down --output unexpected >/dev/null 2>&1; then
    echo "device-ui-hardware accepted unsupported down arguments" >&2
    exit 1
fi
if "$tool" portal --platform esp8266 --capture-readme-media >/dev/null 2>&1; then
    echo "device-ui-hardware accepted README media capture outside full ESP32 mode" >&2
    exit 1
fi

rg -Fq 'Only the explicit secondary adapter may be reconfigured' \
    "$project_dir/tools/lib/device-ui-session.sh"
rg -Fq 'Refusing to use the host default-route interface' \
    "$project_dir/tools/lib/device-ui-session.sh"
rg -Fq 'artifacts/device-ui/' "$project_dir/.gitignore"
rg -Fq 'artifacts/readme-media/' "$project_dir/.gitignore"
rg -Fq 'fps=5,scale=600:750' "$project_dir/tests/device-ui/render-readme-media.sh"
rg -Fq 'README media GIF exceeds its 2 MiB documentation budget' \
    "$project_dir/tests/device-ui/render-readme-media.sh"
rg -Fq 'allow_configured_host="${2:-yes}"' "$tool"
rg -Fq 'wait_for_web_url "df-portal-${platform}.local" no' "$tool"
rg -Fq 'wait_for_serial_port' "$tool"
rg -Fq 'Serial port did not return after erase: $port' "$tool"

echo "DeviceFramework device-UI CLI contract passed"
