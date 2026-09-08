#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
tool="$project_dir/tools/device-ui-hardware"

bash -n "$tool" "$project_dir/tools/lib/device-ui-session.sh"

if "$tool" >/dev/null 2>&1; then
    echo "device-ui-hardware accepted an empty command" >&2
    exit 1
fi

if "$tool" down --output unexpected >/dev/null 2>&1; then
    echo "device-ui-hardware accepted unsupported down arguments" >&2
    exit 1
fi

rg -Fq 'Only the explicit secondary adapter may be reconfigured' \
    "$project_dir/tools/lib/device-ui-session.sh"
rg -Fq 'Refusing to use the host default-route interface' \
    "$project_dir/tools/lib/device-ui-session.sh"
rg -Fq 'artifacts/device-ui/' "$project_dir/.gitignore"

echo "DeviceFramework device-UI CLI contract passed"
