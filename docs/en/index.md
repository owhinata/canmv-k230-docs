# CanMV K230 Guide

A setup and usage guide for the CanMV K230 v1.1 board.

## Board Overview

| Item | Value |
|------|-------|
| Board | CanMV K230 v1.1 |
| SoC | Kendryte K230 (RISC-V dual-core) |
| OS | Linux canaan 5.10.4 (riscv64) |
| WiFi Chip | Broadcom (bcmdhd driver, 2.4GHz only) |
| Serial | USB via `/dev/ttyACM0` at 115200 baud |

## Links

### Official Documentation & SDK

- [K230 SDK (GitHub)](https://github.com/kendryte/k230_sdk) -- Linux + RT-Smart dual OS SDK
- [K230 Docs (GitHub)](https://github.com/kendryte/k230_docs) -- SDK reference documentation
- [K230 SDK Docs (Web)](https://www.kendryte.com/k230/en/dev/index.html) -- Hardware design guide, datasheet, API reference
- [K230 Linux SDK (GitHub)](https://github.com/kendryte/k230_linux_sdk) -- Linux-only SDK (Debian / Ubuntu image support)

### CanMV (MicroPython)

- [CanMV K230 Firmware (GitHub)](https://github.com/kendryte/canmv_k230) -- MicroPython firmware (download images from the releases page)
- [CanMV K230 Docs (Web)](https://www.kendryte.com/k230_canmv/en/main/index.html) -- MicroPython API reference and examples

### AI Development

- [nncase (GitHub)](https://github.com/kendryte/nncase) -- Compiler to convert ONNX / TFLite models to kmodel for KPU
- [K230 AI Development Tutorial](https://www.kendryte.com/ai_docs/en/dev/Development_Basics.html) -- Model inference, AI2D preprocessing, and deployment guide
- [K230 Training Scripts (GitHub)](https://github.com/kendryte/K230_training_scripts) -- End-to-end samples from model training to on-board inference

### Hardware

- [K230 Product Page](https://www.kendryte.com/en/proDetail/230) -- Schematics, PCB data, IOMUX tool

### Firmware Downloads

- [Canaan Firmware Index](https://kendryte-download.canaan-creative.com/developer/k230/) -- Pre-built images for CanMV, Debian, Ubuntu, etc.

## Preparing the OS Image

### Download

Download the SD card image from the following URL:

```
https://kendryte-download.canaan-creative.com/k230/release/sdk_images/v2.0/k230_canmv_defconfig/CanMV-K230_sdcard_v2.0_nncase_v2.10.0.img.gz
```

```sh
wget https://kendryte-download.canaan-creative.com/k230/release/sdk_images/v2.0/k230_canmv_defconfig/CanMV-K230_sdcard_v2.0_nncase_v2.10.0.img.gz
gunzip CanMV-K230_sdcard_v2.0_nncase_v2.10.0.img.gz
```

### Writing to SD Card

!!! warning "Check the device name"
    The device name for `of=` varies by environment (`/dev/sda`, `/dev/sdb`, `/dev/mmcblk0`, etc.).
    **Writing to the wrong device will destroy data.** Verify with `lsblk` before proceeding.

```sh
sudo dd if=CanMV-K230_sdcard_v2.0_nncase_v2.10.0.img of=/dev/sdX bs=1M oflag=sync
```

## Booting the Board

1. Insert the prepared SD card into the K230
2. Connect to the host PC via USB cable
3. The board automatically boots Linux on power-up

## Serial Connection

When connected via USB, `/dev/ttyACM0` appears on the host PC.
Use `picocom` to access the serial console:

```sh
picocom -b 115200 /dev/ttyACM0
```

!!! tip "Installing picocom"
    On Debian/Ubuntu: `sudo apt install picocom`

!!! info "Exiting picocom"
    Press `Ctrl-a Ctrl-x` to exit picocom.

If a login prompt appears, log in as `root` (no password).
