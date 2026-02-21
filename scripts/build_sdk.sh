#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SDK_DIR="k230_sdk"
SDK_REPO="https://github.com/kendryte/k230_sdk"
DOCKER_IMAGE="k230_sdk"

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
