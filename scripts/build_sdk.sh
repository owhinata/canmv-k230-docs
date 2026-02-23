#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR/.."

SDK_DIR="k230_sdk"
DOCKER_IMAGE="k230_sdk"
FASTBOOT=false
RTT_CTRL=true

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --with-fastboot) FASTBOOT=true; shift;;
        --no-rtt-ctrl)   RTT_CTRL=false; shift;;
        *) echo "Unknown option: $1"; exit 1;;
    esac
done

# Step 1: Initialize SDK submodule (skip if already present)
if [ ! -d "$SDK_DIR/.git" ]; then
    echo "==> Initializing K230 SDK submodule..."
    git submodule update --init "$SDK_DIR"
else
    echo "==> SDK submodule already initialized."
fi

cd "$SDK_DIR"

# Step 2: Download toolchain and source code
echo "==> Downloading toolchain and source code..."
make prepare_sourcecode

# Step 3: Build Docker image
echo "==> Building Docker image..."
docker build -f tools/docker/Dockerfile -t "$DOCKER_IMAGE" tools/docker

# Disable fastboot_app unless --with-fastboot is specified (msh shell mode)
if [ "$FASTBOOT" = false ]; then
    echo "==> Disabling fastboot_app (msh shell mode)..."
    echo "# msh shell ready" > src/big/rt-smart/init.sh
fi

# Enable rtt-ctrl (Linux → RT-Smart msh command execution)
# Patch the defconfig source so the setting survives menuconfig_to_code.sh
# which copies common_rttlinux.config → rtconfig.h during build.
if [ "$RTT_CTRL" = true ]; then
    RTT_CONFIG="src/big/rt-smart/kernel/bsp/maix3/configs/common_rttlinux.config"
    if ! grep -q 'RT_USING_RTT_CTRL' "$RTT_CONFIG"; then
        echo "==> Enabling RT_USING_RTT_CTRL in $RTT_CONFIG..."
        sed -i '/RT_USING_IPCM/a #define RT_USING_RTT_CTRL' "$RTT_CONFIG"
    fi
fi

# Step 4: Build SDK inside Docker
echo "==> Building SDK (this may take a while)..."
docker run -it --rm \
    --user "$(id -u):$(id -g)" \
    -v /etc/passwd:/etc/passwd:ro \
    -v /etc/group:/etc/group:ro \
    -v "$(pwd):$(pwd)" \
    -v "$(pwd)/toolchain:/opt/toolchain" \
    -w "$(pwd)" \
    "$DOCKER_IMAGE" \
    bash -c "make CONF=k230_canmv_defconfig"

echo "==> Done. Build artifacts are in output/."
