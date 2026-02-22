# sample_vicap Build

This guide explains how to build the K230 `sample_vicap` application using CMake out-of-tree build. This sample captures video frames from a camera sensor (VICAP) and displays them on a screen (VO), demonstrating the K230 MPP media pipeline.

## Prerequisites

- K230 SDK must be built (toolchain extracted, MPP libraries compiled)
- SDK placed at `k230_sdk/` in the repository root
- Host OS: x86_64 Linux
- CMake 3.16 or later

!!! note "Building the SDK"
    For K230 SDK build instructions, see [SDK Build](sdk_build.md).

## Overview

`sample_vicap` is the official K230 SDK sample for Video Input Capture (VICAP). It demonstrates:

- Configuring camera sensors via the VICAP API
- Setting up the Video Output (VO) display pipeline
- Binding VICAP output channels to VO layers for real-time preview
- Optional GDMA-based rotation for display
- Frame dumping for debugging

### Source Files

| File | Description |
|------|-------------|
| `sample_vicap.c` | Main application — argument parsing, VICAP/VO setup, frame dump loop |
| `vo_test_case.c` | VO display helper functions — DSI init, layer/OSD creation, connector setup |
| `vo_test_case.h` | Header for VO helper functions and type definitions |
| `vo_bind_test.c` | VVI-to-VO binding tests (used for `vo_layer_bind_config`, `vdss_bind_vo_config`) |

These files are copied from the SDK:

- `sample_vicap.c` from `k230_sdk/src/big/mpp/userapps/sample/sample_vicap/`
- `vo_test_case.c`, `vo_test_case.h`, `vo_bind_test.c` from `k230_sdk/src/big/mpp/userapps/sample/sample_vo/`

## Build Steps

### 1. Configure

```bash
cmake -B build/sample_vicap -S apps/sample_vicap \
  -DCMAKE_TOOLCHAIN_FILE="$(pwd)/cmake/toolchain-k230-rtsmart.cmake"
```

### 2. Build

```bash
cmake --build build/sample_vicap
```

### 3. Verify

```bash
file build/sample_vicap/sample_vicap
```

Expected output:

```
sample_vicap: ELF 64-bit LSB executable, UCB RISC-V, RVC, double-float ABI, version 1 (SYSV), statically linked, ...
```

## CMakeLists.txt Details

The `apps/sample_vicap/CMakeLists.txt` handles:

- **MPP include paths**: Headers from `mpp/include/`, `mpp/include/comm/`, `mpp/include/ioctl/`, and `mpp/userapps/api/`
- **MPP static libraries**: All 46 MPP libraries linked with `--start-group` / `--end-group` to resolve circular dependencies
- **`-Wno-error`**: SDK sample code contains warnings, so `-Werror` (set by the toolchain) is disabled for this target

## Command-Line Arguments

```
./sample_vicap -mode <mode> -dev <dev> -sensor <sensor> -chn <chn> -ow <width> -oh <height> [options]
```

### Global Options

| Option | Description | Default |
|--------|-------------|---------|
| `-mode <n>` | Work mode: 0=online, 1=offline, 2=sw_tile | 0 |
| `-conn <n>` | Connector: 0=HX8377, 1=LT9611-1080p60, 2=LT9611-1080p30, 3=ST7701, 4=ILI9806 | 0 |

### Per-Device Options (after `-dev <n>`)

| Option | Description | Default |
|--------|-------------|---------|
| `-dev <n>` | VICAP device ID (0, 1, 2) | 0 |
| `-sensor <n>` | Sensor type (see SDK sensor list) | — |
| `-ae <0\|1>` | Enable/disable AE | 1 |
| `-awb <0\|1>` | Enable/disable AWB | 1 |
| `-hdr <0\|1>` | Enable/disable HDR | 0 |
| `-dw <0\|1>` | Enable dewarp | 0 |

### Per-Channel Options (after `-chn <n>`)

| Option | Description | Default |
|--------|-------------|---------|
| `-chn <n>` | Output channel ID (0, 1, 2) | 0 |
| `-ow <width>` | Output width (aligned to 16) | sensor width |
| `-oh <height>` | Output height | sensor height |
| `-ofmt <n>` | Pixel format: 0=YUV420, 1=RGB888, 2=RGB888P, 3=RAW | 0 |
| `-preview <0\|1>` | Enable display preview | 1 |
| `-rotation <n>` | Rotation: 0=0, 1=90, 2=180, 3=270, 4=none, 17-19=GDMA rotation | 0 |
| `-crop <0\|1>` | Enable crop | 0 |
| `-fps <n>` | Frame rate limit (0=unlimited) | 0 |

### Example

```bash
# OV9732 sensor, 640x480 output, 90-degree rotation, preview enabled
./sample_vicap -mode 0 -dev 0 -sensor 0 -chn 0 -ow 640 -oh 480 -preview 1 -rotation 1
```

## Processing Flow

The application follows this pipeline:

```
Sensor → VICAP (dev) → VICAP (chn) → [GDMA rotation] → VO (layer) → Display
```

### Initialization Sequence

1. **Parse arguments** — configure device/channel parameters
2. **Query connector info** — get display resolution
3. **Query sensor info** — get camera resolution
4. **Set VICAP device attributes** — configure input, ISP pipeline, work mode
5. **Initialize VO connector** — set up display hardware
6. **Initialize VB (Video Buffer)** — allocate buffer pools
7. **Set VICAP channel attributes** — configure output format, size, crop
8. **Bind VICAP to VO** — connect capture output to display layers (optionally via GDMA for rotation)
9. **Configure VO layers** — set display layer sizes, positions, rotation
10. **Start VICAP stream** — begin capture
11. **Enable VO** — start display output

### Interactive Commands

While running, the application accepts keyboard commands:

| Key | Action |
|-----|--------|
| `d` | Dump current frame to file |
| `h` | Dump HDR buffer |
| `s` | Set ISP AE ROI |
| `g` | Get ISP AE ROI |
| `t` | Toggle test pattern |
| `r` | Dump ISP register config to file |
| `q` | Quit |

## Transferring and Running on K230

### Transfer via SCP

```bash
scp build/sample_vicap/sample_vicap root@<K230_IP_ADDRESS>:/sharefs/sample_vicap
```

!!! warning "About /sharefs/"
    The correct destination is `/sharefs/sample_vicap`, **not** `/root/sharefs/sample_vicap`.
    `/sharefs/` is a vfat partition (`/dev/mmcblk1p4`) directly accessible from the bigcore.

### Run on the K230 bigcore (msh)

On the K230 serial console (ACM1), run:

```
msh /> /sharefs/sample_vicap -mode 0 -dev 0 -sensor 0 -chn 0 -ow 640 -oh 480 -preview 1 -rotation 1
```

!!! tip "Serial connection"
    - **Bigcore (RT-Smart msh)**: `/dev/ttyACM1` at 115200 bps

    ```bash
    minicom -D /dev/ttyACM1 -b 115200
    ```
