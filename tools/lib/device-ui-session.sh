#!/usr/bin/env bash
# shellcheck source=tools/lib/harness-locks.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/harness-locks.sh"
# Shared host-side helpers for DeviceFramework's real-board visual test harness.
# Only the explicit secondary adapter may be reconfigured for portal testing.

dfui_state_root() {
    printf '%s/deviceframework-device-ui' "${XDG_STATE_HOME:-$HOME/.local/state}"
}

dfui_require() {
    command -v "$1" >/dev/null 2>&1 || {
        echo "Required command not found: $1" >&2
        return 1
    }
}

dfui_nmcli_permission() {
    local permission="$1"
    awk -F: -v permission="$permission" '$1 == permission { print $2; exit }' \
        <<<"${DFUI_NMCLI_PERMISSIONS:-}"
}

dfui_prepare_networkmanager_authorization() {
    # A graphical Polkit agent can authorize direct nmcli actions. SSH and
    # other headless shells often lack one, so select the scoped sudo path
    # before the board is erased instead of hiding a rejected scan and calling
    # it an AP-discovery failure later.
    local permission value direct=yes
    case "${DFUI_NMCLI_AUTH:-auto}" in
        auto|direct|sudo) ;;
        *)
            echo 'DFUI_NMCLI_AUTH must be auto, direct, or sudo.' >&2
            return 2
            ;;
    esac
    [[ "${DFUI_NMCLI_AUTH_READY:-no}" == yes ]] && return 0

    DFUI_NMCLI_PERMISSIONS="$(nmcli -t -f PERMISSION,VALUE general permissions 2>/dev/null || true)"
    for permission in \
        org.freedesktop.NetworkManager.wifi.scan \
        org.freedesktop.NetworkManager.network-control \
        org.freedesktop.NetworkManager.settings.modify.system; do
        value="$(dfui_nmcli_permission "$permission")"
        [[ "$value" == yes ]] || direct=no
    done

    case "${DFUI_NMCLI_AUTH:-auto}" in
        direct)
            DFUI_NMCLI_MODE=direct
            ;;
        sudo)
            DFUI_NMCLI_MODE=sudo
            ;;
        auto)
            if [[ "$direct" == yes ]]; then
                DFUI_NMCLI_MODE=direct
            # A session D-Bus socket is common over SSH but does not itself
            # provide a graphical Polkit agent, so require an actual display.
            elif [[ -z "${DISPLAY:-}" && -z "${WAYLAND_DISPLAY:-}" ]]; then
                DFUI_NMCLI_MODE=sudo
            else
                # Let a graphical Polkit agent authorize the command. Any
                # genuine NetworkManager error remains visible to the caller.
                DFUI_NMCLI_MODE=direct
            fi
            ;;
    esac

    if [[ "$DFUI_NMCLI_MODE" == sudo ]]; then
        command -v sudo >/dev/null 2>&1 || {
            echo 'NetworkManager requires authorization, but sudo is unavailable. Use a graphical Polkit session or install/configure sudo.' >&2
            return 1
        }
        echo 'NetworkManager requires scoped authorization for the named portal adapter; validating sudo before the board is flashed.' >&2
        sudo -v || {
            echo 'Could not validate sudo for the scoped NetworkManager portal actions.' >&2
            return 1
        }
    fi
    DFUI_NMCLI_AUTH_READY=yes
    export DFUI_NMCLI_MODE DFUI_NMCLI_AUTH_READY DFUI_NMCLI_PERMISSIONS
}

dfui_report_networkmanager_authorization() {
    local permission value direct=yes
    DFUI_NMCLI_PERMISSIONS="$(nmcli -t -f PERMISSION,VALUE general permissions 2>/dev/null || true)"
    for permission in \
        org.freedesktop.NetworkManager.wifi.scan \
        org.freedesktop.NetworkManager.network-control \
        org.freedesktop.NetworkManager.settings.modify.system; do
        value="$(dfui_nmcli_permission "$permission")"
        [[ "$value" == yes ]] || direct=no
    done
    if [[ "$direct" == yes ]]; then
        echo 'NetworkManager portal authorization: direct.'
    elif [[ -z "${DISPLAY:-}" && -z "${WAYLAND_DISPLAY:-}" ]]; then
        echo 'NetworkManager portal authorization: scoped sudo will be requested before a portal command flashes the board.'
    else
        echo 'NetworkManager portal authorization: graphical Polkit may authorize actions; set DFUI_NMCLI_AUTH=sudo to use scoped sudo instead.'
    fi
}

