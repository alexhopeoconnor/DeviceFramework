#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$project_dir"
# shellcheck source=tools/lib/platformio.sh
source "$project_dir/tools/lib/platformio.sh"
usage() {
    cat <<'EOF'
Usage:
  ./scripts/test.sh compile  --platform esp8266|esp32 [--profile-fixture] [--release]
  ./scripts/test.sh examples --platform esp8266|esp32
  ./scripts/test.sh packages --platform esp8266|esp32
  ./scripts/test.sh hardware --platform esp8266|esp32 --port /dev/ttyUSB0 [--env-file test/.env] [--profile-fixture] [--ha-e2e] [--config-header PATH]
EOF
    exit 2
}

mode="${1:-}"
[[ "$mode" == "compile" || "$mode" == "examples" || "$mode" == "packages" || "$mode" == "hardware" ]] || usage
shift
platform=""
profile_fixture=false
ha_e2e=false
port=""
env_file="test/.env"
config_header=""
release=false
while [[ $# -gt 0 ]]; do
    case "$1" in
        --platform) [[ $# -ge 2 ]] || usage; platform="${2:-}"; shift 2 ;;
        --profile-fixture) profile_fixture=true; shift ;;
        --ha-e2e) ha_e2e=true; shift ;;
        --port) [[ $# -ge 2 ]] || usage; port="${2:-}"; shift 2 ;;
        --env-file) [[ $# -ge 2 ]] || usage; env_file="${2:-}"; shift 2 ;;
        --config-header) [[ $# -ge 2 ]] || usage; config_header="${2:-}"; shift 2 ;;
        --release) release=true; shift ;;
        *) usage ;;
    esac
done
[[ "$platform" == "esp8266" || "$platform" == "esp32" ]] || usage
[[ "$mode" != "hardware" || -n "$port" ]] || usage
[[ "$mode" != "hardware" || "$profile_fixture" == "false" || -f "$env_file" ]] || usage
[[ "$ha_e2e" == "false" || "$mode" == "hardware" ]] || { echo "--ha-e2e is only valid with hardware mode" >&2; exit 2; }
[[ "$release" == "false" || "$mode" == "compile" ]] || { echo "--release is only valid with compile mode" >&2; exit 2; }
[[ "$ha_e2e" == "false" || "$profile_fixture" == "false" ]] || { echo "--ha-e2e cannot be combined with --profile-fixture" >&2; exit 2; }
[[ -z "$config_header" || "$ha_e2e" == "true" ]] || { echo "--config-header requires --ha-e2e" >&2; exit 2; }
[[ -z "$config_header" || -f "$config_header" ]] || { echo "Missing private HA E2E configuration header: $config_header" >&2; exit 1; }

on_interrupt() {
    local signal="$1"
    trap - INT TERM
    printf '\nDeviceFramework %s test interrupted by %s; exiting without starting another test.\n' "$mode" "$signal" >&2
    exit 130
}
trap 'on_interrupt INT' INT
trap 'on_interrupt TERM' TERM

# The aggregate runner verifies generated web assets once before it invokes
# individual checks. Direct calls retain the normal safety check.
if [[ "${DEVICEFRAMEWORK_SKIP_WEB_ASSET_CHECK:-0}" != "1" ]]; then
    "$project_dir/tools/check-web-assets.sh"
fi

# Test modes share PlatformIO's package manager and build output; hardware also
# shares a generated credential header. Serialize them in this checkout so
# package updates and temporary configuration cannot race an active test.
hardware_lock_file="${TMPDIR:-/tmp}/deviceframework-hardware-test.lock"
exec {hardware_lock_fd}>"$hardware_lock_file"
if ! flock -n "$hardware_lock_fd"; then
    echo "Another DeviceFramework test is active; waiting for its PlatformIO build state. Press Ctrl-C to cancel safely." >&2
    flock "$hardware_lock_fd"
fi

if [[ "$mode" == "examples" ]]; then
    mapfile -t examples < <(find examples -mindepth 1 -maxdepth 1 -type d -name '[0-9][0-9]-*' -print | sort)
    if (( ${#examples[@]} == 0 )); then
        echo "No example projects found" >&2
        exit 1
    fi
    for example in "${examples[@]}"; do
        df_pio run -d "$example" -e "$platform" </dev/null
    done
    echo "DeviceFramework examples compile check passed for $platform"
    exit 0
fi

if [[ "$mode" == "packages" ]]; then
    # Print the graph selected by the consuming fixture, not stale metadata
    # from an unrelated global PlatformIO package directory.
    df_pio pkg list -d test/compile-project -e "$platform"
    exit 0
fi

environment="$platform"
consumer_config_args=()
if [[ "$release" == "true" ]]; then
    # PlatformIO resolves --project-conf before it applies -d, so this must
    # be an absolute path rather than a name relative to the consumer project.
    consumer_config_args=(-c "$project_dir/test/compile-project/platformio.release.ini")
fi
refresh_clean_consumer_dependency() {
    local target_environment="$1"
    local dependency cached_library
    # The consumer fixture is used both for release-tag verification and
    # ignored sibling-worktree development. Remove all direct first-party
    # packages so PlatformIO resolves the config selected for this invocation;
    # otherwise a cached WiFiManager/DFTE/ArduinoHA tag can mask a local change.
    for dependency in DeviceFramework WiFiManager DeviceFrameworkTemplateEngine home-assistant-integration; do
        cached_library="test/compile-project/.pio/libdeps/${target_environment}/${dependency}"
        [[ -d "$cached_library" || -e "${cached_library}.pio-link" ]] || continue
        # `pio pkg uninstall` has no --project-conf option.  It only removes
        # the named cached package; the following `pio run` is what resolves
        # the selected release or local project configuration afresh.
        df_pio pkg uninstall -d test/compile-project -e "$target_environment" \
            -l "$dependency" --no-save --skip-dependencies >/dev/null
    done
}

[[ "$profile_fixture" == "true" ]] && environment="${platform}_profile"
if [[ "$mode" == "hardware" && "$profile_fixture" == "true" ]]; then
    environment="${platform}_profile_hardware"
fi
if [[ "$mode" == "hardware" && "$ha_e2e" == "true" ]]; then
    environment="${platform}_ha_e2e"
fi
if [[ "$mode" == "compile" ]]; then
    consumer_environments=("$environment")
    if [[ "$profile_fixture" == "true" ]]; then
        consumer_environments+=("${platform}_profile_no_wifi" "${platform}_profile_reconcile")
    else
        consumer_environments+=("${platform}_default_ui")
    fi

    for consumer_environment in "${consumer_environments[@]}"; do
        df_pio run -d test/compile-project "${consumer_config_args[@]}" -e "$consumer_environment" -t clean >/dev/null
        refresh_clean_consumer_dependency "$consumer_environment"
        df_pio run -d test/compile-project "${consumer_config_args[@]}" -e "$consumer_environment"
    done
    if [[ "$profile_fixture" == "false" && "$platform" == "esp8266" ]]; then
        # Prove that callers can omit the optional local web interface.
        refresh_clean_consumer_dependency esp8266_no_web
        df_pio run -d test/compile-project "${consumer_config_args[@]}" -e esp8266_no_web
    fi
    exit 0
fi
[[ -f "$env_file" ]] || { echo "Missing $env_file; copy test/.env.example first." >&2; exit 1; }
set -a
# shellcheck disable=SC1090
source "$env_file"
set +a
for key in DEVICEFRAMEWORK_TEST_WIFI_SSID DEVICEFRAMEWORK_TEST_WIFI_PASSWORD DEVICEFRAMEWORK_TEST_MQTT_SERVER; do
    [[ -n "${!key:-}" ]] || { echo "$key is required in $env_file" >&2; exit 1; }
done

config_file="test/test_device_framework/test_config.generated.h"
test_filter="test_device_framework"
if [[ "$ha_e2e" == "true" ]]; then
    config_file="test/test_ha_e2e/test_config.generated.h"
    test_filter="test_ha_e2e"
fi

escape_c_string() { printf '%s' "$1" | sed 's/\\/\\\\/g; s/"/\\"/g'; }
write_config() {
    [[ -z "$config_header" ]] || return 0
    printf '%s\n' '#pragma once' > "$config_file"
    printf '#define TEST_WIFI_SSID "%s"\n' "$(escape_c_string "$DEVICEFRAMEWORK_TEST_WIFI_SSID")" >> "$config_file"
    printf '#define TEST_WIFI_PASSWORD "%s"\n' "$(escape_c_string "$DEVICEFRAMEWORK_TEST_WIFI_PASSWORD")" >> "$config_file"
    printf '#define TEST_MQTT_SERVER "%s"\n' "$(escape_c_string "$DEVICEFRAMEWORK_TEST_MQTT_SERVER")" >> "$config_file"
    printf '#define TEST_MQTT_USER "%s"\n' "$(escape_c_string "${DEVICEFRAMEWORK_TEST_MQTT_USER:-}")" >> "$config_file"
    printf '#define TEST_MQTT_PASSWORD "%s"\n' "$(escape_c_string "${DEVICEFRAMEWORK_TEST_MQTT_PASSWORD:-}")" >> "$config_file"
    if [[ "$profile_fixture" == "true" ]]; then
        printf "%s\n" "#define TEST_EXPECT_WIFI_FALLBACK 1" >> "$config_file"
    fi
}

hardware_profile=""
hardware_smoke_profile=""
ha_e2e_device_id=""
ha_e2e_device_ip=""
write_profile() {
    local target="$1"
    local profile_id="$2"
    local policy="$3"
    local password="$4"
    local managed_device_name="${5:-}"

    {
        printf "%s\n" "{"
        printf "%s\n" "  \"format\": 2,"
        printf "%s\n" "  \"application\": \"deviceframework\","
        printf "  \"profile\": { \"id\": \"%s\", \"revision\": 1, \"policy\": \"%s\" },\n" "$profile_id" "$policy"
        printf "  \"device_password\": \"%s\",\n" "$(escape_c_string "$password")"
        printf "%s\n" "  \"wifi\": { \"profiles\": ["
        printf "%s\n" "    { \"ssid\": \"df-test-primary-unavailable\", \"password\": \"\" },"
        printf "    { \"ssid\": \"%s\", \"password\": \"%s\" }\n" "$(escape_c_string "$DEVICEFRAMEWORK_TEST_WIFI_SSID")" "$(escape_c_string "$DEVICEFRAMEWORK_TEST_WIFI_PASSWORD")"
        if [[ -n "$managed_device_name" ]]; then
            printf "%s\n" "  ] },"
            printf "  \"parameters\": { \"device\": \"%s\", \"loglevel\": \"Info\" }\n" \
                "$(escape_c_string "$managed_device_name")"
        else
            printf "%s\n" "  ] }"
        fi
        printf "%s\n" "}"
    } > "$target"
}

write_hardware_profile() {
    hardware_profile="$(mktemp -p /tmp deviceframework-profile.XXXXXX)"
    hardware_smoke_profile="$(mktemp -p /tmp deviceframework-smoke-profile.XXXXXX)"
    chmod 600 "$hardware_profile" "$hardware_smoke_profile"
    write_profile "$hardware_profile" "hardware-${platform}-bootstrap" "bootstrap" "default1"
    write_profile "$hardware_smoke_profile" "hardware-${platform}-smoke" "reconcile" "profile-reconcile-password" "Hardware Reconciled"
}


run_unity_hardware_test() {
    local output_file
    output_file="$(mktemp -p /tmp deviceframework-unity.XXXXXX)"

    if [[ "$profile_fixture" == "true" ]]; then
        if ! DEVICEFRAMEWORK_HARDWARE_PROFILE="$hardware_profile" df_pio test -e "$environment" --filter "$test_filter" --upload-port "$port" --without-testing >"$output_file" 2>&1; then
            cat "$output_file"
            rm -f "$output_file"
            return 1
        fi
    elif ! df_pio test -e "$environment" --filter "$test_filter" --upload-port "$port" --without-testing >"$output_file" 2>&1; then
        cat "$output_file"
        rm -f "$output_file"
        return 1
    fi

    if ! python3 tools/capture-unity-serial.py --port "$port" --output "$output_file"; then
        cat "$output_file"
        rm -f "$output_file"
        return 1
    fi
    if [[ "$ha_e2e" == "true" ]]; then
        ha_e2e_device_id="$(sed -nE 's/^HA_E2E_DEVICE_ID=([^[:space:]]+)$/\1/p' "$output_file" | tail -n 1)"
        ha_e2e_device_ip="$(sed -nE 's/^HA_E2E_DEVICE_IP=([^[:space:]]+)$/\1/p' "$output_file" | tail -n 1)"
        if [[ -z "$ha_e2e_device_id" || -z "$ha_e2e_device_ip" ]]; then
            echo "HA E2E firmware did not emit its device ID and IPv4 address" >&2
            cat "$output_file"
            rm -f "$output_file"
            return 1
        fi
    fi
    cat "$output_file"
    rm -f "$output_file"
}

assert_http_endpoint() {
    local description="$1"
    local endpoint="$2"
    local expected_status="$3"
    local password="$4"
    shift 4

    local response_file
    response_file="$(mktemp -p /tmp deviceframework-http.XXXXXX)"
    local curl_args=(
        --silent --show-error --noproxy '*'
        --connect-timeout 3 --max-time 20
        --retry 3 --retry-all-errors
        --output "$response_file" --write-out '%{http_code}'
    )
    if [[ -n "$password" ]]; then
        curl_args+=(-u "admin:$password")
    fi

    local status_code
    if ! status_code="$(curl "${curl_args[@]}" "http://${device_host}${endpoint}")"; then
        rm -f "$response_file"
        echo "Direct LAN check failed to fetch $description" >&2
        return 1
    fi
    if [[ "$status_code" != "$expected_status" ]]; then
        rm -f "$response_file"
        echo "Direct LAN check for $description expected HTTP $expected_status, got $status_code" >&2
        return 1
    fi
    local marker
    for marker in "$@"; do
        if ! rg -Fq -- "$marker" "$response_file"; then
            rm -f "$response_file"
            echo "Direct LAN check for $description is missing expected content: $marker" >&2
            return 1
        fi
    done
    rm -f "$response_file"
    echo "Direct LAN check passed: $description"
}

assert_password_endpoint() {
    local password="$1"
    local response_file
    response_file="$(mktemp -p /tmp deviceframework-password.XXXXXX)"
    local status_code
    if ! status_code="$(curl --silent --show-error --noproxy '*' --connect-timeout 3 --max-time 20 \
        --output "$response_file" --write-out '%{http_code}' \
        -u "admin:$password" -X POST \
        --data-urlencode "new_password=$password" \
        --data-urlencode "confirm_password=$password" \
        "http://${device_host}/api/device-password")"; then
        rm -f "$response_file"
        echo "Direct LAN check failed to update the device password" >&2
        return 1
    fi
    if [[ "$status_code" != "200" ]] || ! rg -Fq '"status":"success"' "$response_file"; then
        rm -f "$response_file"
        echo "Device password endpoint did not accept the profiled password (HTTP $status_code)" >&2
        return 1
    fi
    rm -f "$response_file"
    echo "Direct LAN check passed: persistent device-password endpoint"
}

wait_for_password_restart() {
    local password="$1"
    local status_code=""
    local attempt
    local stable_successes=0

    # A successful password update deliberately schedules a reboot. Observe
    # both the outage and recovered authentication rather than racing it.
    for attempt in {1..15}; do
        status_code="$(curl --silent --noproxy '*' --connect-timeout 1 --max-time 2 --output /dev/null \
            --write-out '%{http_code}' -u "admin:$password" \
            "http://${device_host}/api/status" 2>/dev/null || true)"
        [[ "$status_code" != "200" ]] && break
        sleep 1
    done
    if [[ "$status_code" == "200" ]]; then
        echo "Password endpoint did not trigger the expected restart" >&2
        return 1
    fi

    for attempt in {1..45}; do
        status_code="$(curl --silent --noproxy '*' --connect-timeout 1 --max-time 2 --output /dev/null \
            --write-out '%{http_code}' -u "admin:$password" \
            "http://${device_host}/api/status" 2>/dev/null || true)"
        if [[ "$status_code" == "200" ]]; then
            stable_successes=$((stable_successes + 1))
            if [[ "$stable_successes" -ge 2 ]]; then
                echo "Direct LAN check passed: password persists after restart"
                return 0
            fi
        else
            stable_successes=0
        fi
        sleep 1
    done

    echo "Device did not return with its updated password after restart" >&2
    return 1
}

verify_web_interface() {
    local default_host
    if [[ "$profile_fixture" == "true" ]]; then
        default_host="hardware-reconciled.local"
    else
        default_host="${platform}-controller.local"
    fi
    # Normal hardware coverage is specifically an mDNS contract.  A private
    # IP override used to turn an mDNS regression into a false green result;
    # keep direct-IP access in the explicit OTA diagnostic command instead.
    [[ -z "${DEVICEFRAMEWORK_TEST_DEVICE_HOST:-}" ]] || {
        echo "DEVICEFRAMEWORK_TEST_DEVICE_HOST is no longer accepted by the normal hardware suite; it must prove ${default_host} through mDNS." >&2
        return 1
    }
    command -v avahi-resolve >/dev/null || {
        echo "avahi-resolve is required because the normal hardware suite verifies mDNS." >&2
        return 1
    }
    command -v getent >/dev/null || {
        echo "getent is required because the normal hardware suite verifies the system mDNS resolver." >&2
        return 1
    }
    local mdns_name="$default_host"
    local attempt avahi_ip system_ip
    device_host=""
    for attempt in {1..45}; do
        avahi_ip="$(avahi-resolve -4 -n "$mdns_name" 2>/dev/null | awk 'NR == 1 { print $2; exit }')"
        system_ip="$(getent ahostsv4 "$mdns_name" 2>/dev/null | awk 'NR == 1 { print $1; exit }')"
        if [[ -n "$avahi_ip" && "$avahi_ip" == "$system_ip" ]]; then
            # Keep the hostname in the actual HTTP URL. Resolution and
            # transport are both part of this normal mDNS contract.
            device_host="$mdns_name"
            echo "mDNS: $mdns_name -> $avahi_ip (Avahi and system resolver agree)"
            break
        fi
        sleep 1
    done
    [[ -n "$device_host" ]] || {
        echo "Could not resolve the test device mDNS name through both Avahi and the system resolver within 45 seconds: $mdns_name" >&2
        return 1
    }
    # An erased, unprofiled DeviceFramework board deliberately retains the
    # library's safe test default.  The normal hardware path must prove that
    # protected routes reject anonymous requests and then authenticate with
    # that known value; otherwise the positive checks below would incorrectly
    # expect a protected endpoint to be public.
    local profile_password="default1"
    if [[ "$profile_fixture" == "true" ]]; then
        local profile_source="${hardware_smoke_profile:-test/profiles/profile-fixture.json}"
        profile_password="$(sed -nE 's/^[[:space:]]*"device_password"[[:space:]]*:[[:space:]]*"([^"]*)"[[:space:]]*,?[[:space:]]*$/\1/p' "$profile_source")"
        [[ -n "$profile_password" ]] || { echo "Profile fixture has no device_password" >&2; return 1; }
        assert_http_endpoint "superseded bootstrap password" "/api/status" 401 "default1"
        assert_http_endpoint "unauthenticated API status" "/api/status" 401 ""
        assert_http_endpoint "unauthenticated stylesheet" "/assets/deviceframework.css" 401 ""
        assert_http_endpoint "unauthenticated logo" "/assets/deviceframework-logo" 401 ""
    else
        assert_http_endpoint "unauthenticated API status" "/api/status" 401 ""
        assert_http_endpoint "unauthenticated stylesheet" "/assets/deviceframework.css" 401 ""
        assert_http_endpoint "unauthenticated logo" "/assets/deviceframework-logo" 401 ""
    fi

    if [[ "$profile_fixture" == "true" ]]; then
        assert_http_endpoint "reconciled API status" "/api/status" 200 "$profile_password" \
            runtime chip_id version "Hardware Reconciled" "$DEVICEFRAMEWORK_TEST_MQTT_SERVER"
    else
        assert_http_endpoint "API status" "/api/status" 200 "$profile_password" runtime chip_id version
    fi
    for page_pass in 1 2; do
        assert_http_endpoint "web interface status page (pass $page_pass)" "/" 200 "$profile_password" "<!DOCTYPE html>" "Device Status" "data-df-page=\"status\"" "deviceframework.css" "deviceframework.js" "DeviceFramework UI Test" "Test Lab" "df-web-theme" "--df-accent:#15803d" "</html>"
        assert_http_endpoint "web interface serial page (pass $page_pass)" "/serial" 200 "$profile_password" "Serial Monitor" "data-df-page=\"serial\"" "serial-monitor" "</html>"
        assert_http_endpoint "web interface controls page (pass $page_pass)" "/controls" 200 "$profile_password" "System Controls" "data-df-page=\"controls\"" "device-password-form" "</html>"
        assert_http_endpoint "web interface about page (pass $page_pass)" "/about" 200 "$profile_password" "About" "data-df-page=\"about\"" "https://example.test" "</html>"
        if [[ "$page_pass" == "1" ]]; then
            assert_http_endpoint "web interface stylesheet" "/assets/deviceframework.css" 200 "$profile_password" "Modern CSS Reset" ".page-loader"
            assert_http_endpoint "web interface script" "/assets/deviceframework.js" 200 "$profile_password" "function refreshStatus" "initializeWebSerial"
            assert_http_endpoint "web interface logo" "/assets/deviceframework-logo" 200 "$profile_password" "<svg"
        fi
        assert_http_endpoint "custom 404 page (pass $page_pass)" "/notfound" 200 "$profile_password" 404 "Page Not Found" "Return to Home" "</html>"
    done
    assert_http_endpoint "post-page API status" "/api/status" 200 "$profile_password" runtime chip_id version
    if [[ "$profile_fixture" == "true" ]]; then
        assert_password_endpoint "$profile_password"
        wait_for_password_restart "$profile_password"
        assert_http_endpoint "post-restart API status" "/api/status" 200 "$profile_password" runtime chip_id version
    fi
}
cleanup() {
    if [[ -z "$config_header" ]]; then rm -f "$config_file"; fi
    [[ -z "$hardware_profile" ]] || rm -f "$hardware_profile"
    [[ -z "$hardware_smoke_profile" ]] || rm -f "$hardware_smoke_profile"
}
trap cleanup EXIT
write_config
if [[ "$profile_fixture" == "true" ]]; then
    write_hardware_profile
    run_unity_hardware_test
    DEVICEFRAMEWORK_HARDWARE_PROFILE="$hardware_smoke_profile" df_pio run -d test/compile-project -e "$environment" -t upload --upload-port "$port"
else
    run_unity_hardware_test
fi
if [[ "$ha_e2e" == "false" ]]; then
    verify_web_interface
fi
if [[ "$ha_e2e" == "true" ]]; then
    printf 'HA_E2E_DEVICE_ID=%s\n' "$ha_e2e_device_id"
    printf 'HA_E2E_DEVICE_IP=%s\n' "$ha_e2e_device_ip"
fi
