# Bigcore Software Architecture (RT-Smart / MPP)

This page describes the architecture of RT-Smart (real-time OS) and MPP (Media Processing Platform) running on the K230 bigcore.

## Overview

On the K230 bigcore, RT-Smart serves as the kernel (OS) and MPP controls the multimedia hardware on top of it.

```
┌─────────────────────────────────────────────────────┐
│              User Space (LWP Processes)              │
│  ┌──────────┐  ┌──────────┐  ┌───────────────────┐  │
│  │ MPP      │  │ Middle-  │  │ User Apps         │  │
│  │ MPI API  │  │ ware     │  │ (sample_*.elf)    │  │
│  │ (mpi_*)  │  │ (FFmpeg, │  │                   │  │
│  │          │  │  RTSP,..)│  │                   │  │
│  └────┬─────┘  └────┬─────┘  └───────────────────┘  │
│───────┼──────────────┼───────────────────────────────│
│       │ Kernel Space │                               │
│  ┌────┴─────┐  ┌─────┴──────────────────────────┐    │
│  │ MPP      │  │ RT-Smart Kernel               │    │
│  │ Kernel   │  │ (Scheduler, IPC, DFS,         │    │
│  │ Modules  │  │  Memory Mgmt, LWP, Drivers)   │    │
│  └──────────┘  └────────────────────────────────┘    │
│───────────────────────────────────────────────────────│
│                K230 SoC Hardware                      │
│  RISC-V C908 / ISP / VPU / GPU / DPU / Audio / FFT   │
└─────────────────────────────────────────────────────┘
```

| Component | Role |
|-----------|------|
| **RT-Smart** | Real-time OS providing process management, memory protection, device drivers, and file systems |
| **MPP** | Library suite controlling K230 SoC multimedia hardware (ISP, VPU, GPU, DPU, Audio, etc.) |

## RT-Smart (Real-Time OS)

### What is RT-Smart