dfui_nmcli() {
    # The browser runner, its artifacts, and its private state remain owned by
    # the developer. Only these named NetworkManager actions use sudo when the
    # preflight selected it, and they remain constrained to the explicit
    # non-default adapter or a generated temporary connection.
    if [[ "${DFUI_NMCLI_MODE:-direct}" == sudo ]]; then
        sudo -n true || {
            echo 'The sudo authorization for scoped NetworkManager actions expired; run sudo -v and retry the portal command.' >&2
            return 1
        }
        sudo -n -- nmcli "$@"
    else
        nmcli "$@"
    fi
}

dfui_default_route_interface() {
    ip route show default 2>/dev/null | awk '/^default/{print $5; exit}'
}

dfui_acquire_hardware_lock() {
    # All ordinary ESP portals use this gateway/subnet. Keep portal commands
    # mutually exclusive even when they name different boards or adapters.
    df_harness_lock_portal_network
}

dfui_require_client_adapter() {
    local interface="$1" allow_takeover="$2" default_interface device_type active_connection
    ip link show "$interface" >/dev/null 2>&1 || {
        echo "Wi-Fi interface not found: $interface" >&2
        return 1
    }
    default_interface="$(dfui_default_route_interface)"
    [[ "$interface" != "$default_interface" ]] || {
        echo "Refusing to use the host default-route interface: $interface" >&2
        return 1
    }
    if ! device_type="$(dfui_nmcli -g GENERAL.TYPE device show "$interface" 2>/dev/null)"; then
        echo "NetworkManager could not inspect the selected portal adapter: $interface" >&2
        return 1
    fi
    [[ "$device_type" == "wifi" ]] || {
        echo "Client adapter is not Wi-Fi: $interface (${device_type:-unknown})." >&2
        return 1
    }
    df_harness_lock_wifi_adapter "$interface"
    if ! active_connection="$(dfui_nmcli -g GENERAL.CONNECTION device show "$interface" 2>/dev/null)"; then
        echo "NetworkManager could not inspect the selected portal adapter: $interface" >&2
        return 1
    fi
    if [[ -n "$active_connection" && "$active_connection" != "--" && "$allow_takeover" != "yes" ]]; then
        echo "Client adapter $interface already has connection '$active_connection'." >&2
        echo "Pass --take-over-client-adapter to replace only that adapter's connection." >&2
        return 1
    fi
}

dfui_portal_ssid() {
    case "$1" in
        esp8266) printf '%s\n' 'DF-Portal-ESP8266' ;;
        esp32) printf '%s\n' 'DF-Portal-ESP32' ;;
        *) return 1 ;;
    esac
}

dfui_wait_for_portal_ssid() {
    local interface="$1" ssid="$2" attempt advertised
    if ! dfui_nmcli device wifi rescan ifname "$interface"; then
        echo "NetworkManager could not scan the selected portal adapter: $interface" >&2
        return 1
    fi
    for attempt in $(seq 1 45); do
        if ! advertised="$(dfui_nmcli -t -f SSID device wifi list ifname "$interface")"; then
            echo "NetworkManager could not read Wi-Fi scan results from $interface." >&2
            return 1
        fi
        if grep -Fxq "$ssid" <<<"$advertised"; then
            return 0
        fi
        sleep 1
        if ! dfui_nmcli device wifi rescan ifname "$interface"; then
            echo "NetworkManager could not refresh Wi-Fi scan results from $interface." >&2
            return 1
        fi
    done
    echo "DeviceFramework portal SSID was not detected on $interface: $ssid" >&2
    return 1
}

dfui_remove_connection_by_name() {
    local name="$1"
    [[ -n "$name" ]] || return 0
    if ! dfui_nmcli connection down "$name" >/dev/null 2>&1; then
        :  # The profile may be pending or already disconnected.
    fi
    if ! dfui_nmcli connection delete "$name"; then
        # A pending recovery record can survive an uncatchable exit before
        # `connection add` ran. A successful complete connection listing that
        # does not contain this generated name is positive evidence that
        # there is nothing left to delete. Never make that inference when
        # NetworkManager cannot be queried: an authorization failure must
        # retain the state for a later `down` command.
        if dfui_connection_name_is_absent "$name"; then
            return 0
        fi
        dfui_report_portal_connection_cleanup_failure
        return 1
    fi
}

dfui_connection_name_is_absent() {
    local name="$1" names
    if ! names="$(dfui_nmcli -t -f NAME connection show 2>/dev/null)"; then
        return 1
    fi
    ! grep -Fxq -- "$name" <<<"$names"
}

dfui_connection_uuid_is_absent() {
    local uuid="$1" uuids
    if ! uuids="$(dfui_nmcli -t -f UUID connection show 2>/dev/null)"; then
        return 1
    fi
    ! grep -Fxq -- "$uuid" <<<"$uuids"
}

dfui_report_portal_connection_cleanup_failure() {
    echo 'Could not remove the temporary DeviceFramework portal connection. Its recovery state was retained; restore NetworkManager authorization and run ./tools/device-ui-hardware down.' >&2
}

