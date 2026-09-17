#!/usr/bin/env bash
# Shared host-side helpers for DeviceFramework's real-board visual contract.
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

dfui_default_route_interface() {
    ip route show default 2>/dev/null | awk '/^default/{print $5; exit}'
}

dfui_acquire_hardware_lock() {
    # Share the lock used by the WiFiManager hardware runner: both tools can
    # address the same serial board and secondary Wi-Fi adapter on one host.
    local lock_file="${DEVICEFRAMEWORK_HARDWARE_LOCK_FILE:-${TMPDIR:-/tmp}/deviceframework-hardware-test.lock}"
    exec 9>"$lock_file"
    if ! flock -n 9; then
        echo "Another DeviceFramework hardware task is active; waiting for its board/build lock." >&2
        flock 9
    fi
}

dfui_require_client_adapter() {
    local interface="$1" allow_takeover="$2" default_interface active_connection
    ip link show "$interface" >/dev/null 2>&1 || {
        echo "Wi-Fi interface not found: $interface" >&2
        return 1
    }
    default_interface="$(dfui_default_route_interface)"
    [[ "$interface" != "$default_interface" ]] || {
        echo "Refusing to use the host default-route interface: $interface" >&2
        return 1
    }
    active_connection="$(nmcli -g GENERAL.CONNECTION device show "$interface" 2>/dev/null || true)"
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
    local interface="$1" ssid="$2" attempt
    nmcli device wifi rescan ifname "$interface" >/dev/null 2>&1 || true
    for attempt in $(seq 1 45); do
        if nmcli -t -f SSID device wifi list ifname "$interface" | grep -Fxq "$ssid"; then
            return 0
        fi
        sleep 1
        nmcli device wifi rescan ifname "$interface" >/dev/null 2>&1 || true
    done
    echo "DeviceFramework portal SSID was not detected on $interface: $ssid" >&2
    return 1
}

dfui_remove_connection_by_name() {
    local name="$1"
    [[ -n "$name" ]] || return 0
    nmcli connection down "$name" >/dev/null 2>&1 || true
    nmcli connection delete "$name" >/dev/null 2>&1 || true
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
    nmcli device disconnect "$interface" >/dev/null 2>&1 || true
    if ! dfui_wait_for_portal_ssid "$interface" "$ssid"; then
        dfui_remove_portal_connection
        return 1
    fi
    if ! nmcli connection add type wifi ifname "$interface" con-name "$name" ssid "$ssid" \
        ipv4.method auto ipv4.never-default yes ipv6.method ignore connection.autoconnect no >/dev/null; then
        dfui_remove_portal_connection
        return 1
    fi
    # The UUID is assigned at creation time, so capture it before subsequent
    # security, autoconnect, or association operations introduce an
    # interruption window.
    uuid="$(nmcli -g connection.uuid connection show "$name")"
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
    if [[ -n "$password" ]] && ! nmcli connection modify "$name" wifi-sec.key-mgmt wpa-psk wifi-sec.psk "$password"; then
        dfui_remove_portal_connection
        return 1
    fi
    if [[ "$reconnect_after_drop" == "yes" ]] && ! nmcli connection modify "$name" connection.autoconnect yes; then
        dfui_remove_portal_connection
        return 1
    fi
    if ! nmcli connection up "$name" ifname "$interface"; then
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
        nmcli connection down uuid "$uuid" >/dev/null 2>&1 || true
        nmcli connection delete uuid "$uuid" >/dev/null 2>&1 || true
    else
        dfui_remove_connection_by_name "$name"
    fi
    dfui_clear_state
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
        rm -f "$temporary_file"
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
    rm -f "$(dfui_state_file)"
}
