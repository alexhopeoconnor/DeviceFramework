#!/usr/bin/env bash
# ArduinoOTA callback-firewall helpers. The caller supplies route and run
# variables after it has resolved the board and selected the normal LAN route.

firewall_recovery_instructions() {
    echo "Temporary UFW callback rule may still be present; it was not removed automatically." >&2
    echo "Find only the rule tagged '$firewall_rule_comment' with: sudo ufw status numbered" >&2
    echo "Then remove that numbered rule with: sudo ufw delete NUMBER" >&2
    [[ -z "$firewall_rule_note" ]] || echo "Recovery details: $firewall_rule_note" >&2
}

owns_ufw_callback_rule() {
    local added_rules expected
    [[ -n "$ufw_path" && -n "$firewall_rule_comment" ]] || return 1
    added_rules="$(sudo -n env LC_ALL=C "$ufw_path" show added 2>/dev/null)" || return 1
    expected="ufw allow in on $route_interface from $resolved_ip to $route_source_ip port $host_port proto tcp comment '$firewall_rule_comment'"
    grep -Fqx "$expected" <<<"$added_rules"
}

existing_ufw_callback_tuple() {
    local added_rules tuple matching line
    added_rules="$(sudo -n env LC_ALL=C "$ufw_path" show added)" || return 3
    tuple="in on $route_interface from $resolved_ip to $route_source_ip port $host_port proto tcp"
    matching="$(grep -F "$tuple" <<<"$added_rules" || true)"
    [[ -n "$matching" ]] || return 1
    while IFS= read -r line; do
        # An exact user-owned allow may already cover this callback. A deny,
        # reject, limit, route rule, or unusual action must stop instead: UFW
        # could otherwise replace it when asked to add an allow rule.
        case "$line" in
            "ufw allow in on $route_interface from $resolved_ip to $route_source_ip port $host_port proto tcp"*|\
            "ufw allow log in on $route_interface from $resolved_ip to $route_source_ip port $host_port proto tcp"*|\
            "ufw allow log-all in on $route_interface from $resolved_ip to $route_source_ip port $host_port proto tcp"*) ;;
            *)
                echo "Firewall: an incompatible user-owned UFW rule already matches the callback tuple:" >&2
                echo "  $line" >&2
                echo "The runner will not replace it; adjust the host policy or use a different --host-port." >&2
                return 2
                ;;
        esac
    done <<<"$matching"
    return 0
}

release_callback_firewall() {
    [[ "$firewall_rule_added" == "yes" ]] || return 0
    if ! owns_ufw_callback_rule; then
        firewall_recovery_instructions
        return 1
    fi
    if ! sudo -n env LC_ALL=C "$ufw_path" --force delete allow in on "$route_interface" \
        from "$resolved_ip" to "$route_source_ip" port "$host_port" proto tcp \
        comment "$firewall_rule_comment" >/dev/null; then
        firewall_recovery_instructions
        return 1
    fi
    echo "Firewall: removed temporary UFW callback rule tagged '$firewall_rule_comment'."
    firewall_rule_added="no"
    firewall_rule_comment=""
}

print_manual_callback_firewall_rule() {
    printf '%s\n' 'Firewall manual action (only if the host policy blocks the callback):'
    printf '  sudo %q allow in on %q from %q to %q port %q proto tcp comment %q\n' \
        "$ufw_path" "$route_interface" "$resolved_ip" "$route_source_ip" "$host_port" \
        'DeviceFramework ArduinoOTA callback'
    printf '%s\n' 'The board is the TCP client. Host-side TCP 8266/3232 rules do not permit this callback.'
}

