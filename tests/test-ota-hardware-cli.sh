#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
tool="$project_dir/tools/ota-hardware"
cd "$project_dir"

bash -n "$tool" "$project_dir/tools/check-ota-partitions.sh" \
    "$project_dir/tools/lib/platformio.sh"
python3 -m py_compile "$project_dir/tools/capture-serial-boot.py"
python3 "$project_dir/tests/test-capture-serial-boot.py"
"$project_dir/tools/check-ota-partitions.sh"

fixture_port="$(mktemp)"
helper_tmp="$(mktemp -d)"
trap 'rm -f -- "$fixture_port"; rm -rf -- "$helper_tmp"' EXIT

mkdir -p "$helper_tmp/home/.platformio/penv/bin"
printf '%s\n' '#!/usr/bin/env bash' 'exit 0' > "$helper_tmp/home/.platformio/penv/bin/pio"
chmod 755 "$helper_tmp/home/.platformio/penv/bin/pio"
(
    export HOME="$helper_tmp/home"
    export PATH=/usr/bin:/bin
    # shellcheck source=tools/lib/platformio.sh
    source "$project_dir/tools/lib/platformio.sh"
    [[ "$(df_pio_executable)" == "$HOME/.platformio/penv/bin/pio" ]]
)

if "$tool" >/dev/null 2>&1; then
    echo "ota-hardware accepted an empty command" >&2
    exit 1
fi
if "$tool" arduino --platform esp32 --port "$fixture_port" --defer-mdns --device-ip 192.168.1.44 >/dev/null 2>&1; then
    echo "ota-hardware accepted ESP32 deferred-mDNS mode" >&2
    exit 1
fi
if "$tool" arduino --platform esp8266 --port "$fixture_port" --defer-mdns >/dev/null 2>&1; then
    echo "ota-hardware accepted deferred-mDNS without an explicit IP" >&2
    exit 1
fi

rg -Fq 'mDNS: not exercised; diagnostic transport mode only.' "$tool"
rg -Fq 'Avahi and system resolver agree' "$tool"
rg -Fq 'Wrong ArduinoOTA password was rejected as required.' "$tool"
rg -Fq 'firmware_a_artifact' "$tool"
rg -Fq "printf 'OTA %s fits:" "$tool"
rg -Fq 'platformio.local.ini.ota-hardware' "$tool"
rg -Fq 'profile_build_dir="$run_dir/platformio-build"' "$tool"
rg -Fq 'umask 077' "$tool"
rg -Fq 'mDNS responder deferred for low heap:' "$tool"
rg -Fq 'no resolver response observed' "$tool"
rg -Fq 'monitor_upload_started_file' "$tool"
rg -Fq 'stale A-era cache cannot stand in for' "$tool"
rg -Fq 'DeviceFramework UDP OTA fixture image:' "$tool"
rg -Fq 'ota-lan-open-fixture.json' "$tool"
rg -Fq 'DEVICEFRAMEWORK_TEST_*=*' "$tool"
rg -Fq 'DEVICEFRAMEWORK_PIO_EXECUTABLE' "$project_dir/tools/lib/platformio.sh"
rg -Fq 'esp8266_udp_ota_deferred_b' "$project_dir/test/ota-harness/platformio.base.ini"
rg -Fq 'board_build.ldscript = eagle.flash.4m1m.ld' "$project_dir/test/ota-harness/platformio.base.ini"
rg -Fq 'setConfigSerialBaudRate(115200)' "$project_dir/test/ota-harness/src/main.cpp"
rg -Fq 'DeviceFrameworkRTC::clear();' "$project_dir/test/ota-harness/src/main.cpp"
rg -Fq 'kSerialMonitorAttachDelayMs' "$project_dir/test/ota-harness/src/main.cpp"
rg -Fq -- '--df-accent:#2477c9' "$project_dir/scripts/test.sh"
rg -Fq 'esp8266_default_hardware' "$project_dir/test/compile-project/platformio.base.ini"
rg -Fq 'esp32_default_hardware' "$project_dir/test/compile-project/platformio.base.ini"
rg -Fq 'hardware_default_profile' "$project_dir/scripts/test.sh"
rg -Fq '"${platform}_default_hardware"' "$project_dir/scripts/test.sh"
rg -Fq 'if [[ "$ha_e2e" == "true" ]]; then' "$project_dir/scripts/test.sh"
rg -Fq 'HA E2E owns the Unity image' "$project_dir/scripts/test.sh"
rg -Fq 'expected transient lookup failure' "$project_dir/scripts/test.sh"
rg -Fq '`set -e -o pipefail`' "$tool"
rg -Fq "awk 'NR == 1 {print \$2; exit}' || true" "$tool"
if sed -n '/^capture_a_boot()/,/^}/p' "$tool" | rg -Fq -- '--reset'; then
    echo "Normal ArduinoOTA boot capture must not manufacture a second reset" >&2
    exit 1
fi

python3 - <<'PY'
import json
from pathlib import Path

root = Path.cwd()
for name in ("ota-lan-protected-fixture.json", "ota-lan-open-fixture.json"):
    document = json.loads((root / "test" / "profiles" / name).read_text())
    assert document["format"] == 2
    assert document["wifi"]["profiles"][0]["ssid"] == "ota-fixture-network"
PY

echo "DeviceFramework ArduinoOTA hardware CLI contract passed"
