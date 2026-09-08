#!/usr/bin/env bash
set -euo pipefail
exec /work/node_modules/.bin/playwright test --config /work/playwright.config.cjs
