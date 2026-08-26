#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage:
  ./scripts/run-tests.sh compile  --platform esp8266|esp32 [--profile-fixture]
  ./scripts/run-tests.sh hardware --platform esp8266|esp32 --port /dev/ttyUSB0 [--env-file test/.env] [--profile-fixture]
EOF
    exit 2
}

mode="${1:-}"
[[ "$mode" == "compile" || "$mode" == "hardware" ]] || usage
shift
platform=""
profile_fixture=false
port=""
env_file="test/.env"
while [[ $# -gt 0 ]]; do
    case "$1" in
        --platform) platform="${2:-}"; shift 2 ;;
        --profile-fixture) profile_fixture=true; shift ;;
        --port) port="${2:-}"; shift 2 ;;
        --env-file) env_file="${2:-}"; shift 2 ;;
        *) usage ;;
    esac
done
[[ "$platform" == "esp8266" || "$platform" == "esp32" ]] || usage
[[ "$mode" != "hardware" || -n "$port" ]] || usage
[[ "$mode" != "hardware" || "$profile_fixture" == "false" || -f "$env_file" ]] || usage

environment="$platform"
refresh_clean_consumer_dependency() {
    local target_environment="$1"
    local cached_library="test/compile-project/.pio/libdeps/${target_environment}/DeviceFramework"
    [[ -d "$cached_library" || -e "${cached_library}.pio-link" ]] || return 0
    pio pkg uninstall -d test/compile-project -e "$target_environment" \
        -l DeviceFramework --no-save --skip-dependencies >/dev/null
}

[[ "$profile_fixture" == "true" ]] && environment="${platform}_profile"
if [[ "$mode" == "compile" ]]; then
    refresh_clean_consumer_dependency "$environment"
    pio run -d test/compile-project -e "$environment"
    if [[ "$profile_fixture" == "false" && "$platform" == "esp8266" ]]; then
        # Prove that callers can omit the optional local web interface.
        refresh_clean_consumer_dependency esp8266_no_web
        pio run -d test/compile-project -e esp8266_no_web
    fi
    exit 0
fi
[[ -f "$env_file" ]] || { echo "Missing $env_file; copy test/.env.example first." >&2; exit 1; }
set -a
# shellcheck disable=SC1090
source "$env_file"
set +a
for key in DEVICEFRAMEWORK_TEST_WIFI_SSID DEVICEFRAMEWORK_TEST_WIFI_PASSWORD DEVICEFRAMEWORK_TEST_MQTT_SERVER DEVICEFRAMEWORK_TEST_MQTT_USER DEVICEFRAMEWORK_TEST_MQTT_PASSWORD; do
    [[ -n "${!key:-}" ]] || { echo "$key is required in $env_file" >&2; exit 1; }
done

config_file="test/test_device_framework/test_config.generated.h"

escape_c_string() { printf '%s' "$1" | sed 's/\\/\\\\/g; s/"/\\"/g'; }
write_config() {
    printf '%s\n' '#pragma once' > "$config_file"
    printf '#define TEST_WIFI_SSID "%s"\n' "$(escape_c_string "$DEVICEFRAMEWORK_TEST_WIFI_SSID")" >> "$config_file"
    printf '#define TEST_WIFI_PASSWORD "%s"\n' "$(escape_c_string "$DEVICEFRAMEWORK_TEST_WIFI_PASSWORD")" >> "$config_file"
    printf '#define TEST_MQTT_SERVER "%s"\n' "$(escape_c_string "$DEVICEFRAMEWORK_TEST_MQTT_SERVER")" >> "$config_file"
    printf '#define TEST_MQTT_USER "%s"\n' "$(escape_c_string "$DEVICEFRAMEWORK_TEST_MQTT_USER")" >> "$config_file"
    printf '#define TEST_MQTT_PASSWORD "%s"\n' "$(escape_c_string "$DEVICEFRAMEWORK_TEST_MQTT_PASSWORD")" >> "$config_file"
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
        --silent --show-error
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
    if ! status_code="$(curl --silent --show-error --connect-timeout 3 --max-time 20 \
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

verify_web_interface() {
    local default_host
    if [[ "$profile_fixture" == "true" ]]; then
        default_host="df-test-${platform}.local"
    else
        default_host="${platform}-controller.local"
    fi
    device_host="${DEVICEFRAMEWORK_TEST_DEVICE_HOST:-$default_host}"
    if [[ "$device_host" == *.local ]]; then
        command -v avahi-resolve >/dev/null || {
            echo "avahi-resolve is required for automatic .local discovery; set DEVICEFRAMEWORK_TEST_DEVICE_HOST to an IP address instead" >&2
            return 1
        }
        device_host="$(avahi-resolve -4 -n "$device_host" | awk 'NR == 1 { print $2; exit }')"
        [[ -n "$device_host" ]] || {
            echo "Could not resolve the test device mDNS name; set DEVICEFRAMEWORK_TEST_DEVICE_HOST to an IP address instead" >&2
            return 1
        }
    fi
    local profile_password=""
    if [[ "$profile_fixture" == "true" ]]; then
        profile_password="$(sed -nE 's/^[[:space:]]*"device_password"[[:space:]]*:[[:space:]]*"([^"]*)"[[:space:]]*,?[[:space:]]*$/\1/p' test/profiles/profile-fixture.json)"
        [[ -n "$profile_password" ]] || { echo "Profile fixture has no device_password" >&2; return 1; }
        assert_http_endpoint "unauthenticated API status" "/api/status" 401 ""
        assert_password_endpoint "$profile_password"
    fi

    assert_http_endpoint "API status" "/api/status" 200 "$profile_password" runtime chip_id version
    assert_http_endpoint "web interface root" "/" 200 "$profile_password" '<!DOCTYPE html>' 'Device Status' 'System Controls' 'refreshStatus()'
    assert_http_endpoint "custom 404 page" "/notfound" 200 "$profile_password" 404 'Page Not Found' 'Return to Home'
}

cleanup() {
    rm -f "$config_file"
}
trap cleanup EXIT INT TERM
write_config
pio test -e "$environment" --filter test_device_framework --upload-port "$port" --test-port "$port"
verify_web_interface
