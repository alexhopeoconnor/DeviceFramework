#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
failed=0

link_pattern='\]\(([^ )]+)'
while IFS= read -r file; do
    in_fence=false
    while IFS= read -r line || [[ -n "$line" ]]; do
        if [[ "$line" =~ ^[[:space:]]*\`\`\` ]]; then
            [[ "$in_fence" == true ]] && in_fence=false || in_fence=true
            continue
        fi
        [[ "$in_fence" == true ]] && continue
        remainder="$line"
        while [[ "$remainder" =~ $link_pattern ]]; do
            target="${BASH_REMATCH[1]}"
            remainder="${remainder#*]($target)}"
            case "$target" in
                \#*|http://*|https://*|mailto:*|tel:*) continue ;;
            esac
            target="${target%%#*}"
            [[ -z "$target" ]] && continue
            if [[ "$target" == /* ]]; then
                candidate="$root/${target#/}"
            else
                candidate="$(dirname "$file")/$target"
            fi
            if [[ ! -e "$candidate" ]]; then
                printf 'Broken local Markdown link: %s -> %s\n' "${file#$root/}" "$target" >&2
                failed=1
            fi
        done
    done < "$file"
done < <(find "$root" -path "$root/.git" -prune -o -path '*/.pio' -prune -o -type f -name '*.md' -print)

for required in \
    README.md CHANGELOG.md docs/README.md docs/GETTING_STARTED.md docs/CONFIGURATION.md \
    docs/LIFECYCLE.md docs/PARAMETERS.md docs/HOME_ASSISTANT_MQTT.md docs/API_REFERENCE.md \
    docs/OPERATIONS.md docs/TROUBLESHOOTING.md docs/SCENARIOS.md \
    docs/WEB_UI.md docs/WEB_RESOURCES.md docs/TESTING.md docs/DEVELOPMENT.md docs/COMPATIBILITY.md docs/TARGETS.md \
    docs/scenarios/fault-tolerant-temperature-sensor.md docs/scenarios/protected-output-controller.md \
    docs/scenarios/presence-aware-lighting.md docs/scenarios/product-family-deployment.md; do
    if [[ ! -f "$root/$required" ]]; then
        printf 'Missing required documentation file: %s\n' "$required" >&2
        failed=1
    fi
done

check_readme_media() {
    local asset="$1"
    local expected_type="$2"
    local max_bytes="$3"
    local path="$root/docs/assets/readme/$asset"
    if [[ ! -s "$path" ]]; then
        printf 'Missing README media asset: %s\n' "docs/assets/readme/$asset" >&2
        failed=1
        return
    fi
    if [[ "$(file --brief --mime-type "$path")" != "$expected_type" ]]; then
        printf 'Unexpected README media type: %s\n' "docs/assets/readme/$asset" >&2
        failed=1
    fi
    if (( $(wc -c < "$path") > max_bytes )); then
        printf 'README media exceeds its size limit: %s\n' "docs/assets/readme/$asset" >&2
        failed=1
    fi
}

check_readme_media device-lifecycle-tour.gif image/gif $((2 * 1024 * 1024))
check_readme_media device-status.png image/png $((1024 * 1024))
check_readme_media home-assistant-device-page.png image/png $((1024 * 1024))

if [[ -n "$(git -C "$root" ls-files -- 'artifacts/readme-media/**')" ]]; then
    printf 'Ignored README media artifacts must not be tracked.\n' >&2
    failed=1
fi

if [[ "${DEVICEFRAMEWORK_SKIP_WEB_ASSET_CHECK:-0}" != "1" ]]; then
    "$root/tools/check-web-assets.sh"
fi

while IFS= read -r example; do
    for required in README.md platformio.ini; do
        if [[ ! -f "$example/$required" ]]; then
            printf 'Incomplete example: %s is missing %s\n' "${example#$root/}" "$required" >&2
            failed=1
        fi
    done
    if ! find "$example" -maxdepth 3 -type f \( -name '*.ino' -o -name '*.cpp' \) -print -quit | grep -q .; then
        printf 'Incomplete example: %s has no source\n' "${example#$root/}" >&2
        failed=1
    fi
done < <(find "$root/examples" -mindepth 1 -maxdepth 1 -type d -name '[0-9][0-9]-*' -print | sort)

if [[ "$failed" -ne 0 ]]; then
    exit 1
fi

echo "DeviceFramework documentation checks passed"
