#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR/.."

SDK_DIR="k230_sdk"
SDK_REPO="https://github.com/kendryte/k230_sdk"
DOCKER_IMAGE="k230_sdk"
NO_FASTBOOT=false

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --no-fastboot) NO_FASTBOOT=true; shift;;
        *) echo "Unknown option: $1"; exit 1;;
    esac
done

# Step 1: Clone SDK (skip if already present)
if [ ! -d "$SDK_DIR" ]; then
    echo "==> Cloning K230 SDK..."
    git clone "$SDK_REPO"
else
    echo "==> SDK directory already exists, skipping clone."
fi

cd "$SDK_DIR"

# Step 2: Download toolchain and source code
echo "==> Downloading toolchain and source code..."
make prepare_sourcecode

# Step 3: Build Docker image
echo "==> Building Docker image..."
docker build -f tools/docker/Dockerfile -t "$DOCKER_IMAGE" tools/docker

# Disable fastboot_app if requested (msh shell mode)
if [ "$NO_FASTBOOT" = true ]; then
    echo "==> Disabling fastboot_app (msh shell mode)..."
    echo "# msh shell ready" > src/big/rt-smart/init.sh
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
