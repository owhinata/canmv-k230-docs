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
