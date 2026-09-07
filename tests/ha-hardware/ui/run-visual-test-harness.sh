#!/usr/bin/env bash
set -euo pipefail

arguments=(/opt/playwright/node_modules/.bin/playwright test --config /work/playwright.config.mjs)
if [[ "${PLAYWRIGHT_UPDATE_SNAPSHOTS:-0}" == "1" ]]; then
    arguments+=(--update-snapshots)
fi
exec "${arguments[@]}"
