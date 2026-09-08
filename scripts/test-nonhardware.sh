#!/usr/bin/env bash
# Run DeviceFramework's board-free release gate as one interruptible command.
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$project_dir"

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

"$project_dir/tools/check-web-assets.sh"
run_test compile --platform esp8266 --release
run_test compile --platform esp8266 --profile-fixture --release
run_test compile --platform esp32 --release
run_test compile --platform esp32 --profile-fixture --release
run_test examples --platform esp8266
run_test examples --platform esp32
DEVICEFRAMEWORK_SKIP_WEB_ASSET_CHECK=1 "$project_dir/scripts/check-docs.sh"
"$project_dir/tests/test-device-ui-hardware-cli.sh"
"$project_dir/tools/ha-hardware" fixture --ui-capture

echo "DeviceFramework non-hardware test suite passed"
