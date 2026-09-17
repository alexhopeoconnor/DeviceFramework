#!/usr/bin/env bash
# Shared PlatformIO invocation for DeviceFramework's maintained test targets.
#
# DeviceFramework has one maintained package graph: ESP8266 uses the pinned
# framework snapshot and ESP32 uses pioarduino 55.03.311.  Keep that graph in
# a dedicated Core, package, and cache root so an unrelated legacy project
# cannot inject stale package metadata or a flat esptool module into these
# test runs.  Callers may redirect all three locations for a disposable
# diagnosis or a CI workspace; this helper never removes a caller's cache.

df_pio_core_dir() {
    printf '%s\n' "${DEVICEFRAMEWORK_PLATFORMIO_CORE_DIR:-${XDG_CACHE_HOME:-$HOME/.cache}/deviceframework-platformio/current}"
}

df_pio_packages_dir() {
    local core_dir
    core_dir="$(df_pio_core_dir)"
    printf '%s\n' "${DEVICEFRAMEWORK_PLATFORMIO_PACKAGES_DIR:-$core_dir/packages}"
}

df_pio_cache_dir() {
    local core_dir
    core_dir="$(df_pio_core_dir)"
    printf '%s\n' "${DEVICEFRAMEWORK_PLATFORMIO_CACHE_DIR:-$core_dir/cache}"
}

df_pio() {
    local core_dir packages_dir cache_dir

    core_dir="$(df_pio_core_dir)"
    packages_dir="$(df_pio_packages_dir)"
    cache_dir="$(df_pio_cache_dir)"

    install -d -m 700 "$core_dir" "$packages_dir" "$cache_dir"
    PLATFORMIO_CORE_DIR="$core_dir" \
        PLATFORMIO_PACKAGES_DIR="$packages_dir" \
        PLATFORMIO_CACHE_DIR="$cache_dir" \
        command pio "$@"
}
