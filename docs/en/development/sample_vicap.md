# sample_vicap

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
| `vo_test_case.c` | VO display helpers — layer/OSD creation (`vo_creat_layer_test`, `vo_creat_osd_test`) |
| `vo_test_case.h` | Header for VO helper types (`osd_info`, `layer_info`) and function declarations |

These files are copied from the SDK:

- `sample_vicap.c` from `k230_sdk/src/big/mpp/userapps/sample/sample_vicap/`
- `vo_test_case.c`, `vo_test_case.h` from `k230_sdk/src/big/mpp/userapps/sample/sample_vo/`

## Processing Flow

The application follows this pipeline:

```
Sensor → VI → ISP → Dewarp → [GDMA rotation] → VO → Display
```

See the [K230 VICAP API Reference](https://www.kendryte.com/k230/en/dev/01_software/board/mpp/K230_VICAP_API_Reference.html) for details on the VICAP hardware modules (Sensor, VI, ISP, Dewarp).

### Initialization Sequence

#### 1. Parse arguments

Parses command-line options to configure work mode, connector type, device/channel parameters, and output settings.

**Source:** [`main()` L557–L913][vicap-main-557]

No MPP API calls — uses standard C argument parsing (`strcmp`, `atoi`).

#### 2. Query connector info

Retrieves display connector information to determine the output resolution.

**Source:** [`main()` L921–L928][vicap-main-921]

| API Call | Purpose |
|----------|---------|
| `kd_mpi_get_connector_info()` | Get connector resolution and configuration |

#### 3. Query sensor info

Retrieves sensor capabilities (resolution, format) for the configured sensor type.

**Source:** [`main()` L930–L952][vicap-main-930]

| API Call | Purpose |
|----------|---------|
| `kd_mpi_vicap_get_sensor_info()` | Get sensor resolution and information |

#### 4. Set VICAP device attributes

Configures input window, ISP pipeline controls (AE, AWB, HDR, DNR3), work mode, and optional dewarp. In load-image mode, loads a raw image file into the device.

**Source:** [`main()` L957–L1049][vicap-main-957]

| API Call | Purpose |
|----------|---------|
| `kd_mpi_vicap_set_dev_attr()` | Set device acquisition window, work mode, ISP pipeline |
| `kd_mpi_vicap_load_image()` | Load raw image data (load-image mode only) |

#### 5. Initialize VO connector

Opens and initializes the display connector hardware.

**Source:** [`main()` L1098–L1102][vicap-main-1098] → [`sample_vicap_vo_init()`][vicap-120]

| API Call | Purpose |
|----------|---------|
| `kd_mpi_get_connector_info()` | Get connector details |
| `kd_mpi_connector_open()` | Open connector device |
| `kd_mpi_connector_power_set()` | Enable connector power |
| `kd_mpi_connector_init()` | Initialize connector hardware |

#### 6. Initialize VB (Video Buffer)

Calculates buffer sizes per channel based on pixel format and resolution, then initializes the video buffer pools. Registers `vb_exit()` with `atexit()`.

**Source:** [`main()` L1104–L1109][vicap-main-1104]

- [`sample_vicap_vb_init()`][vicap-258] — calculates buffer pool sizes and initializes VB
- [`vb_exit()`][vicap-518] — registered with `atexit` for cleanup

| API Call | Purpose |
|----------|---------|
| `kd_mpi_vb_set_config()` | Configure buffer pool count and sizes |
| `kd_mpi_vb_set_supplement_config()` | Set JPEG supplement buffer config |
| `kd_mpi_vb_init()` | Initialize video buffer pools |

#### 7. Set VICAP channel attributes

Configures each enabled channel's output window, crop region, pixel format, buffer count, and frame rate.

**Source:** [`main()` L1112–L1168][vicap-main-1112]

| API Call | Purpose |
|----------|---------|
| `kd_mpi_vicap_set_dump_reserved()` | Reserve dump buffer for the channel |
| `kd_mpi_vicap_set_chn_attr()` | Set channel output format, size, crop, buffer |

#### 8. Bind VICAP to VO

Connects VICAP output channels to VO display layers. For rotation values 17–19, a GDMA channel is inserted between VI and VO. Otherwise, VI is bound directly to VO.

**Source:** [`main()` L1170–L1283][vicap-main-1170]

- [`sample_vicap_bind_vo()`][vicap-358] — direct VI-to-VO binding (no GDMA)
- [`dma_dev_attr_init()`][vicap-402] — initializes GDMA device (rotation path)

**Direct binding (no GDMA):**

| API Call | Purpose |
|----------|---------|
| `kd_mpi_sys_bind()` | Bind VI channel to VO channel |

**GDMA rotation path (rotation 17–19):**

| API Call | Purpose |
|----------|---------|
| `kd_mpi_dma_set_dev_attr()` | Configure GDMA device |
| `kd_mpi_dma_start_dev()` | Start GDMA device |
| `kd_mpi_dma_request_chn()` | Request a GDMA channel |
| `kd_mpi_sys_bind()` | Bind VI → GDMA |
| `kd_mpi_sys_bind()` | Bind GDMA → VO |
| `kd_mpi_dma_set_chn_attr()` | Set GDMA channel rotation and format |
| `kd_mpi_dma_start_chn()` | Start GDMA channel |

#### 9. Configure VO layers

Sets up display layers and OSD. Calculates positioning with margins to center layers on screen.

**Source:** [`main()` L1289–L1293][vicap-main-1289]

- [`sample_vicap_vo_layer_init()`][vicap-148] — orchestrates layer/OSD creation
- [`vo_creat_layer_test()`][vo-75] — creates a video layer
- [`vo_creat_osd_test()`][vo-34] — creates an OSD layer

| API Call | Purpose |
|----------|---------|
| `kd_mpi_vo_set_video_layer_attr()` | Set layer size, position, rotation |
| `kd_mpi_vo_enable_video_layer()` | Enable video layer |
| `kd_mpi_vo_set_video_osd_attr()` | Set OSD attributes |
| `kd_mpi_vo_osd_enable()` | Enable OSD layer |

#### 10. Initialize and start VICAP

Initializes each enabled VICAP device and begins frame capture.

**Source:** [`main()` L1295–L1315][vicap-main-1295]

| API Call | Purpose |
|----------|---------|
| `kd_mpi_vicap_init()` | Initialize VICAP device |
| `kd_mpi_vicap_start_stream()` | Start capture stream |

#### 11. Enable VO

Enables the display output.

**Source:** [`main()` L1317][vicap-main-1317] → [`sample_vicap_vo_enable()`][vicap-248]

| API Call | Purpose |
|----------|---------|
| `kd_mpi_vo_enable()` | Enable VO display output |

#### 12. Configure slave mode (optional)

When slave mode is enabled, configures the VICAP slave timing parameters for external sync signal generation.

**Source:** [`main()` L1319–L1333][vicap-main-1319]

| API Call | Purpose |
|----------|---------|
| `kd_mpi_vicap_set_slave_attr()` | Set slave timing (vsync cycle, high period) |
| `kd_mpi_vicap_set_slave_enable()` | Enable slave vsync/hsync output |

### Cleanup Sequence

When the application exits (user presses `q`), resources are released in reverse order:

#### 1. Disable slave mode

**Source:** [`main()` L1604–L1611][vicap-main-1604]

| API Call | Purpose |
|----------|---------|
| `kd_mpi_vicap_set_slave_enable()` | Disable vsync/hsync output |

#### 2. Stop VICAP stream

**Source:** [`main()` L1613–L1621][vicap-main-1613]

| API Call | Purpose |
|----------|---------|
| `kd_mpi_vicap_stop_stream()` | Stop capture stream |

#### 3. Deinitialize VICAP

**Source:** [`main()` L1623–L1627][vicap-main-1623]

| API Call | Purpose |
|----------|---------|
| `kd_mpi_vicap_deinit()` | Deinitialize VICAP device |

#### 4. Disable VO layers

Disables video display layers and OSD overlays.

**Source:** [`main()` L1636–L1670][vicap-main-1636]

- [`sample_vicap_disable_vo_layer()`][vicap-250]
- [`sample_vicap_disable_vo_osd()`][vicap-254]

| API Call | Purpose |
|----------|---------|
| `kd_mpi_vo_disable_video_layer()` | Disable the video layer |
| `kd_mpi_vo_osd_disable()` | Disable the OSD layer |

#### 5. Release GDMA (if used)

**Source:** [`main()` L1671–L1699][vicap-main-1671]

| API Call | Purpose |
|----------|---------|
| `kd_mpi_dma_stop_chn()` | Stop GDMA channel |
| `kd_mpi_sys_unbind()` | Unbind VI → GDMA and GDMA → VO |
| `kd_mpi_dma_release_chn()` | Release GDMA channel |

#### 6. Unbind VI–VO (if no GDMA)

**Source:** [`main()` L1700–L1702][vicap-main-1700] → [`sample_vicap_unbind_vo()`][vicap-380]

| API Call | Purpose |
|----------|---------|
| `kd_mpi_sys_unbind()` | Unbind VI from VO |

#### 7. Stop GDMA device

**Source:** [`main()` L1707–L1712][vicap-main-1707]

| API Call | Purpose |
|----------|---------|
| `kd_mpi_dma_stop_dev()` | Stop GDMA device |

#### 8. Release VB

**Source:** registered via [`atexit()` L1109][vicap-main-1109] → [`vb_exit()`][vicap-518]

| API Call | Purpose |
|----------|---------|
| `kd_mpi_vb_exit()` | Deinitialize VB subsystem |

[vicap-main-557]: https://github.com/owhinata/canmv-k230/blob/13a92a6/apps/sample_vicap/src/sample_vicap.c#L557-L913
[vicap-main-921]: https://github.com/owhinata/canmv-k230/blob/13a92a6/apps/sample_vicap/src/sample_vicap.c#L921-L928
[vicap-main-930]: https://github.com/owhinata/canmv-k230/blob/13a92a6/apps/sample_vicap/src/sample_vicap.c#L930-L952
[vicap-main-957]: https://github.com/owhinata/canmv-k230/blob/13a92a6/apps/sample_vicap/src/sample_vicap.c#L957-L1049
[vicap-main-1098]: https://github.com/owhinata/canmv-k230/blob/13a92a6/apps/sample_vicap/src/sample_vicap.c#L1098-L1102
[vicap-main-1104]: https://github.com/owhinata/canmv-k230/blob/13a92a6/apps/sample_vicap/src/sample_vicap.c#L1104-L1109
[vicap-main-1112]: https://github.com/owhinata/canmv-k230/blob/13a92a6/apps/sample_vicap/src/sample_vicap.c#L1112-L1168
[vicap-main-1170]: https://github.com/owhinata/canmv-k230/blob/13a92a6/apps/sample_vicap/src/sample_vicap.c#L1170-L1283
[vicap-main-1289]: https://github.com/owhinata/canmv-k230/blob/13a92a6/apps/sample_vicap/src/sample_vicap.c#L1289-L1293
[vicap-main-1295]: https://github.com/owhinata/canmv-k230/blob/13a92a6/apps/sample_vicap/src/sample_vicap.c#L1295-L1315
[vicap-main-1317]: https://github.com/owhinata/canmv-k230/blob/13a92a6/apps/sample_vicap/src/sample_vicap.c#L1317
[vicap-main-1319]: https://github.com/owhinata/canmv-k230/blob/13a92a6/apps/sample_vicap/src/sample_vicap.c#L1319-L1333
[vicap-main-1604]: https://github.com/owhinata/canmv-k230/blob/13a92a6/apps/sample_vicap/src/sample_vicap.c#L1604-L1611
[vicap-main-1613]: https://github.com/owhinata/canmv-k230/blob/13a92a6/apps/sample_vicap/src/sample_vicap.c#L1613-L1621
[vicap-main-1623]: https://github.com/owhinata/canmv-k230/blob/13a92a6/apps/sample_vicap/src/sample_vicap.c#L1623-L1627
[vicap-main-1636]: https://github.com/owhinata/canmv-k230/blob/13a92a6/apps/sample_vicap/src/sample_vicap.c#L1636-L1670
[vicap-main-1671]: https://github.com/owhinata/canmv-k230/blob/13a92a6/apps/sample_vicap/src/sample_vicap.c#L1671-L1699
[vicap-main-1700]: https://github.com/owhinata/canmv-k230/blob/13a92a6/apps/sample_vicap/src/sample_vicap.c#L1700-L1702
[vicap-main-1707]: https://github.com/owhinata/canmv-k230/blob/13a92a6/apps/sample_vicap/src/sample_vicap.c#L1707-L1712
[vicap-main-1109]: https://github.com/owhinata/canmv-k230/blob/13a92a6/apps/sample_vicap/src/sample_vicap.c#L1109
[vicap-120]: https://github.com/owhinata/canmv-k230/blob/13a92a6/apps/sample_vicap/src/sample_vicap.c#L120-L146
[vicap-148]: https://github.com/owhinata/canmv-k230/blob/13a92a6/apps/sample_vicap/src/sample_vicap.c#L148-L246
[vicap-248]: https://github.com/owhinata/canmv-k230/blob/13a92a6/apps/sample_vicap/src/sample_vicap.c#L248
[vicap-250]: https://github.com/owhinata/canmv-k230/blob/13a92a6/apps/sample_vicap/src/sample_vicap.c#L250-L252
[vicap-254]: https://github.com/owhinata/canmv-k230/blob/13a92a6/apps/sample_vicap/src/sample_vicap.c#L254-L256
[vicap-258]: https://github.com/owhinata/canmv-k230/blob/13a92a6/apps/sample_vicap/src/sample_vicap.c#L258-L356
[vicap-358]: https://github.com/owhinata/canmv-k230/blob/13a92a6/apps/sample_vicap/src/sample_vicap.c#L358-L378
[vicap-380]: https://github.com/owhinata/canmv-k230/blob/13a92a6/apps/sample_vicap/src/sample_vicap.c#L380-L400
[vicap-402]: https://github.com/owhinata/canmv-k230/blob/13a92a6/apps/sample_vicap/src/sample_vicap.c#L402-L422
[vicap-518]: https://github.com/owhinata/canmv-k230/blob/13a92a6/apps/sample_vicap/src/sample_vicap.c#L518
[vo-34]: https://github.com/owhinata/canmv-k230/blob/13a92a6/apps/sample_vicap/src/vo_test_case.c#L34-L73
[vo-75]: https://github.com/owhinata/canmv-k230/blob/13a92a6/apps/sample_vicap/src/vo_test_case.c#L75-L115

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
| `-conn <n>` | Connector type (see [`-conn` Details](#-conn-details)) | 0 |

### Per-Device Options (after `-dev <n>`)

| Option | Description | Default |
|--------|-------------|---------|
| `-dev <n>` | VICAP device ID (0, 1, 2) | 0 |
| `-sensor <n>` | Sensor type (see [`-sensor` (OV5647) Details](#-sensor-ov5647-details)) | — |
| `-ae <0\|1>` | Enable/disable AE | 1 |
| `-awb <0\|1>` | Enable/disable AWB | 1 |
| `-hdr <0\|1>` | Enable/disable HDR | 0 |
| `-dw <0\|1>` | Enable dewarp | 0 |
| `-mirror <n>` | Mirror: 0=none, 1=horizontal, 2=vertical, 3=both | 0 |

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

### Examples

```bash
# OV5647 1080p30 (CSI0) → HDMI 1080p60, vertical mirror
./sample_vicap -mode 0 -conn 1 -dev 0 -sensor 24 -chn 0 -mirror 2

# OV5647 720p60 (CSI0) → HDMI 1080p60, vertical mirror
./sample_vicap -mode 0 -conn 1 -dev 0 -sensor 44 -chn 0 -mirror 2

# OV5647 720p60 (CSI0) → HDMI 720p60, vertical mirror
./sample_vicap -mode 0 -conn 5 -dev 0 -sensor 44 -chn 0 -mirror 2
```

### `-conn` Details

| Value | Chip | Interface | Resolution | FPS |
|-------|------|-----------|------------|-----|
| 0 | HX8399 | MIPI DSI 4-lane LCD | 1080x1920 | 30 |
| 1 | LT9611 | HDMI (MIPI-to-HDMI bridge) | 1920x1080 | 60 |
| 2 | LT9611 | HDMI (MIPI-to-HDMI bridge) | 1920x1080 | 30 |
| 3 | ST7701 | MIPI DSI 2-lane LCD | 480x800 | 30 |
| 4 | ILI9806 | MIPI DSI 2-lane LCD | 480x800 | 30 |
| 5 | LT9611 | HDMI (MIPI-to-HDMI bridge) | 1280x720 | 60 |
| 6 | LT9611 | HDMI (MIPI-to-HDMI bridge) | 1280x720 | 30 |
| 7 | LT9611 | HDMI (MIPI-to-HDMI bridge) | 640x480 | 60 |

### `-sensor` (OV5647) Details

All modes use 10-bit Linear, MIPI CSI 2-lane.

| Value | Resolution | FPS | CSI Port | Notes |
|-------|------------|-----|----------|-------|
| 21 | 1920x1080 | 30 | — | Legacy (no CSI port specified) |
| 22 | 2592x1944 | 10 | — | Full resolution, legacy |
| 23 | 640x480 | 90 | — | Legacy |
| 24 | 1920x1080 | 30 | CSI0 | |
| 27 | 1920x1080 | 30 | CSI1 | |
| 28 | 1920x1080 | 30 | CSI2 | |
| 37 | 1920x1080 | 30 | CSI0 | V2 |
| 38 | 1920x1080 | 30 | CSI1 | V2 |
| 39 | 1920x1080 | 30 | CSI2 | V2 |
| 41 | 640x480 | 90 | CSI1 | |
| 42 | 1280x720 | 60 | CSI1 | |
| 43 | 1280x960 | 45 | CSI1 | |
| 44 | 1280x720 | 60 | CSI0 | |
| 45 | 1280x960 | 45 | CSI0 | |
| 46 | 640x480 | 90 | CSI2 | |
| 47 | 1280x720 | 60 | CSI2 | |
| 48 | 1280x960 | 45 | CSI2 | |

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

### Run on the K230 bigcore (msh)

On the K230 serial console (ACM1), run:

```
msh /> /sharefs/sample_vicap -mode 0 -conn 1 -dev 0 -sensor 24 -chn 0 -mirror 2
```

!!! tip "Serial connection"
    - **Bigcore (RT-Smart msh)**: `/dev/ttyACM1` at 115200 bps

    ```bash
    minicom -D /dev/ttyACM1 -b 115200
    ```
