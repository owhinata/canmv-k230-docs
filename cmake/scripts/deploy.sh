#!/usr/bin/env bash
# deploy.sh — Auto-detect K230 IP and deploy files via SCP.
#
# Environment variables:
#   K230_USER       SSH user (required)
#   K230_IP         IP address (empty = auto-detect)
#   K230_SERIAL_LC  Littlecore serial port for auto-detect
#   K230_BAUD       Baud rate
#   K230_DEPLOY_DIR Remote deploy directory
#
# Usage: deploy.sh <local:remote> [...]
set -euo pipefail

: "${K230_USER:?K230_USER is required}"
: "${K230_SERIAL_LC:?K230_SERIAL_LC is required}"
: "${K230_BAUD:?K230_BAUD is required}"
: "${K230_DEPLOY_DIR:?K230_DEPLOY_DIR is required}"

IP="${K230_IP:-}"

# --- IP auto-detection ---
if [ -z "$IP" ]; then
    echo "K230_IP not set. Auto-detecting via ${K230_SERIAL_LC}..."
    detect_ip() {
        local port="$1" baud="$2"
        [ -e "$port" ] || return 1
        command -v fuser &>/dev/null && fuser "$port" &>/dev/null && return 1

        stty -F "$port" "$baud" raw -echo
        exec 4<>"$port"
        timeout 0.2 cat <&4 >/dev/null 2>&1 || true

        # Send CR to elicit a prompt (shell or login)
        printf '\r' >&4; sleep 0.3
        local prompt
        prompt=$(timeout 1 cat <&4 2>/dev/null || true)

        # If at login prompt, send root + empty password
        if echo "$prompt" | grep -qi 'login:'; then
            printf 'root\r' >&4; sleep 0.3
            # Consume password prompt (if any) and send empty password
            timeout 0.5 cat <&4 >/dev/null 2>&1 || true
            printf '\r' >&4; sleep 0.5
            timeout 0.5 cat <&4 >/dev/null 2>&1 || true
        fi

        printf 'ifconfig\r' >&4; sleep 0.5

        local output
        output=$(timeout 1 cat <&4 2>/dev/null || true)
        exec 4>&-

        # BusyBox ifconfig: "inet addr:192.168.x.x" — exclude 127.0.0.1
        echo "$output" | grep -oE 'inet addr:([0-9.]+)' | cut -d: -f2 \
            | grep -v '^127\.' | head -1
    }

    IP=$(detect_ip "$K230_SERIAL_LC" "$K230_BAUD" || true)
    if [ -z "$IP" ]; then
        echo "Error: Auto-detection failed." >&2
        echo "  - Is littlecore serial (${K230_SERIAL_LC}) available? (close picocom if open)" >&2
        echo "  - Or set manually: cmake -DK230_IP=<ip>" >&2
        exit 1
    fi
    echo "Detected K230 IP: ${IP}"
fi

# --- SSH connection test ---
SSH_OPTS=(-o StrictHostKeyChecking=no -o ConnectTimeout=5)

echo "Connecting to ${K230_USER}@${IP}..."
if ! ssh "${SSH_OPTS[@]}" "${K230_USER}@${IP}" true 2>/dev/null; then
    echo "Error: Cannot connect to ${K230_USER}@${IP}" >&2
    echo "  - Verify K230 is powered on and network-connected" >&2
    echo "  - Set up SSH key: ssh-copy-id ${K230_USER}@${IP}" >&2
    exit 1
fi

ssh "${SSH_OPTS[@]}" "${K230_USER}@${IP}" "mkdir -p ${K230_DEPLOY_DIR}"

# --- SCP transfer ---
for mapping in "$@"; do
    local_path="${mapping%%:*}"
    remote_name="${mapping#*:}"
    if [ ! -f "$local_path" ]; then
        echo "Error: File not found: $local_path" >&2
        echo "  (Have you run the 'train' target?)" >&2
        exit 1
    fi
    echo "  ${remote_name}"
    scp -q "${SSH_OPTS[@]}" "$local_path" "${K230_USER}@${IP}:${K230_DEPLOY_DIR}/${remote_name}"
done

echo "Deployed to ${IP}:${K230_DEPLOY_DIR}/"
