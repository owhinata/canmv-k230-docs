# K230 SDK Build

This guide walks you through building the K230 SDK from source to generate the toolchain and firmware images.

## Prerequisites

- Docker installed
- git installed
- x86_64 Linux host

## 1. Clone the SDK

```bash
git clone https://github.com/kendryte/k230_sdk
cd k230_sdk
```

## 2. Download Toolchain and Source Code

Use the scripts included in the SDK to download the toolchain and dependency source code.

```bash
source tools/get_download_url.sh
make prepare_sourcecode
```

Once complete, the cross-compiler is extracted under the `toolchain/` directory.

## 3. Build the Docker Image

Create the build Docker image from the Dockerfile included in the SDK.

```bash
docker build -f tools/docker/Dockerfile -t k230_sdk tools/docker
```

## 4. Build the SDK (Inside Docker)

Build the SDK inside a Docker container. The host user and group IDs are passed through to avoid file permission issues.

```bash
docker run -it --rm \
    --user $(id -u):$(id -g) \
    -v /etc/passwd:/etc/passwd:ro \
    -v /etc/group:/etc/group:ro \
    -v $(pwd):$(pwd) \
    -v $(pwd)/toolchain:/opt/toolchain \
    -w $(pwd) \
    k230_sdk \
    bash -c "make CONF=k230_canmv_defconfig"
```

!!! note "Build time"
    The initial build may take from tens of minutes to several hours.

## Build Artifacts

After a successful build, firmware images are generated in the `output/` directory.
The `toolchain/` directory contains the cross-compiler, used for application development such as the [bigcore Hello World Build](hello_world.md).

| Path | Contents |
|------|----------|
| `output/` | Firmware images (for SD card flashing) |
| `toolchain/` | RISC-V cross-compiler |

!!! tip "Build script"
    A build script automating the steps above is also available:
    [owhinata/canmv-k230](https://github.com/owhinata/canmv-k230)
