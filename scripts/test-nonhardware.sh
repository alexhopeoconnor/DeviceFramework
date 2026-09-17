#!/usr/bin/env bash
# Run DeviceFramework's board-free release gate as one interruptible command.
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$project_dir"
# shellcheck source=tools/lib/platformio.sh
source "$project_dir/tools/lib/platformio.sh"

# One complete integration run must not start every SCons build with the host's
# CPU count. A developer can opt in to more parallelism explicitly.
export PLATFORMIO_RUN_JOBS="${PLATFORMIO_RUN_JOBS:-2}"

usage() {
    cat <<'EOF'
Usage: ./scripts/test-nonhardware.sh

Runs the complete board-free DeviceFramework test suite.
EOF
}

case "${1:-}" in
    "") ;;
    -h|--help) usage; exit 0 ;;
    *) usage >&2; exit 2 ;;
esac

on_interrupt() {
    trap - INT TERM
    printf '\nDeviceFramework non-hardware test suite interrupted; no later test will start.\n' >&2
    exit 130
}
trap on_interrupt INT TERM

run_test() {
    DEVICEFRAMEWORK_SKIP_WEB_ASSET_CHECK=1 "$project_dir/scripts/test.sh" "$@"
}

run_ota_fixture_build() {
    local environment="$1"
    printf '[OTA fixture] %s\n' "$environment"
    df_pio run -d test/ota-harness -e "$environment"
}

run_udp_ota_fixture_build() {
    local environment="$1" profile="$2"
    printf '[UDP OTA fixture] %s (%s profile)\n' "$environment" "$profile"
    DEVICEFRAMEWORK_OTA_PROFILE="$project_dir/test/profiles/ota-lan-${profile}-fixture.json" \
        df_pio run -d test/ota-harness -e "$environment"
}

assert_ota_fixture_pair() {
    local platform="$1" environment_prefix="$2" label="$3" a b slot_a slot_b
    a="test/ota-harness/.pio/build/${environment_prefix}_a/firmware.bin"
    b="test/ota-harness/.pio/build/${environment_prefix}_b/firmware.bin"
    [[ -s "$a" && -s "$b" ]] || {
        echo "OTA fixture pair did not produce both images for $label" >&2
        return 1
    }
    if cmp -s "$a" "$b"; then
        echo "OTA fixture A and B are unexpectedly identical for $label" >&2
        return 1
    fi
    if [[ "$platform" == "esp32" ]]; then
        slot_a="$(esp32_ota_slot_bytes app0 ota_0)"
        slot_b="$(esp32_ota_slot_bytes app1 ota_1)"
        assert_ota_image_fits "$a" "$slot_a" "A"
        assert_ota_image_fits "$b" "$slot_b" "B"
    fi
    echo "OTA fixture A/B test harness passed for $label"
}

esp32_ota_slot_bytes() {
    local partition="$1" subtype="$2" value
    value="$(awk -F, -v partition="$partition" -v subtype="$subtype" '
        function trim(value) { gsub(/^[[:space:]]+|[[:space:]]+$/, "", value); return value }
        /^[[:space:]]*#/ || NF < 5 { next }
        trim($1) == partition && trim($2) == "app" && trim($3) == subtype { print trim($5); exit }
    ' partitions/esp32_ota_4m_no_fs.csv)"
    [[ "$value" =~ ^0x[0-9A-Fa-f]+$ ]] || {
        echo "ESP32 OTA partition table has no valid $partition slot." >&2
        return 1
    }
    printf '%d\n' "$((value))"
}

assert_ota_image_fits() {
    local image="$1" capacity="$2" label="$3" size
    size="$(wc -c < "$image" | tr -d '[:space:]')"
    [[ "$size" =~ ^[0-9]+$ && "$size" -le "$capacity" ]] || {
        echo "ESP32 OTA fixture $label exceeds its CSV application slot." >&2
        return 1
    }
}

"$project_dir/tools/check-web-assets.sh"
"$project_dir/tools/check-ota-partitions.sh"
run_test compile --platform esp8266
run_test compile --platform esp8266 --profile-fixture
run_test compile --platform esp32
run_test compile --platform esp32 --profile-fixture
for platform in esp8266 esp32; do
    for profile in protected open; do
        for image in a b; do
            run_ota_fixture_build "${platform}_portal_ota_${profile}_${image}"
        done
        assert_ota_fixture_pair "$platform" "${platform}_portal_ota_${profile}" "portal ${platform}/${profile}"
    done
    for profile in protected open; do
        for image in a b; do
            run_udp_ota_fixture_build "${platform}_udp_ota_${image}" "$profile"
        done
        assert_ota_fixture_pair "$platform" "${platform}_udp_ota" "UDP ${platform}/${profile}"
    done
done
for image in a b; do
    run_udp_ota_fixture_build "esp8266_udp_ota_deferred_${image}" protected
done
assert_ota_fixture_pair esp8266 esp8266_udp_ota_deferred "UDP ESP8266/deferred"
run_test examples --platform esp8266
run_test examples --platform esp32
DEVICEFRAMEWORK_SKIP_WEB_ASSET_CHECK=1 "$project_dir/scripts/check-docs.sh"
"$project_dir/tools/ha-hardware" fixture --ui-capture

echo "DeviceFramework non-hardware test suite passed"
