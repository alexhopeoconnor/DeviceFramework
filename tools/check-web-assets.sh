#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
generated="$root/src/WebInterface/templates/WebInterfaceLogo.h"
temporary="$(mktemp)"
trap 'rm -f "$temporary"' EXIT

"$root/tools/generate-web-assets.sh" "$temporary" >/dev/null
if ! cmp -s "$temporary" "$generated"; then
    echo "Generated web-logo header is stale. Run ./tools/generate-web-assets.sh" >&2
    diff -u "$generated" "$temporary" || true
    exit 1
fi

echo "Web assets are current."