dfui_create_portal_connection() {
    local interface="$1" ssid="$2" password="$3" reconnect_after_drop="${4:-no}" platform="${5:-}" name uuid
    name="deviceframework-portal-${RANDOM}-$(date +%s)"
    # Keep an in-process ownership record before the first NetworkManager
    # mutation.  The caller's signal trap can then remove this exact temporary
    # connection throughout the pending-to-active state transition.
    DFUI_PORTAL_CONNECTION_NAME="$name"
    DFUI_PORTAL_CONNECTION_UUID=""
    DFUI_PORTAL_CONNECTION_OWNED=yes
    export DFUI_PORTAL_CONNECTION_UUID DFUI_PORTAL_CONNECTION_NAME DFUI_PORTAL_CONNECTION_OWNED
    # Commit a pending record before the first NetworkManager mutation.  If
    # the host dies after this point, `down` can remove this exact name even
    # before NetworkManager's UUID has been obtained.
    if [[ -n "$platform" ]] && ! dfui_write_state "$interface" "$platform" "" "$name"; then
        DFUI_PORTAL_CONNECTION_OWNED=no
        unset DFUI_PORTAL_CONNECTION_UUID DFUI_PORTAL_CONNECTION_NAME
        return 1
    fi
    if ! dfui_nmcli device disconnect "$interface" >/dev/null 2>&1; then
        :  # An idle adapter has nothing to disconnect.
    fi
    if ! dfui_wait_for_portal_ssid "$interface" "$ssid"; then
        # The state was written before scanning so an uncatchable exit can be
        # recovered. This known scan failure happens before `connection add`,
        # so it has not created the generated NetworkManager profile and can
        # discard only that pending record directly.
        if ! dfui_clear_state; then
            dfui_report_portal_connection_cleanup_failure
            return 1
        fi
        DFUI_PORTAL_CONNECTION_OWNED=no
        unset DFUI_PORTAL_CONNECTION_UUID DFUI_PORTAL_CONNECTION_NAME
        return 1
    fi
    if ! dfui_nmcli connection add type wifi ifname "$interface" con-name "$name" ssid "$ssid" \
        ipv4.method auto ipv4.never-default yes ipv6.method ignore connection.autoconnect no >/dev/null; then
        dfui_remove_portal_connection
        return 1
    fi
    # The UUID is assigned at creation time, so capture it before subsequent
    # security, autoconnect, or association operations introduce an
    # interruption window.
    uuid="$(dfui_nmcli -g connection.uuid connection show "$name")"
    if [[ -z "$uuid" || "$uuid" == "--" ]]; then
        dfui_remove_portal_connection
        echo "NetworkManager did not return a UUID for the portal connection." >&2
        return 1
    fi
    DFUI_PORTAL_CONNECTION_UUID="$uuid"
    export DFUI_PORTAL_CONNECTION_UUID
    # Atomically replace the pending record with the exact UUID before
    # association.  The normal signal trap removes it immediately; the record
    # also lets `down` clean up after an uncatchable host termination.
    if [[ -n "$platform" ]] && ! dfui_write_state "$interface" "$platform" "$uuid" "$name"; then
        dfui_remove_portal_connection
        return 1
    fi
    # An empty fixture password deliberately means an open portal. A fresh
    # NetworkManager connection has no wireless-security settings, so do not
    # manufacture a WPA configuration in that case. The caller has already
    # been restricted to an explicit non-default adapter.
    if [[ -n "$password" ]] && ! dfui_nmcli connection modify "$name" wifi-sec.key-mgmt wpa-psk wifi-sec.psk "$password"; then
        dfui_remove_portal_connection
        return 1
    fi
    if [[ "$reconnect_after_drop" == "yes" ]] && ! dfui_nmcli connection modify "$name" connection.autoconnect yes; then
        dfui_remove_portal_connection
        return 1
    fi
    if ! dfui_nmcli connection up "$name" ifname "$interface"; then
        dfui_remove_portal_connection
        return 1
    fi
}

dfui_verify_portal_route() {
    local interface="$1" route
    route="$(ip route get 192.168.4.1 2>/dev/null || true)"
    [[ "$route" == *" dev $interface "* ]] || {
        echo "Portal route does not use the selected adapter: $route" >&2
        return 1
    }
}

