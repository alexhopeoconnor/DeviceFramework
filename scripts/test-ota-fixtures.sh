#!/usr/bin/env bash
# Build the board-free OTA fixture identities for one maintained platform.
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$project_dir"
# shellcheck source=tools/lib/platformio.sh
source "$project_dir/tools/lib/platformio.sh"
# shellcheck source=tools/lib/ota-fixture-identity.sh
source "$project_dir/tools/lib/ota-fixture-identity.sh"

# Fixture identities require two real compilations; keep a standalone run as
# civil to a developer workstation as the aggregate runner is.
export PLATFORMIO_RUN_JOBS="${PLATFORMIO_RUN_JOBS:-2}"

usage() {
    cat <<'EOF'
Usage: ./scripts/test-ota-fixtures.sh --platform esp8266|esp32

Builds each safe A/B OTA fixture pair in one PlatformIO environment. Fixture A
is copied aside before that environment is rebuilt as fixture B, so both
identities are verified without resolving the same dependency graph twice.
EOF
}

platform=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --platform) [[ $# -ge 2 ]] || { usage >&2; exit 2; }; platform="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) usage >&2; exit 2 ;;
    esac
done
[[ "$platform" == esp8266 || "$platform" == esp32 ]] || { usage >&2; exit 2; }
command -v flock >/dev/null 2>&1 || { echo "flock is required." >&2; exit 1; }

fixture_dir="$(mktemp -d "${TMPDIR:-/tmp}/deviceframework-ota-fixtures.XXXXXX")"
chmod 700 "$fixture_dir"
export DEVICEFRAMEWORK_OTA_IDENTITY_DIR="$fixture_dir/identity"
cleanup() {
    trap - EXIT HUP INT TERM
    df_remove_ota_fixture_identity
    rm -rf -- "$fixture_dir"
}
on_signal() {
    local status="$1"
    # A signal must stop this runner after cleanup.  Merely removing the
    # temporary input and returning would let the shell start a later fixture
    # with a newly-created identity directory.
    trap - HUP INT TERM
    cleanup
    exit "$status"
}
trap cleanup EXIT
trap 'on_signal 129' HUP
trap 'on_signal 130' INT
trap 'on_signal 143' TERM

fixture_build_dir="$fixture_dir/build"

esp32_ota_slot_bytes() {
    local partition="$1" subtype="$2" value
    value="$(awk -F, -v partition="$partition" -v subtype="$subtype" '
        function trim(value) { gsub(/^[[:space:]]+|[[:space:]]+$/, "", value); return value }
        /^[[:space:]]*#/ || NF < 5 { next }
        trim($1) == partition && trim($2) == "app" && trim($3) == subtype { print trim($5); exit }
    ' "$project_dir/partitions/esp32_ota_4m_no_fs.csv")"
    [[ "$value" =~ ^0x[0-9A-Fa-f]+$ ]] || {
        echo "ESP32 OTA partition table has no valid $partition slot." >&2
        return 1
    }
    printf '%d\n' "$((value))"
}

assert_ota_image_fits() {
    local image="$1" capacity="$2" label="$3" size
    size="$(wc -c < "$image" | tr -d '[:space:]')"
    [[ "$size" =~ ^[0-9]+$ && "$size" -gt 0 && "$size" -le "$capacity" ]] || {
        echo "ESP32 OTA fixture $label exceeds its CSV application slot." >&2
        return 1
    }
}

build_pair() {
    local environment="$1" transport="$2" profile="$3" label="$4" expected_portal_protected="${5:-}"
    local build_dir a_source b_source a_artifact b_artifact
    local version_prefix="0.0.0-${transport}-ota"

    df_lock_ota_fixture_environment "$environment"
    printf '[OTA fixture] %s (%s) A\n' "$environment" "$label"
    df_write_ota_fixture_identity A "${version_prefix}-a" "$expected_portal_protected"
    DEVICEFRAMEWORK_OTA_PROFILE="$profile" PLATFORMIO_BUILD_DIR="$fixture_build_dir" \
        df_pio run -d test/ota-harness -e "$environment"
    a_source="$fixture_build_dir/$environment/firmware.bin"
    [[ -s "$a_source" ]] || { echo "OTA fixture A was not produced for $label." >&2; return 1; }
    a_artifact="$fixture_dir/${environment}-${transport}-a.bin"
    install -m 600 "$a_source" "$a_artifact"

    # This header is included only by fixture main.cpp. Rebuild B in the same
    # environment without cleaning libdeps or unrelated libraries.
    df_write_ota_fixture_identity B "${version_prefix}-b" "$expected_portal_protected"
    printf '[OTA fixture] %s (%s) B\n' "$environment" "$label"
    DEVICEFRAMEWORK_OTA_PROFILE="$profile" PLATFORMIO_BUILD_DIR="$fixture_build_dir" \
        df_pio run -d test/ota-harness -e "$environment"
    b_source="$fixture_build_dir/$environment/firmware.bin"
    [[ -s "$b_source" ]] || { echo "OTA fixture B was not produced for $label." >&2; return 1; }
    b_artifact="$fixture_dir/${environment}-${transport}-b.bin"
    install -m 600 "$b_source" "$b_artifact"

    if cmp -s "$a_artifact" "$b_artifact"; then
        echo "OTA fixture A and B are unexpectedly identical for $label." >&2
        return 1
    fi
    if [[ "$platform" == esp32 ]]; then
        assert_ota_image_fits "$a_artifact" "$(esp32_ota_slot_bytes app0 ota_0)" "A ($label)"
        assert_ota_image_fits "$b_artifact" "$(esp32_ota_slot_bytes app1 ota_1)" "B ($label)"
    fi
    printf 'OTA fixture A/B test harness passed for %s\n' "$label"
}

for profile in protected open; do
    case "$profile" in
        protected) expected_portal_protected=1 ;;
        open) expected_portal_protected=0 ;;
    esac
    build_pair "${platform}_portal_ota" portal "../profiles/ota-portal-${profile}.json" \
        "portal ${platform}/${profile}" "$expected_portal_protected"
done
for profile in protected open; do
    build_pair "${platform}_udp_ota" udp "../profiles/ota-lan-${profile}-fixture.json" "UDP ${platform}/${profile}"
done
if [[ "$platform" == esp8266 ]]; then
    build_pair esp8266_udp_ota_deferred udp ../profiles/ota-lan-protected-fixture.json 'UDP ESP8266/deferred'
fi

echo "DeviceFramework board-free OTA fixture builds passed for $platform"
