#!/usr/bin/env bash
# Shared PlatformIO invocation for DeviceFramework's maintained test targets.
#
# DeviceFramework has one maintained package graph: ESP8266 uses the pinned
# framework snapshot and ESP32 uses pioarduino 55.03.311. Keep that graph in
# the shared maintained-framework Core, package, and cache root: WiFiManager,
# DFTE, ArduinoHA, and DeviceFramework deliberately use the same pins, so
# separate repository caches would redownload identical multi-gigabyte inputs.
# It remains separate from an unrelated legacy/global PlatformIO installation.
# Callers may redirect all three locations for a disposable diagnosis or CI
# workspace; this helper never removes a caller's cache.

df_pio_core_dir() {
    printf '%s\n' "${DEVICEFRAMEWORK_PLATFORMIO_CORE_DIR:-${XDG_CACHE_HOME:-$HOME/.cache}/arduino-framework-platformio/core-3.3.11}"
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

df_pio_executable() {
    # PlatformIO's normal installer places its executable below the user's
    # PlatformIO Core directory, but non-interactive shells do not always add
    # that directory to PATH. Prefer an explicit override or PATH, then use
    # the standard installation path so hardware runners work over SSH too.
    local executable
    if [[ -n "${DEVICEFRAMEWORK_PIO_EXECUTABLE:-}" ]]; then
        executable="$DEVICEFRAMEWORK_PIO_EXECUTABLE"
    else
        executable="$(command -v pio 2>/dev/null || true)"
        if [[ -z "$executable" && -x "$HOME/.platformio/penv/bin/pio" ]]; then
            executable="$HOME/.platformio/penv/bin/pio"
        fi
    fi
    [[ -n "$executable" && -x "$executable" ]] || {
        echo "PlatformIO is required; install it or set DEVICEFRAMEWORK_PIO_EXECUTABLE." >&2
        return 1
    }
    printf '%s\n' "$executable"
}

df_pio_available() {
    df_pio_executable >/dev/null
}

df_pio() {
    local core_dir packages_dir cache_dir executable

    core_dir="$(df_pio_core_dir)"
    packages_dir="$(df_pio_packages_dir)"
    cache_dir="$(df_pio_cache_dir)"
    executable="$(df_pio_executable)" || return

    install -d -m 700 "$core_dir" "$packages_dir" "$cache_dir"
    PLATFORMIO_CORE_DIR="$core_dir" \
        PLATFORMIO_PACKAGES_DIR="$packages_dir" \
        PLATFORMIO_CACHE_DIR="$cache_dir" \
        "$executable" "$@"
}
