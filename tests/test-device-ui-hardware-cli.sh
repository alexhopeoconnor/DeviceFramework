#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
tool="$project_dir/tools/device-ui-hardware"

bash -n "$tool" "$project_dir/tools/lib/device-ui-session.sh" \
    "$project_dir/tools/lib/platformio.sh" "$project_dir/tools/check-ota-partitions.sh"
node --check "$project_dir/tests/device-ui/tests/portal-ota.integration.spec.cjs"

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
if "$tool" ota-portal --platform esp8266 --profile invalid >/dev/null 2>&1; then
    echo "device-ui-hardware accepted an invalid OTA portal profile" >&2
    exit 1
fi
if "$tool" ota-portal --platform esp8266 --env-file unexpected >/dev/null 2>&1; then
    echo "device-ui-hardware accepted a station environment for ota-portal" >&2
    exit 1
fi
if "$tool" portal --platform esp8266 --profile protected >/dev/null 2>&1; then
    echo "device-ui-hardware accepted an OTA portal profile for the ordinary portal command" >&2
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
rg -Fq 'wait_for_web_url "df-portal-${platform}.local"' "$tool"
rg -Fq 'Do not let `pipefail` bypass this bounded retry loop.' "$tool"
rg -Fq "awk 'NR == 1 { print \$2; exit }' || true" "$tool"
if rg -Fq 'DEVICEFRAMEWORK_TEST_DEVICE_HOST' "$tool"; then
    echo "device-ui-hardware retained the retired normal-test IP override" >&2
    exit 1
fi
rg -Fq 'wait_for_serial_port' "$tool"
rg -Fq 'Serial port did not return after erase: $port' "$tool"
rg -Fq 'ota-portal --platform esp8266|esp32 --port DEVICE' "$tool"
rg -Fq 'Portal OTA image B' "$tool"
rg -Fq 'The PlatformIO portal A build changed after its immutable artifact was captured.' "$tool"
rg -Fq 'ota-portal)' "$tool"
rg -Fq 'wifi-sec.key-mgmt wpa-psk' "$project_dir/tools/lib/device-ui-session.sh"
rg -Fq 'An empty fixture password deliberately means an open portal' \
    "$project_dir/tools/lib/device-ui-session.sh"
rg -Fq 'esp8266_portal_ota_protected_a' "$project_dir/test/ota-harness/platformio.base.ini"
rg -Fq 'esp32_portal_ota_open_b' "$project_dir/test/ota-harness/platformio.base.ini"
rg -Fq 'board_build.partitions = ../../partitions/esp32_ota_4m_no_fs.csv' \
    "$project_dir/test/ota-harness/platformio.base.ini"
rg -Fq 'DF_PORTAL_OTA_IMAGE' "$project_dir/test/ota-harness/src/main.cpp"
rg -Fq 'firmware-marker' "$project_dir/tests/device-ui/tests/portal-ota.integration.spec.cjs"
rg -Fq 'automatic A-to-B reboot' "$project_dir/tests/device-ui/tests/portal-ota.integration.spec.cjs"
rg -Fq 'DEVICE_UI_OTA_FIRMWARE' "$project_dir/tests/device-ui/compose.ota.yaml"
node -e 'for (const file of process.argv.slice(1)) JSON.parse(require("fs").readFileSync(file, "utf8"));' \
    "$project_dir/test/profiles/ota-portal-protected.json" \
    "$project_dir/test/profiles/ota-portal-open.json"

echo "DeviceFramework device-UI CLI contract passed"
