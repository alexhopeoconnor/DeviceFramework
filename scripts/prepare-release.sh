#!/usr/bin/env bash
set -euo pipefail

tag="${1:-}"
[[ "$tag" =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "Usage: $0 vMAJOR.MINOR.PATCH [--tag]" >&2; exit 2; }
[[ "${2:-}" == "" || "${2:-}" == "--tag" ]] || { echo "Usage: $0 vMAJOR.MINOR.PATCH [--tag]" >&2; exit 2; }
version="${tag#v}"
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"
manifest_version="$(sed -n 's/.*"version": "\([^"]*\)".*/\1/p' library.json | head -n 1)"
build_version="$(sed -n -E '/DEVICEFRAMEWORK_LIBRARY_VERSION=/s/.*([0-9]+\.[0-9]+\.[0-9]+).*/\1/p' library.json)"
source_version="$(sed -n -E 's/^#define DEVICEFRAMEWORK_LIBRARY_VERSION "([^"]+)"/\1/p' src/Configuration/DeviceFrameworkIdentity.h)"
[[ "$manifest_version" == "$version" ]] || { echo "library.json is $manifest_version, expected $version" >&2; exit 1; }
[[ "$build_version" == "$version" ]] || { echo "library.json build flag is $build_version, expected $version" >&2; exit 1; }
[[ "$source_version" == "$version" ]] || { echo "DeviceFrameworkIdentity.h is $source_version, expected $version" >&2; exit 1; }
grep -q "^## $version$" CHANGELOG.md || {
    echo "CHANGELOG.md has no $version heading" >&2
    exit 1
}
if awk -v heading="## $version" '
    $0 == heading { found = 1; next }
    found && /^## / { exit }
    found { print }
' CHANGELOG.md | grep -Fq 'TODO: Describe this release.'; then
    echo "CHANGELOG.md still has the generated TODO for $version" >&2
    exit 1
fi
repo_url="https://github.com/alexhopeoconnor/DeviceFramework.git"
validate_reference() {
    local file="$1"
    local reference_count
    reference_count="$(grep -F "$repo_url#v" "$file" | wc -l)"
    [[ "$reference_count" -eq 1 ]] || { echo "$file must contain exactly one canonical release reference" >&2; exit 1; }
    grep -Fq "$repo_url#$tag" "$file" || { echo "$file does not reference $tag" >&2; exit 1; }
}
validate_reference README.md
validate_reference docs/GETTING_STARTED.md

git diff --check
package_dir="$(mktemp -d)"
trap 'rm -rf "$package_dir"' EXIT
pio pkg pack . --output "$package_dir/package.tar.gz" >/dev/null
echo "Validated release metadata and PlatformIO package for $tag"
if [[ "${2:-}" == "--tag" ]]; then
    git diff --quiet && git diff --cached --quiet
    git tag -a "$tag" -m "Release $tag"
    echo "Created $tag. Push the branch and tag; GitHub Actions will publish the release."
fi
