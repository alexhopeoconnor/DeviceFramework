#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
tool="$project_dir/tools/device-ui-hardware"
tmp="$(mktemp -d "${TMPDIR:-/tmp}/deviceframework-device-ui-cli.XXXXXX")"
cleanup_tmp() { rm -rf "$tmp"; }
trap cleanup_tmp EXIT

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

# Exercise the real runner trap with disposable command stubs. A TERM
# immediately after `connection add` must remove its exact pending name and
# clear state before the runner can continue to another board operation.
stub_bin="$tmp/bin"
signal_log="$tmp/signal-calls.log"
mkdir -p "$stub_bin"
printf '%s\n' '#!/usr/bin/env bash' \
'if [[ "$1" == "link" && "$2" == "show" ]]; then exit 0; fi' \
'if [[ "$1" == "route" && "$2" == "show" ]]; then echo "default via 192.0.2.1 dev wlan-main"; exit 0; fi' \
'echo "192.168.4.1 dev wlan-client src 192.168.4.2"' >"$stub_bin/ip"
printf '%s\n' '#!/usr/bin/env bash' \
'printf "%s\\n" "$*" >>"$SIGNAL_LOG"' \
'if [[ "$1" == "-t" && "$2" == "-f" && "$3" == "SSID" ]]; then echo "DF-Portal-ESP8266"; exit 0; fi' \
'if [[ "$1" == "-g" && "$2" == "connection.uuid" ]]; then echo "stub-uuid"; exit 0; fi' \
'if [[ "$1" == "-g" ]]; then echo "--"; exit 0; fi' \
'if [[ "${NMCLI_FAIL_ADD:-}" == "yes" && "$1" == "connection" && "$2" == "add" ]]; then exit 7; fi' \
'if [[ "${NMCLI_SIGNAL_PARENT:-}" == "yes" && "$1" == "connection" && "$2" == "add" ]]; then kill -TERM "$PPID"; exit 0; fi' \
'exit 0' >"$stub_bin/nmcli"
printf '%s\n' '#!/usr/bin/env bash' 'exit 0' >"$stub_bin/pio"
printf '%s\n' '#!/usr/bin/env bash' \
'if [[ "$1" == "compose" && "$2" == "version" ]]; then exit 0; fi' \
'exit 0' >"$stub_bin/docker"
chmod 755 "$stub_bin"/*

runner_state="$tmp/runner-state"
if PATH="$stub_bin:$PATH" \
    DEVICEFRAMEWORK_PIO_EXECUTABLE="$stub_bin/pio" \
    DEVICEFRAMEWORK_PLATFORMIO_CORE_DIR="$tmp/pio-core" \
    DEVICEFRAMEWORK_HARDWARE_LOCK_FILE="$tmp/hardware.lock" \
    XDG_STATE_HOME="$runner_state" SIGNAL_LOG="$signal_log" NMCLI_SIGNAL_PARENT=yes \
    "$tool" portal --platform esp8266 --port /dev/null --client-interface wlan-client \
        --output "$tmp/runner-output" >/dev/null 2>&1; then
    echo "device-ui runner survived a connection-creation interrupt" >&2
    exit 1
fi
grep -Eq 'connection delete deviceframework-portal-' "$signal_log"
[[ ! -e "$runner_state/deviceframework-device-ui/session.env" ]] || {
    echo "device-ui runner interrupt left a portal state record behind" >&2
    exit 1
}

# A rejected `connection add` must also clear the pre-add pending record rather
# than blocking the developer's next portal command.
failed_state="$tmp/failed-state"
if PATH="$stub_bin:$PATH" \
    DEVICEFRAMEWORK_PIO_EXECUTABLE="$stub_bin/pio" \
    DEVICEFRAMEWORK_PLATFORMIO_CORE_DIR="$tmp/pio-core-failed" \
    DEVICEFRAMEWORK_HARDWARE_LOCK_FILE="$tmp/hardware.lock" \
    XDG_STATE_HOME="$failed_state" SIGNAL_LOG="$signal_log" NMCLI_FAIL_ADD=yes \
    "$tool" portal --platform esp8266 --port /dev/null --client-interface wlan-client \
        --output "$tmp/failed-output" >/dev/null 2>&1; then
    echo "device-ui runner accepted a rejected connection creation" >&2
    exit 1
fi
grep -Eq 'connection delete deviceframework-portal-' "$signal_log"
[[ ! -e "$failed_state/deviceframework-device-ui/session.env" ]] || {
    echo "device-ui rejected creation left pending recovery state behind" >&2
    exit 1
}

# The portal scan failure is explicit too: do not proceed to connection add
# merely because the helper was called from a conditional context.
wait_state="$tmp/wait-state"
if ROOT="$project_dir" XDG_STATE_HOME="$wait_state" bash -c '
    set -euo pipefail
    source "$ROOT/tools/lib/device-ui-session.sh"
    dfui_wait_for_portal_ssid() { return 1; }
    nmcli() { exit 99; }
    if dfui_create_portal_connection wlan-client "fixture portal" placeholder no esp8266; then
        exit 1
    fi
    [[ ! -e "$(dfui_state_file)" ]]
'; then
    echo "device-ui accepted a failed portal scan" >&2
    exit 1
fi

# Model an uncatchable host death after the pre-add atomic write. `down` must
# accept the name-only pending record and remove only that connection.
pending_state="$tmp/pending-state"
ROOT="$project_dir" XDG_STATE_HOME="$pending_state" bash -c '
    source "$ROOT/tools/lib/device-ui-session.sh"
    dfui_write_state wlan-client esp8266 "" deviceframework-pending-recovery
'
grep -Fxq 'DFUI_PORTAL_STATE=pending' "$pending_state/deviceframework-device-ui/session.env"
PATH="$stub_bin:$PATH" DEVICEFRAMEWORK_HARDWARE_LOCK_FILE="$tmp/hardware.lock" \
    XDG_STATE_HOME="$pending_state" SIGNAL_LOG="$signal_log" "$tool" down >/dev/null
grep -Fq 'connection delete deviceframework-pending-recovery' "$signal_log"
[[ ! -e "$pending_state/deviceframework-device-ui/session.env" ]] || {
    echo "device-ui pending recovery state was not cleared" >&2
    exit 1
}

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
rg -Fq 'DFUI_PORTAL_CONNECTION_OWNED' "$project_dir/tools/lib/device-ui-session.sh"
rg -Fq 'deviceframework-hardware-test.lock' "$project_dir/tools/lib/device-ui-session.sh"
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
