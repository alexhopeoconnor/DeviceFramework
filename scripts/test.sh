#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage:
  ./scripts/test.sh compile  --platform esp8266|esp32 [--profile-fixture]
  ./scripts/test.sh examples --platform esp8266|esp32
  ./scripts/test.sh hardware --platform esp8266|esp32 --port /dev/ttyUSB0 [--env-file test/.env] [--profile-fixture]
EOF
    exit 2
}

"$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/tools/check-web-assets.sh"

mode="${1:-}"
[[ "$mode" == "compile" || "$mode" == "examples" || "$mode" == "hardware" ]] || usage
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

if [[ "$mode" == "examples" ]]; then
    mapfile -t examples < <(find examples -mindepth 1 -maxdepth 1 -type d -name '[0-9][0-9]-*' -print | sort)
    if (( ${#examples[@]} == 0 )); then
        echo "No example projects found" >&2
        exit 1
    fi
    for example in "${examples[@]}"; do
        pio run -d "$example" -e "$platform" </dev/null
    done
    echo "DeviceFramework examples compile check passed for $platform"
    exit 0
fi

environment="$platform"
refresh_clean_consumer_dependency() {
    local target_environment="$1"
    local cached_library="test/compile-project/.pio/libdeps/${target_environment}/DeviceFramework"
    [[ -d "$cached_library" || -e "${cached_library}.pio-link" ]] || return 0
    pio pkg uninstall -d test/compile-project -e "$target_environment" \
        -l DeviceFramework --no-save --skip-dependencies >/dev/null
}

[[ "$profile_fixture" == "true" ]] && environment="${platform}_profile"
if [[ "$mode" == "hardware" && "$profile_fixture" == "true" ]]; then
    environment="${platform}_profile_hardware"
fi
if [[ "$mode" == "compile" && "$profile_fixture" == "true" ]]; then
    pio run -d test/compile-project -e "$environment" -t clean >/dev/null
    pio run -d test/compile-project -e "${platform}_profile_no_wifi" -t clean >/dev/null
fi
if [[ "$mode" == "compile" ]]; then
    refresh_clean_consumer_dependency "$environment"
    pio run -d test/compile-project -e "$environment"
    if [[ "$profile_fixture" == "true" ]]; then
        local_profile_environment="${platform}_profile_no_wifi"
        refresh_clean_consumer_dependency "$local_profile_environment"
        pio run -d test/compile-project -e "$local_profile_environment"
    else
        refresh_clean_consumer_dependency "${platform}_default_ui"
        pio run -d test/compile-project -e "${platform}_default_ui"
    fi
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
    if [[ "$profile_fixture" == "true" ]]; then
        printf "%s\n" "#define TEST_EXPECT_WIFI_FALLBACK 1" >> "$config_file"
    fi
}

hardware_profile=""
hardware_smoke_profile=""
write_profile() {
    local target="$1"
    local profile_id="$2"
    local policy="$3"

    local password="$4"
    {
        printf "%s\n" "{"
        printf "%s\n" "  \"format\": 2,"
        printf "%s\n" "  \"application\": \"deviceframework\","
        printf "  \"profile\": { \"id\": \"%s\", \"revision\": 1, \"policy\": \"%s\" },\n" "$profile_id" "$policy"
        printf "  \"device_password\": \"%s\",\n" "$(escape_c_string "$password")"
        printf "%s\n" "  \"wifi\": { \"profiles\": ["
        printf "%s\n" "    { \"ssid\": \"df-test-primary-unavailable\", \"password\": \"\" },"
        printf "    { \"ssid\": \"%s\", \"password\": \"%s\" }\n" "$(escape_c_string "$DEVICEFRAMEWORK_TEST_WIFI_SSID")" "$(escape_c_string "$DEVICEFRAMEWORK_TEST_WIFI_PASSWORD")"
        printf "%s\n" "  ] }"
        printf "%s\n" "}"
    } > "$target"
}

write_hardware_profile() {
    hardware_profile="$(mktemp -p /tmp deviceframework-profile.XXXXXX)"
    hardware_smoke_profile="$(mktemp -p /tmp deviceframework-smoke-profile.XXXXXX)"
    chmod 600 "$hardware_profile" "$hardware_smoke_profile"
    write_profile "$hardware_profile" "hardware-${platform}-bootstrap" "bootstrap" "profile-fixture-password"
    write_profile "$hardware_smoke_profile" "hardware-${platform}-smoke" "reconcile" "profile-reconcile-password"
}


run_unity_hardware_test() {
    local output_file
    output_file="$(mktemp -p /tmp deviceframework-unity.XXXXXX)"

    # The generated credential header is intentionally ignored, so force the
    # test translation units that include it to rebuild for every hardware run.
    pio run -e "$environment" -t clean >/dev/null
    if [[ "$profile_fixture" == "true" ]]; then
        if ! DEVICEFRAMEWORK_HARDWARE_PROFILE="$hardware_profile" pio test -e "$environment" --filter test_device_framework --upload-port "$port" --without-testing >"$output_file" 2>&1; then
            cat "$output_file"
            rm -f "$output_file"
            return 1
        fi
    elif ! pio test -e "$environment" --filter test_device_framework --upload-port "$port" --without-testing >"$output_file" 2>&1; then
        cat "$output_file"
        rm -f "$output_file"
        return 1
    fi

    if ! python3 tools/capture-unity-serial.py --port "$port" --output "$output_file"; then
        cat "$output_file"
        rm -f "$output_file"
        return 1
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

wait_for_password_restart() {
    local password="$1"
    local status_code=""
    local attempt
    local stable_successes=0

    # A successful password update deliberately schedules a reboot. Observe
    # both the outage and recovered authentication rather than racing it.
    for attempt in {1..15}; do
        status_code="$(curl --silent --connect-timeout 1 --max-time 2 --output /dev/null \
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
        status_code="$(curl --silent --connect-timeout 1 --max-time 2 --output /dev/null \
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
        local mdns_name="$device_host"
        local attempt
        device_host=""
        for attempt in {1..45}; do
            device_host="$(avahi-resolve -4 -n "$mdns_name" 2>/dev/null | awk 'NR == 1 { print $2; exit }')"
            [[ -n "$device_host" ]] && break
            sleep 1
        done
        [[ -n "$device_host" ]] || {
            echo "Could not resolve the test device mDNS name within 45 seconds; set DEVICEFRAMEWORK_TEST_DEVICE_HOST to an IP address instead" >&2
            return 1
        }
    fi
    local profile_password=""
    if [[ "$profile_fixture" == "true" ]]; then
        local profile_source="${hardware_smoke_profile:-test/profiles/profile-fixture.json}"
        profile_password="$(sed -nE 's/^[[:space:]]*"device_password"[[:space:]]*:[[:space:]]*"([^"]*)"[[:space:]]*,?[[:space:]]*$/\1/p' "$profile_source")"
        [[ -n "$profile_password" ]] || { echo "Profile fixture has no device_password" >&2; return 1; }
        assert_http_endpoint "superseded bootstrap password" "/api/status" 401 "profile-fixture-password"
        assert_http_endpoint "unauthenticated API status" "/api/status" 401 ""
        assert_http_endpoint "unauthenticated stylesheet" "/assets/deviceframework.css" 401 ""
        assert_http_endpoint "unauthenticated logo" "/assets/deviceframework-logo" 401 ""
    fi

    assert_http_endpoint "API status" "/api/status" 200 "$profile_password" runtime chip_id version
    for page_pass in 1 2; do
        assert_http_endpoint "web interface root (pass $page_pass)" "/" 200 "$profile_password" "<!DOCTYPE html>" "Device Status" "System Controls" "deviceframework.css" "deviceframework.js" "DeviceFramework UI Test" "Test Lab" "df-web-theme" "--df-accent:#15803d" "</html>"
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
    rm -f "$config_file"
    [[ -z "$hardware_profile" ]] || rm -f "$hardware_profile"
    [[ -z "$hardware_smoke_profile" ]] || rm -f "$hardware_smoke_profile"
}
trap cleanup EXIT INT TERM
write_config
if [[ "$profile_fixture" == "true" ]]; then
    write_hardware_profile
    run_unity_hardware_test
    DEVICEFRAMEWORK_HARDWARE_PROFILE="$hardware_smoke_profile" pio run -d test/compile-project -e "$environment" -t upload --upload-port "$port"
else
    run_unity_hardware_test
fi
verify_web_interface