report_callback_firewall() {
    if ! command -v ufw >/dev/null 2>&1; then
        echo "Firewall: UFW is not installed; --firewall check made no policy change."
        return 0
    fi
    ufw_path="$(command -v ufw)"
    if command -v systemctl >/dev/null 2>&1 && systemctl is-active --quiet ufw.service 2>/dev/null; then
        echo "Firewall: UFW service is active; callback permission cannot be inferred from a local bind."
    elif sudo -n env LC_ALL=C "$ufw_path" status 2>/dev/null | grep -q '^Status: active'; then
        echo "Firewall: UFW is active; callback permission cannot be inferred from a local bind."
    else
        echo "Firewall: UFW is installed, but its active state is unavailable without elevated access."
    fi
}

enable_temporary_callback_firewall_rule() {
    local add_output status existing_status
    command -v ufw >/dev/null 2>&1 || {
        echo "--firewall allow requires UFW; use --firewall manual for another host firewall." >&2
        return 1
    }
    ufw_path="$(command -v ufw)"
    sudo -v || {
        echo "--firewall allow needs sudo to add its temporary callback rule." >&2
        return 1
    }
    status="$(sudo -n env LC_ALL=C "$ufw_path" status 2>&1)" || {
        echo "Could not read UFW status after sudo authentication." >&2
        return 1
    }
    if ! grep -q '^Status: active' <<<"$status"; then
        echo "Firewall: UFW is not active; no callback rule was added."
        return 0
    fi

    # UFW updates the comment (and can replace action/logging details) when an
    # otherwise identical rule already exists. Inspect before adding so every
    # user-owned rule remains untouched, including a same-tuple deny/reject.
    if existing_ufw_callback_tuple; then
        echo "Firewall: an existing UFW allow rule already permits this callback; the runner left it unchanged."
        return 0
    else
        existing_status=$?
    fi
    case "$existing_status" in
        1) ;; # No matching tuple; it is safe to add our uniquely tagged rule.
        2) return 1 ;;
        *)
            echo "Could not inspect existing UFW rules before adding a temporary callback rule." >&2
            return 1
            ;;
    esac

    firewall_rule_comment="deviceframework-ota-${platform}-${auth_mode}-${BASHPID}-${RANDOM}"
    firewall_rule_note="$run_dir/firewall-callback-rule.txt"
    {
        printf 'comment=%s\n' "$firewall_rule_comment"
        printf 'interface=%s\n' "$route_interface"
        printf 'board_ip=%s\n' "$resolved_ip"
        printf 'host_ip=%s\n' "$route_source_ip"
        printf 'host_port=%s\n' "$host_port"
        printf "remove_with=sudo %s --force delete allow in on %s from %s to %s port %s proto tcp comment '%s'\n" \
            "$ufw_path" "$route_interface" "$resolved_ip" "$route_source_ip" "$host_port" "$firewall_rule_comment"
    } > "$firewall_rule_note"
    chmod 600 "$firewall_rule_note"
    if ! add_output="$(sudo -n env LC_ALL=C "$ufw_path" allow in on "$route_interface" \
        from "$resolved_ip" to "$route_source_ip" port "$host_port" proto tcp \
        comment "$firewall_rule_comment" 2>&1)"; then
        echo "Could not add the temporary UFW callback rule:" >&2
        echo "$add_output" >&2
        if owns_ufw_callback_rule; then
            firewall_rule_added="yes"
        fi
        return 1
    fi
    if ! owns_ufw_callback_rule; then
        echo "UFW accepted a callback rule but the runner cannot prove ownership from its unique comment." >&2
        firewall_recovery_instructions
        return 1
    fi
    firewall_rule_added="yes"
    echo "Firewall: added a temporary UFW callback rule tagged '$firewall_rule_comment'."
}

configure_callback_firewall() {
    case "$firewall_mode" in
        check)
            report_callback_firewall
            echo "Firewall: --firewall check does not change policy; use --firewall manual or --firewall allow if needed."
            ;;
        manual)
            report_callback_firewall
            [[ -n "$ufw_path" ]] || ufw_path="ufw"
            print_manual_callback_firewall_rule
            echo "Firewall: --firewall manual did not change policy."
            ;;
        allow)
            enable_temporary_callback_firewall_rule
            ;;
    esac
}