RT-Smart is an extended version of [RT-Thread](https://www.rt-thread.io/) that supports user-space processes (LWP: Lightweight Process) using the MMU for memory protection.

- **LWP (Lightweight Process)**: Separates kernel and user space with memory protection
- **200+ system calls**: POSIX-compatible via musl libc
- **Toolchain**: `riscv64-unknown-linux-musl-gcc`

!!! info "RT-Thread vs RT-Smart"
    RT-Thread is an RTOS for microcontrollers. RT-Smart extends it with MMU-based process isolation.
    The K230 bigcore uses RT-Smart.

### Directory Structure

RT-Smart source code is located under `k230_sdk/src/big/rt-smart/`.

| Path | Contents |
|------|----------|
| `kernel/rt-thread/src/` | Kernel core (scheduler, IPC, timers, etc.) |
| `kernel/rt-thread/components/` | Kernel components (DFS, finsh, lwp, drivers, net, etc.) |
| `kernel/rt-thread/libcpu/` | CPU architecture-dependent code (including RISC-V) |
| `kernel/bsp/maix3/` | K230 board support package (BSP) |
| `userapps/` | User-space applications, SDK, and test cases |
| `init.sh` | Boot script source |

### Key Kernel Components

Major components under `kernel/rt-thread/components/`:

| Component | Description |
|-----------|-------------|
| `lwp/` | Lightweight Process — user-space process management |
| `dfs/` | Distributed File System — multiple filesystem support (ROMFS, FAT, JFFS2, tmpfs, etc.) |
| `finsh/` | msh shell — console interface |
| `drivers/` | Device driver framework |
| `net/` | Network stack |
| `libc/` | C library support |
| `cplusplus/` | C++ support (Thread, Mutex, Semaphore, etc.) |

### K230 BSP (`kernel/bsp/maix3/`)

K230-specific drivers and board configuration are part of the BSP.

#### Internal Drivers (`board/interdrv/`)

Drivers for SoC built-in peripherals:

| Driver | Target |
|--------|--------|
| `uart/`, `uart_canaan/` | UART serial communication |
| `i2c/` | I2C bus |
| `spi/` | SPI bus |
| `gpio/` | GPIO pin control |
| `pwm/` | PWM output |
| `adc/` | ADC (analog-to-digital converter) |
| `hwtimer/` | Hardware timer |
| `wdt/` | Watchdog timer |
| `rtc/` | Real-time clock |
| `sdio/` | SDIO (SD cards, etc.) |
| `cipher/` | Cryptographic engine |
| `gnne/` | Neural network engine |
| `hardlock/` | Hardware lock |
| `pdma/` | DMA controller |
| `sysctl/` | System control |
| `tsensor/` | Temperature sensor |

#### External Drivers (`board/extdrv/`)

Drivers for externally connected devices:

| Driver | Target |
|--------|--------|
| `cyw43xx/` | CYW43 WiFi/Bluetooth module |
| `realtek/` | Realtek WiFi drivers |
| `nand/` | NAND flash |
| `eeprom/` | EEPROM |
| `touch/` | Touch panel |
| `regulator/` | Voltage regulator |

#### Inter-Processor Communication (`board/ipcm/`)

Communication between the bigcore (RT-Smart) and littlecore (Linux):

| File | Function |
|------|----------|
| `sharefs_init.c` | sharefs — file sharing between cores |
| `virt_tty_init.c` | Virtual TTY — console forwarding between cores |
| `rtt_ctrl_init.c` | RT-Thread control initialization |

#### USB Support (`board/extcomponents/CherryUSB/`)

USB device/host support via the CherryUSB stack. Supports CDC, HID, MSC, Audio, Video, and other USB classes.

### Build System

The RT-Smart kernel is built with **SCons + Kconfig**.

| Item | Value |
|------|-------|
| Build tool | SCons |
| Configuration | Kconfig (`rtconfig.h`) |
| Toolchain | `riscv64-unknown-linux-musl-gcc` |
| Kernel config file | `kernel/bsp/maix3/rtconfig.h` |

!!! note "Build instructions"
    For overall SDK build instructions, see [SDK Build](sdk_build.md).
    For RT-Smart kernel partial builds, see [RT-Smart Boot Customization](rtsmart_boot.md).

## MPP (Media Processing Platform)

### What is MPP

MPP is a suite of libraries for controlling the K230 SoC multimedia hardware. It provides camera input, video encoding/decoding, display output, audio processing, AI inference preprocessing, and more.

MPP has a 3-layer architecture:

```
User Application
    ↓ (MPI API calls)
MPI User-Space Libraries (mpi_*_api)
    ↓ (ioctl / system calls)
MPP Kernel Modules
    ↓
K230 Hardware
```

| Layer | Location | Description |
|-------|----------|-------------|
| Kernel modules | `mpp/kernel/` | Drivers with direct hardware access |
| MPI API | `mpp/userapps/api/` | User-space C API (`mpi_vicap_api.h`, etc.) |
| Middleware | `mpp/middleware/` | High-level libraries (FFmpeg, RTSP, MP4, etc.) |

### Directory Structure

MPP source code is located under `k230_sdk/src/big/mpp/`.

| Path | Contents |
|------|----------|
| `kernel/` | Kernel-space drivers (sensor, connector, FFT, GPU, PM, etc.) |
| `kernel/lib/` | Pre-compiled kernel libraries (21) |
| `userapps/api/` | MPI API headers (25) |
| `userapps/lib/` | Pre-compiled user-space libraries (50+) |
| `userapps/sample/` | Sample applications (55+) |
| `userapps/src/` | User-space library source code |
| `include/` | Public headers (type definitions, error codes, common module definitions) |
| `middleware/` | Multimedia middleware (FFmpeg, Live555, etc.) |

### Module List

| Module | MPI API | Function |
|--------|---------|----------|
| **VICAP** | `mpi_vicap_api.h` | Video capture (camera input) |
| **ISP** | `mpi_isp_api.h` | Image signal processing |
| **VENC** | `mpi_venc_api.h` | Video encoding (H.264/H.265) |
| **VDEC** | `mpi_vdec_api.h` | Video decoding |
| **VO** | `mpi_vo_api.h` | Video output |
| **AI** | `mpi_ai_api.h` | Audio input |
| **AO** | `mpi_ao_api.h` | Audio output |
| **AENC** | `mpi_aenc_api.h` | Audio encoding |
| **ADEC** | `mpi_adec_api.h` | Audio decoding |
| **DPU** | `mpi_dpu_api.h` | Depth processing unit |
| **DMA** | `mpi_dma_api.h` | DMA transfer |
| **GPU** | `vg_lite.h` | 2D graphics (VGLite) |
| **FFT** | `mpi_fft_api.h` | FFT accelerator |
| **PM** | `mpi_pm_api.h` | Power management |
| **Sensor** | `mpi_sensor_api.h` | Image sensor control |
| **Connector** | `mpi_connector_api.h` | Display connector (LCD/HDMI) |
| **VDSS** | `mpi_vdss_api.h` | Video subsystem |
| **NonAI 2D** | `mpi_nonai_2d_api.h` | Non-AI 2D image processing |
| **Dewarp** | `mpi_dewarp_api.h` | Lens distortion correction |
| **Cipher** | `mpi_cipher_api.h` | Cryptographic operations |

#### Supported Image Sensors

The following sensor drivers are included in `mpp/kernel/sensor/src/`:

GC2053, GC2093, IMX335, OS08A20, OV5647, OV9286, OV9732, SC035HGS, SC132GS, SC201CS, etc.

#### Supported Display Connectors

The following connector drivers are included in `mpp/kernel/connector/src/`:

HX8399, ILI9806, LT9611 (HDMI), NT35516, ST7701, etc.

### Middleware

High-level multimedia libraries under `mpp/middleware/`:

| Library | Description |
|---------|-------------|
| **FFmpeg** | Multimedia framework (encoding/decoding/transcoding) |
| **Live555** | RTSP/RTP streaming library |
| **x264** | H.264 software encoder |
| **kdmedia** | Canaan custom media framework |
| **mp4_format** | MP4 file format handling |
| **rtsp_server** | RTSP server implementation |
| **rtsp_client** | RTSP client implementation |
| **rtsp_pusher** | RTSP stream publishing |
| **mp4_player** | MP4 player implementation |

### Sample Applications

Numerous samples are provided under `mpp/userapps/sample/` and `mpp/middleware/sample/`.

**Video:**

- `sample_vicap.elf` — Camera capture
- `sample_venc.elf` — Video encoding
- `sample_vdec.elf` — Video decoding
- `sample_vo.elf` — Video output

**Audio:**

- `sample_audio.elf` — Audio input/output
- `sample_av.elf` — Integrated audio/video

**AI / Image Processing:**

- `sample_face_detect.elf` — Face detection
- `sample_face_ae.elf` — Face recognition + auto exposure
- `sample_dpu.elf` — Depth processing
- `sample_dpu_vicap.elf` — Depth processing + camera input

**Graphics:**

- `sample_gpu_cube.elf` — GPU 3D rendering
- `sample_lvgl.elf` — LVGL GUI framework

**Streaming (middleware):**

- `sample_rtspserver` — RTSP server
- `sample_rtspclient` — RTSP client
- `sample_player` — Media player
- `sample_muxer` / `sample_demuxer` — Media muxing/demuxing

**Peripherals:**

- `sample_gpio.elf`, `sample_pwm.elf`, `sample_adc.elf`, `sample_i2c_slave.elf`, `sample_hwtimer.elf`, etc.

## Relationship Between RT-Smart and MPP

MPP is a library suite that runs on top of RT-Smart, spanning both kernel space and user space.

```
┌──────────────────────────────────────────────────┐
│  User Space (RT-Smart LWP Processes)             │
│                                                  │
│  Application                                     │
│    ↓                                             │
│  MPI API (mpi_vicap_api, mpi_venc_api, ...)      │
│    ↓ ioctl                                       │
│══════════════════════════════════════════════════│
│  Kernel Space                                    │
│                                                  │
│  MPP Kernel Modules                              │
│  (VICAP, VPU, ISP, VO, AI/AO, DPU, FFT, GPU)    │
│    ↓                                             │
│  RT-Smart Kernel                                 │
│  (Scheduler, Memory Mgmt, Driver FW, DFS, IPC)   │
│    ↓                                             │
│  K230 SoC Hardware                               │
└──────────────────────────────────────────────────┘
```

- **MPP kernel modules** run in RT-Smart kernel space and directly access the SoC multimedia hardware
- **MPI API** is a user-space library that controls kernel modules via ioctl
- User applications run as **LWP processes** and call the MPI API to use multimedia features
- RT-Smart provides memory protection (MMU), preventing user-space bugs from corrupting the kernel

!!! note "Related documents"
    - [SDK Build](sdk_build.md) — How to build the K230 SDK
    - [RT-Smart Boot Customization](rtsmart_boot.md) — Boot sequence and init.sh modification
    - [Hello World](hello_world.md) — Building applications for bigcore / littlecore
