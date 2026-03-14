#!/usr/bin/env bash
# run.sh — Send a command to K230 bigcore msh via serial port.
#
# Forwards stdin to serial so interactive input (e.g. 'q' to quit) works.
# Press Ctrl+C to disconnect.
#
# Environment variables:
#   K230_SERIAL  Serial port (required)
#   K230_BAUD    Baud rate (required)
#
# Usage: run.sh <command>
set -euo pipefail

: "${K230_SERIAL:?K230_SERIAL is required}"
: "${K230_BAUD:?K230_BAUD is required}"

CMD="$1"

if [ ! -e "$K230_SERIAL" ]; then
    echo "Error: Serial port not found: $K230_SERIAL" >&2
    echo "  - Is the K230 connected via USB?" >&2
    echo "  - Try: ls /dev/ttyACM*" >&2
    exit 1
fi

if command -v fuser &>/dev/null && fuser "$K230_SERIAL" &>/dev/null; then
    echo "Error: $K230_SERIAL is in use by another process." >&2
    echo "  Close minicom/picocom first." >&2
    exit 1
fi

if [ ! -w "$K230_SERIAL" ]; then
    echo "Error: No write permission on $K230_SERIAL" >&2
    echo "  Run: sudo usermod -aG dialout \$USER" >&2
    exit 1
fi

stty -F "$K230_SERIAL" "$K230_BAUD" raw -echo

# Save terminal settings and switch to char-by-char mode.
# -icanon: deliver keystrokes immediately (no line buffering)
# -echo: don't local-echo (the remote side echoes back)
# -icrnl: pass Enter as CR (0x0D), not NL — K230 msh expects CR
# isig is kept so Ctrl+C still generates SIGINT.
OLD_TTY=$(stty -g)
stty -icanon -echo -icrnl min 1 time 0

exec 3<>"$K230_SERIAL"

# serial → stdout (background)
cat <&3 &
CAT_PID=$!

cleanup() {
    kill "$CAT_PID" 2>/dev/null
    exec 3>&-
    stty "$OLD_TTY"
    exit 0
}
trap cleanup INT TERM EXIT

echo "Sending to msh: $CMD"
echo "Press Ctrl+C to disconnect."
printf '%s\r' "$CMD" >&3

# stdin → serial (foreground, forwards keystrokes until Ctrl+C)
cat >&3
