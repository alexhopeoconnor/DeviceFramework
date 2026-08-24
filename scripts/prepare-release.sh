#!/usr/bin/env bash
set -euo pipefail

tag="${1:-}"
[[ "$tag" =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "Usage: $0 vMAJOR.MINOR.PATCH [--tag]" >&2; exit 2; }
[[ "${2:-}" == "" || "${2:-}" == "--tag" ]] || { echo "Usage: $0 vMAJOR.MINOR.PATCH [--tag]" >&2; exit 2; }
version="${tag#v}"
manifest_version="$(sed -n 's/.*"version": "\([^"]*\)".*/\1/p' library.json | head -n 1)"
[[ "$manifest_version" == "$version" ]] || { echo "library.json is $manifest_version, expected $version" >&2; exit 1; }
package_dir="$(mktemp -d)"
trap 'rm -rf "$package_dir"' EXIT
pio pkg pack . --output "$package_dir/package.tar.gz" >/dev/null
echo "Validated PlatformIO package for $tag"
if [[ "${2:-}" == "--tag" ]]; then
    git diff --quiet && git diff --cached --quiet
    git tag -a "$tag" -m "Release $tag"
    echo "Created $tag. Push the branch and tag; GitHub Actions will publish the release."
fi
