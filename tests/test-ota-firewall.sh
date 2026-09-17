#!/usr/bin/env bash
# Behavioral contract for the opt-in UFW callback helper. No real firewall or
# sudo command is invoked: shell functions model the narrow UFW surface.
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=tools/lib/ota-firewall.sh
source "$project_dir/tools/lib/ota-firewall.sh"

mock_root="$(mktemp -d)"
mock_state="$mock_root/ufw-added.txt"
mock_log="$mock_root/calls.txt"
trap 'rm -rf -- "$mock_root"' EXIT

fail() {
    echo "ota-firewall test failed: $*" >&2
    exit 1
}

rule_with_comment() {
    printf "ufw allow in on %s from %s to %s port %s proto tcp comment '%s'\n" \
        "$route_interface" "$resolved_ip" "$route_source_ip" "$host_port" "$1"
}

mock_ufw() {
    local command="${1:-}" previous="" argument comment="" expected temporary
    printf 'ufw %s\n' "$*" >> "$mock_log"
    case "$command" in
        status)
            printf '%s\n' 'Status: active'
            ;;
        show)
            [[ "${2:-}" == "added" ]] || fail "unexpected UFW show arguments: $*"
            printf '%s\n' "Added user rules (see 'ufw status' for running firewall):"
            cat "$mock_state"
            ;;
        allow)
            for argument in "$@"; do
                if [[ "$previous" == "comment" ]]; then
                    comment="$argument"
                    break
                fi
                previous="$argument"
            done
            [[ -n "$comment" ]] || fail "temporary allow rule lacks a comment"
            expected="$(rule_with_comment "$comment")"
            [[ "$*" == "allow in on $route_interface from $resolved_ip to $route_source_ip port $host_port proto tcp comment $comment" ]] || fail "temporary allow rule was not exact: $*"
            if [[ "$mock_mode" == "fail" ]]; then
                return 1
            fi
            printf '%s' "$expected" >> "$mock_state"
            if [[ "$mock_mode" == "fail-after-write" ]]; then
                return 1
            fi
            printf '%s\n' 'Rule added'
            ;;
        --force)
            [[ "${2:-}" == "delete" ]] || fail "unexpected UFW delete arguments: $*"
            for argument in "$@"; do
                if [[ "$previous" == "comment" ]]; then
                    comment="$argument"
                    break
                fi
                previous="$argument"
            done
            [[ -n "$comment" ]] || fail "temporary delete rule lacks a comment"
            expected="$(rule_with_comment "$comment")"
            [[ "$*" == "--force delete allow in on $route_interface from $resolved_ip to $route_source_ip port $host_port proto tcp comment $comment" ]] || fail "temporary delete rule was not exact: $*"
            grep -Fqx "$expected" "$mock_state" || fail "attempted to delete an unowned rule"
            temporary="$mock_root/state-without-owned-rule.txt"
            grep -Fvx "$expected" "$mock_state" > "$temporary" || true
            mv "$temporary" "$mock_state"
            printf '%s\n' 'Rule deleted'
            ;;
        *) fail "unexpected UFW command: $*" ;;
    esac
}

ufw() {
    mock_ufw "$@"
}

sudo() {
    printf 'sudo %s\n' "$*" >> "$mock_log"
    case "${1:-}" in
        -v) return 0 ;;
        -n) shift ;;
        *) fail "unexpected sudo arguments: $*" ;;
    esac
    [[ "${1:-}" == "env" ]] || fail "sudo did not receive env wrapper: $*"
    shift
    while [[ "${1:-}" == *=* ]]; do
        shift
    done
    [[ "${1:-}" == "$ufw_path" ]] || fail "sudo did not call the selected UFW command: $*"
    shift
    mock_ufw "$@"
}

reset_context() {
    local name="$1"
    route_interface="wlp1s0"
    resolved_ip="192.168.1.177"
    route_source_ip="192.168.1.86"
    host_port="43123"
    platform="esp8266"
    auth_mode="protected"
    run_dir="$mock_root/$name"
    install -d -m 700 "$run_dir"
    ufw_path=""
    firewall_rule_comment=""
    firewall_rule_note=""
    firewall_rule_added="no"
    mock_mode="success"
    : > "$mock_state"
    : > "$mock_log"
}

reset_context add-and-remove
enable_temporary_callback_firewall_rule
[[ "$firewall_rule_added" == "yes" ]] || fail "new rule was not marked owned"
[[ -n "$firewall_rule_comment" ]] || fail "new rule did not get a unique comment"
grep -Fqx "$(rule_with_comment "$firewall_rule_comment")" "$mock_state" || fail "owned rule missing"
release_callback_firewall
[[ "$firewall_rule_added" == "no" && -z "$firewall_rule_comment" ]] || fail "owned rule state not cleared"
[[ ! -s "$mock_state" ]] || fail "owned rule was not removed"
grep -Fq 'ufw --force delete allow in on wlp1s0 from 192.168.1.177 to 192.168.1.86 port 43123 proto tcp comment' "$mock_log" || fail "release did not use the full owned-rule specification"

reset_context existing-allow
printf '%s' "$(rule_with_comment existing-user-allow)" > "$mock_state"
enable_temporary_callback_firewall_rule
[[ "$firewall_rule_added" == "no" ]] || fail "existing allow became runner-owned"
grep -Fqx "$(rule_with_comment existing-user-allow)" "$mock_state" || fail "existing allow changed"
if grep -Fq 'ufw allow in on' "$mock_log"; then
    fail "existing allow triggered a new UFW allow command"
fi

reset_context existing-deny
printf '%s\n' 'ufw deny in on wlp1s0 from 192.168.1.177 to 192.168.1.86 port 43123 proto tcp' > "$mock_state"
if enable_temporary_callback_firewall_rule; then
    fail "matching deny rule was accepted"
fi
[[ "$firewall_rule_added" == "no" ]] || fail "matching deny became runner-owned"
grep -Fqx 'ufw deny in on wlp1s0 from 192.168.1.177 to 192.168.1.86 port 43123 proto tcp' "$mock_state" || fail "matching deny was changed"
if grep -Fq 'ufw allow in on' "$mock_log"; then
    fail "matching deny triggered a new UFW allow command"
fi

reset_context add-failure-rollback
mock_mode="fail-after-write"
if enable_temporary_callback_firewall_rule; then
    fail "failed UFW add was accepted"
fi
[[ "$firewall_rule_added" == "yes" ]] || fail "partially added owned rule was not retained for cleanup"
release_callback_firewall
[[ ! -s "$mock_state" ]] || fail "partially added owned rule was not removed"

echo "DeviceFramework ArduinoOTA firewall contract passed"
