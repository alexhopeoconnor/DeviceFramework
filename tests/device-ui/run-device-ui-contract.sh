#!/usr/bin/env bash
set -euo pipefail

playwright=(/work/node_modules/.bin/playwright test --config /work/playwright.config.cjs)

# Portal provisioning deliberately moves the board off 192.168.4.1. Capture
# the portal while it is still present, then run the ordinary end-to-end
# contract (including the actual AP-to-station hand-off). The same ordering is
# harmless for the connected web UI and avoids making the media test depend on
# Playwright's file-discovery order.
if [[ "${DEVICE_UI_CAPTURE_README_MEDIA:-0}" == "1" ]]; then
    DEVICE_UI_README_MEDIA_ONLY=1 "${playwright[@]}" tests/readme-media.spec.cjs
    DEVICE_UI_README_MEDIA_ONLY=0 "${playwright[@]}"
else
    exec "${playwright[@]}"
fi