dfui_remove_portal_connection() {
    local uuid="${DFUI_PORTAL_CONNECTION_UUID:-}" name="${DFUI_PORTAL_CONNECTION_NAME:-}"
    [[ -n "$uuid" || -n "$name" ]] || return 0
    if [[ -n "$uuid" ]]; then
        if ! dfui_nmcli connection down uuid "$uuid" >/dev/null 2>&1; then
            :  # Continue to deletion even if the profile is already down.
        fi
        if ! dfui_nmcli connection delete uuid "$uuid"; then
            # An active UUID means the runner did create a profile. Retain its
            # recovery state unless the same authorized NetworkManager view
            # positively proves another actor already removed it. A failed
            # listing (including an expired scoped sudo ticket) is never
            # treated as absence.
            if ! dfui_connection_uuid_is_absent "$uuid"; then
                dfui_report_portal_connection_cleanup_failure
                return 1
            fi
        fi
    else
        dfui_remove_connection_by_name "$name" || return 1
    fi
    if ! dfui_clear_state; then
        echo 'The temporary DeviceFramework portal connection was removed, but its recovery state could not be cleared.' >&2
        return 1
    fi
    DFUI_PORTAL_CONNECTION_OWNED=no
    unset DFUI_PORTAL_CONNECTION_UUID DFUI_PORTAL_CONNECTION_NAME
}

dfui_state_file() {
    printf '%s/session.env\n' "$(dfui_state_root)"
}

dfui_require_no_active_session() {
    local file
    file="$(dfui_state_file)"
    [[ ! -e "$file" ]] || {
        echo "An existing DeviceFramework portal session is recorded; run ./tools/device-ui-hardware down first." >&2
        return 1
    }
}

dfui_write_state() {
    local interface="$1" platform="$2" uuid="$3" name="$4" root file temporary_file state
    root="$(dfui_state_root)"
    file="$(dfui_state_file)"
    install -d -m 700 "$root"
    if [[ -n "$uuid" ]]; then
        state="active"
    else
        state="pending"
    fi
    temporary_file="$(mktemp "$root/.session.env.XXXXXX")" || return 1
    if ! {
        printf 'DFUI_PORTAL_STATE=%s\nDFUI_PORTAL_INTERFACE=%s\nDFUI_PORTAL_PLATFORM=%s\nDFUI_PORTAL_CONNECTION_UUID=%s\nDFUI_PORTAL_CONNECTION_NAME=%s\n' \
            "$state" "$interface" "$platform" "$uuid" "$name"
    } > "$temporary_file"; then
        rm -f -- "$temporary_file"
        return 1
    fi
    chmod 600 "$temporary_file"
    mv -f "$temporary_file" "$file"
}

dfui_load_state() {
    local file key value
    file="$(dfui_state_file)"
    [[ -f "$file" ]] || {
        echo "No active DeviceFramework portal session was found." >&2
        return 1
    }
    DFUI_PORTAL_INTERFACE=""
    DFUI_PORTAL_PLATFORM=""
    DFUI_PORTAL_CONNECTION_UUID=""
    DFUI_PORTAL_CONNECTION_NAME=""
    DFUI_PORTAL_STATE=""
    while IFS='=' read -r key value; do
        case "$key" in
            DFUI_PORTAL_STATE|DFUI_PORTAL_INTERFACE|DFUI_PORTAL_PLATFORM|DFUI_PORTAL_CONNECTION_UUID|DFUI_PORTAL_CONNECTION_NAME)
                printf -v "$key" '%s' "$value"
                ;;
            '') ;;
            *)
                echo "Invalid DeviceFramework portal session state." >&2
                return 1
                ;;
        esac
    done < "$file"
    [[ -n "$DFUI_PORTAL_INTERFACE" && -n "$DFUI_PORTAL_PLATFORM" && -n "$DFUI_PORTAL_CONNECTION_NAME" ]] || {
        echo "Incomplete DeviceFramework portal session state." >&2
        return 1
    }
    # Retained state created by older runners has no phase but always carries
    # a UUID; treat it as the active form for backwards-compatible cleanup.
    if [[ -z "$DFUI_PORTAL_STATE" && -n "$DFUI_PORTAL_CONNECTION_UUID" ]]; then
        DFUI_PORTAL_STATE="active"
    fi
    case "$DFUI_PORTAL_STATE" in
        pending)
            [[ -z "$DFUI_PORTAL_CONNECTION_UUID" ]] || {
                echo "Invalid DeviceFramework pending portal session state." >&2
                return 1
            }
            ;;
        active)
            [[ -n "$DFUI_PORTAL_CONNECTION_UUID" ]] || {
                echo "Incomplete DeviceFramework active portal session state." >&2
                return 1
            }
            ;;
        *)
            echo "Invalid DeviceFramework portal session state." >&2
            return 1
            ;;
    esac
    export DFUI_PORTAL_STATE DFUI_PORTAL_INTERFACE DFUI_PORTAL_PLATFORM DFUI_PORTAL_CONNECTION_UUID DFUI_PORTAL_CONNECTION_NAME
}

dfui_clear_state() {
    rm -f -- "$(dfui_state_file)"
}
