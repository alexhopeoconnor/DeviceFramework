#!/usr/bin/env bash
# Safe, non-secret identity input for the disposable OTA test harness.

declare -A DF_OTA_FIXTURE_LOCK_FDS=()

df_lock_ota_fixture_environment() {
    local environment="$1" lock_root lock_file lock_fd
    [[ "$environment" =~ ^[A-Za-z0-9_-]+$ ]] || {
        echo "Unsafe OTA fixture environment name: $environment" >&2
        return 2
    }
    [[ -n "${DF_OTA_FIXTURE_LOCK_FDS[$environment]:-}" ]] && return 0
    command -v flock >/dev/null 2>&1 || {
        echo "flock is required to protect PlatformIO OTA fixture inputs." >&2
        return 1
    }
    lock_root="$project_dir/test/ota-harness/.pio/harness-locks"
    install -d -m 700 "$lock_root"
    lock_file="$lock_root/${environment}.lock"
    exec {lock_fd}>"$lock_file"
    if ! flock -n "$lock_fd"; then
        echo "OTA fixture environment '$environment' is already in use by another local test-harness run." >&2
        return 1
    fi
    DF_OTA_FIXTURE_LOCK_FDS["$environment"]="$lock_fd"
}

df_ota_fixture_identity_dir() {
    [[ -n "${DEVICEFRAMEWORK_OTA_IDENTITY_DIR:-}" ]] || {
        echo "DEVICEFRAMEWORK_OTA_IDENTITY_DIR must be set before writing an OTA fixture identity." >&2
        return 2
    }
    printf '%s\n' "$DEVICEFRAMEWORK_OTA_IDENTITY_DIR"
}

df_ota_fixture_identity_file() {
    printf '%s/ota_fixture_identity.h\n' "$(df_ota_fixture_identity_dir)"
}

df_write_ota_fixture_identity() {
    local image="$1" version="$2" expected_portal_protected="${3:-}" directory target temporary
    case "$image" in
        A|B) ;;
        *) echo "OTA fixture identity must be A or B." >&2; return 2 ;;
    esac
    [[ "$version" =~ ^[A-Za-z0-9._-]+$ ]] || {
        echo "OTA fixture version contains unsupported characters." >&2
        return 2
    }
    [[ -z "$expected_portal_protected" || "$expected_portal_protected" == 0 || "$expected_portal_protected" == 1 ]] || {
        echo "OTA fixture portal expectation must be 0 or 1." >&2
        return 2
    }
    directory="$(df_ota_fixture_identity_dir)" || return
    target="$(df_ota_fixture_identity_file)"
    install -d -m 700 "$directory"
    temporary="$(mktemp "$directory/ota_fixture_identity.XXXXXX")"
    chmod 600 "$temporary"
    printf '%s\n' '#pragma once' > "$temporary"
    printf '#define DF_OTA_FIXTURE_IMAGE "%s"\n' "$image" >> "$temporary"
    printf '#define DF_OTA_FIXTURE_VERSION "%s"\n' "$version" >> "$temporary"
    if [[ -n "$expected_portal_protected" ]]; then
        # This is fixture metadata, generated beside the A/B identity rather
        # than baked into a second protected/open PlatformIO environment.
        printf '#define DF_OTA_FIXTURE_PORTAL_EXPECT_PROTECTED %s\n' "$expected_portal_protected" >> "$temporary"
    fi
    mv -f -- "$temporary" "$target"
}

df_remove_ota_fixture_identity() {
    local directory target
    [[ -n "${DEVICEFRAMEWORK_OTA_IDENTITY_DIR:-}" ]] || return 0
    directory="$(df_ota_fixture_identity_dir)" || return
    target="$(df_ota_fixture_identity_file)"
    rm -f -- "$target"
    rmdir -- "$directory" 2>/dev/null || true
}
