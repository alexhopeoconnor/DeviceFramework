#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "Usage: $0 vMAJOR.MINOR.PATCH" >&2
    exit 2
}

tag="${1:-}"
[[ "$tag" =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]] || usage

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
version="${tag#v}"
repo_url="https://github.com/alexhopeoconnor/DeviceFramework.git"
reference_files=(README.md docs/GETTING_STARTED.md)

dependency_version() {
    local name="$1"
    sed -n -E '/"name": "'"$name"'"/,/"version":/s/.*#v([0-9]+\.[0-9]+\.[0-9]+).*/\1/p' \
        "$root/library.json" | head -n 1
}

update_compatibility_row() {
    local series="${version%.*}"
    local wifi_version dfte_version arduinoha_version row temporary
    wifi_version="$(dependency_version WiFiManager)"
    dfte_version="$(dependency_version DeviceFrameworkTemplateEngine)"
    arduinoha_version="$(dependency_version home-assistant-integration)"
    [[ -n "$wifi_version" && -n "$dfte_version" && -n "$arduinoha_version" ]] || {
        echo "Could not read the maintained dependency versions from library.json." >&2
        exit 1
    }
    row="| ${series}.x | ${wifi_version} | ${dfte_version} | ${arduinoha_version} | ESP8266, ESP32 |"
    temporary="$(mktemp)"
    awk -v series="$series" -v row="$row" '
        $0 ~ "^\\| " series "\\.x \\|" { print row; written = 1; next }
        !written && $0 ~ /^\| [0-9]+\.[0-9]+\.x \|/ { print row; written = 1 }
        { print }
        END { if (!written) exit 1 }
    ' "$root/docs/COMPATIBILITY.md" > "$temporary" || {
        rm -f "$temporary"
        echo "Could not update docs/COMPATIBILITY.md." >&2
        exit 1
    }
    mv "$temporary" "$root/docs/COMPATIBILITY.md"
}

current_version="$(sed -n 's/.*"version": "\([^"]*\)".*/\1/p' "$root/library.json" | head -n 1)"
[[ "$current_version" != "$version" ]] || {
    echo "library.json already declares $version; choose a new version." >&2
    exit 1
}
grep -q "^## $version$" "$root/CHANGELOG.md" && {
    echo "CHANGELOG.md already has a $version section; choose a new version." >&2
    exit 1
}

sed -i -E '0,/"version": "[0-9]+\.[0-9]+\.[0-9]+"/s//"version": "'"$version"'"/' "$root/library.json"
sed -i -E "/DEVICEFRAMEWORK_LIBRARY_VERSION=/s/[0-9]+\.[0-9]+\.[0-9]+/$version/" "$root/library.json"
sed -i -E "/^#define DEVICEFRAMEWORK_LIBRARY_VERSION /s/\"[^\"]+\"/\"$version\"/" "$root/src/Configuration/DeviceFrameworkIdentity.h"

for file in "${reference_files[@]}"; do
    sed -i -E "s|${repo_url}#v[0-9]+\.[0-9]+\.[0-9]+|${repo_url}#v${version}|g" "$root/$file"
done

update_compatibility_row

temp_file="$(mktemp)"
trap 'rm -f "$temp_file"' EXIT
{
    IFS= read -r changelog_heading < "$root/CHANGELOG.md"
    [[ "$changelog_heading" == "# Changelog" ]] || {
        echo "CHANGELOG.md must begin with # Changelog" >&2
        exit 1
    }
    printf '%s\n\n## %s\n\n- TODO: Describe this release.\n' "$changelog_heading" "$version"
    tail -n +2 "$root/CHANGELOG.md"
} > "$temp_file"
mv "$temp_file" "$root/CHANGELOG.md"

echo "Updated DeviceFramework declarations, compatibility row, and canonical install references to $tag."
echo "Replace the generated changelog TODO with the release summary, then run scripts/check-docs.sh and scripts/prepare-release.sh $tag."
