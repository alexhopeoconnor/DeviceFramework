#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage:
  ./scripts/run-tests.sh compile  --platform esp8266|esp32
  ./scripts/run-tests.sh hardware --platform esp8266|esp32 --port /dev/ttyUSB0 [--env-file test/.env]
EOF
    exit 2
}

mode="${1:-}"
[[ "$mode" == "compile" || "$mode" == "hardware" ]] || usage
shift
platform=""
port=""
env_file="test/.env"
while [[ $# -gt 0 ]]; do
    case "$1" in
        --platform) platform="${2:-}"; shift 2 ;;
        --port) port="${2:-}"; shift 2 ;;
        --env-file) env_file="${2:-}"; shift 2 ;;
        *) usage ;;
    esac
done
[[ "$platform" == "esp8266" || "$platform" == "esp32" ]] || usage
[[ "$mode" != "hardware" || -n "$port" ]] || usage

environment="$platform"
if [[ "$mode" == "compile" ]]; then
    pio test -e "$environment" --without-uploading --without-testing
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

host_ip="${DEVICEFRAMEWORK_TEST_HOST_IP:-}"
if [[ -z "$host_ip" ]]; then
    host_ip="$(ip route get 1.1.1.1 | awk '{print $7; exit}')"
fi
[[ -n "$host_ip" ]] || { echo "Set DEVICEFRAMEWORK_TEST_HOST_IP in $env_file" >&2; exit 1; }
fetcher_port="${DEVICEFRAMEWORK_TEST_FETCHER_PORT:-8080}"
config_file="test/test_device_framework/test_config.generated.h"

escape_c_string() { printf '%s' "$1" | sed 's/\\/\\\\/g; s/"/\\"/g'; }
write_config() {
    printf '%s\n' '#pragma once' > "$config_file"
    printf '#define TEST_FETCHER_HOST "%s"\n' "$(escape_c_string "$host_ip")" >> "$config_file"
    printf '#define TEST_FETCHER_PORT %s\n' "$fetcher_port" >> "$config_file"
    printf '#define TEST_FETCHER_URL "http://%s:%s"\n' "$(escape_c_string "$host_ip")" "$fetcher_port" >> "$config_file"
    printf '#define TEST_WIFI_SSID "%s"\n' "$(escape_c_string "$DEVICEFRAMEWORK_TEST_WIFI_SSID")" >> "$config_file"
    printf '#define TEST_WIFI_PASSWORD "%s"\n' "$(escape_c_string "$DEVICEFRAMEWORK_TEST_WIFI_PASSWORD")" >> "$config_file"
    printf '#define TEST_MQTT_SERVER "%s"\n' "$(escape_c_string "$DEVICEFRAMEWORK_TEST_MQTT_SERVER")" >> "$config_file"
    printf '#define TEST_MQTT_USER "%s"\n' "$(escape_c_string "$DEVICEFRAMEWORK_TEST_MQTT_USER")" >> "$config_file"
    printf '#define TEST_MQTT_PASSWORD "%s"\n' "$(escape_c_string "$DEVICEFRAMEWORK_TEST_MQTT_PASSWORD")" >> "$config_file"
}
cleanup() {
    rm -f "$config_file"
    docker compose -f scripts/docker-compose.test.yml down --remove-orphans >/dev/null 2>&1 || true
}
trap cleanup EXIT INT TERM
write_config
export FETCHER_PORT="$fetcher_port"
docker compose -f scripts/docker-compose.test.yml up -d endpoint-fetcher
until curl -fsS --connect-timeout 1 --max-time 2 "http://localhost:$fetcher_port/health" >/dev/null; do sleep 1; done
pio test -e "$environment" --filter test_device_framework --upload-port "$port" --test-port "$port"
