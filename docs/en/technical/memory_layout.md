# K230 DDR Memory Layout

## 1. Overview

The K230 is a dual-core RISC-V SoC with 512 MB DDR (0x00000000 - 0x1FFFFFFF). The memory is partitioned among three major consumers:

- **RT-Smart** (Big Core, C908) - real-time OS with MPP media framework
- **Linux** (Little Core, C908) - general-purpose Linux with rootfs
- **MMZ** (Media Memory Zone) - shared media buffer pool managed by MPP

The memory boundaries are defined at build time in `k230_sdk/src/big/mpp/include/comm/k_autoconf_comm.h` and cannot be changed at runtime.

## 2. DDR Physical Memory Map

```
 Physical Address        Size      Region
 ========================================================================
 0x00000000 +---------+
            | Reserved|  1 MB     SoC reserved (boot ROM scratch area)
 0x00100000 +---------+
            |  IPCM   |  1 MB     Inter-core communication (shared memory)
 0x00200000 +---------+
            |         |
            | RT-Smart| 126 MB    Big Core OS (kernel + heap + pages)
            |         |
 0x08000000 +---------+
            |         |
            |  Linux  | 128 MB    Little Core OS (OpenSBI + kernel)
            |         |
 0x10000000 +---------+
            |         |
            |   MMZ   | ~252 MB   Media Memory Zone (MPP buffers)
            |         |
 0x1FBFF000 +---------+
            |  Guard  | ~4 MB     Tail guard / unused
 0x20000000 +---------+
            Total: 512 MB
```

### Source: `k_autoconf_comm.h`

| Define | Value | Meaning |
|--------|-------|---------|
| `CONFIG_MEM_TOTAL_SIZE` | `0x20000000` | 512 MB total DDR |
| `CONFIG_MEM_IPCM_BASE` | `0x00100000` | IPCM region start |
| `CONFIG_MEM_IPCM_SIZE` | `0x00100000` | 1 MB |
| `CONFIG_MEM_RTT_SYS_BASE` | `0x00200000` | RT-Smart region start |
| `CONFIG_MEM_RTT_SYS_SIZE` | `0x07E00000` | 126 MB |
| `CONFIG_MEM_LINUX_SYS_BASE` | `0x08000000` | Linux region start |
| `CONFIG_MEM_LINUX_SYS_SIZE` | `0x08000000` | 128 MB |
| `CONFIG_MEM_MMZ_BASE` | `0x10000000` | MMZ region start |
| `CONFIG_MEM_MMZ_SIZE` | `0x0FC00000` | 252 MB (config value) |
| `CONFIG_MEM_BOUNDARY_RESERVED_SIZE` | `0x00001000` | 4 KB boundary guard |

## 3. IPCM (Inter-Processor Communication) Shared Memory

The 1 MB IPCM region at 0x00100000 is subdivided as follows:

```
 0x00100000 +-----------+
            | RTT->Linux|  512 KB   Shared memory (node 1 -> node 0)
 0x00180000 +-----------+
            | Linux->RTT|  484 KB   Shared memory (node 0 -> node 1)
 0x001F9000 +-----------+
            | Virt TTY  |   16 KB   Virtual TTY (serial over IPCM)
 0x001FD000 +-----------+
            | Reserved  |   12 KB   IPCM node descriptor + guard
 0x00200000 +-----------+
```

### Source: IPCM Config Files

**RT-Smart side** (`k230_riscv_rtsmart_config`):
```
node_id=1               # RT-Smart is node 1
top_role="slave"
shm_phys_1to0=0x100000  # RTT(1)->Linux(0) base
shm_size_1to0=0x80000   # 512 KB
shm_phys_0to1=0x180000  # Linux(0)->RTT(1) base
shm_size_0to1=0x79000   # 484 KB
virt_tty_phys=0x1f9000  # Virtual TTY base
virt_tty_size=0x4000    # 16 KB
```

**Linux side** (`k230_riscv_linux_config`):
```
node_id=0               # Linux is node 0
top_role="master"
virt_tty_role="server"
```

The Linux side does not configure SHM addresses directly; the RT-Smart (slave) side owns the physical address definitions, and the Linux kernel IPCM driver reads them at runtime via the IPCM protocol.

## 4. RT-Smart Internal Memory Layout

RT-Smart occupies 0x00200000 - 0x07FFFFFF (126 MB). Its internal structure is:

```
 0x00200000 +-----------+
            | Bootloader|  128 KB   U-Boot loads RT-Smart image here
 0x00220000 +-----------+
            | .text     |           Kernel code (linked at 0x220000)
            | .rodata   |
            | .data     |
            | .bss      |
   __bss_end +-----------+
            |           |
            | Kernel    |  32 MB    RT_HW_HEAP (rt_malloc pool)
            | Heap      |
            |           |
  +0x2000000 +-----------+
            |           |
            | Page      |  ~93 MB   Page allocator (for LWP user processes)
            | Allocator |
            |           |
 0x07FFF000 +-----------+
            | Reserved  |  4 KB     Boundary guard (MEMORY_RESERVED)
 0x08000000 +-----------+
```

### Source: `board.h`

```c
#define MEMORY_RESERVED     0x1000
#define RAM_END             0x7fff000

#define RT_HW_HEAP_BEGIN    ((void *)&__bss_end)
#define RT_HW_HEAP_END      ((void *)(((rt_size_t)RT_HW_HEAP_BEGIN) + 0x2000000))

#define RT_HW_PAGE_START    ((void *)((rt_size_t)RT_HW_HEAP_END + sizeof(rt_size_t)))
#define RT_HW_PAGE_END      ((void *)(RAM_END))
```

### Source: `link.lds`

```
MEMORY
{
   SRAM : ORIGIN = 0x220000, LENGTH = 128895K
}
/* 0x00200000 - 0x00220000: Bootloader */
/* 0x00220000 - 0x08000000: Kernel     */
```

## 5. Linux Internal Memory Layout

Linux occupies 0x08000000 - 0x0FFFFFFF (128 MB). The breakdown is:

```
 0x08000000 +-----------+
            | OpenSBI   |  2 MB     SBI firmware (supervisor binary interface)
 0x08200000 +-----------+
            | Linux     | ~126 MB   Kernel code + data + user memory
            | Kernel    |           DTS declares: reg = <0x8200000 0x7dff000>
            |           |
            |  0x0A000000: DTB placement address
            |  0x0A100000: initrd placement address
            |  0x0C800000: CMA region (52 MB)
            |           |
 0x0FFFF000 +-----------+
            | Guard     |  4 KB     Boundary
 0x10000000 +-----------+
```

### Source: `k230_canmv.dts`

```dts
&ddr {
    reg = <0x0 0x8200000 0x0 0x7dff000>;  /* 132,116,480 bytes = ~126 MB */
};

chosen {
    linux,initrd-start = <0x0 0xa100000>;
};
```

### Source: `k230_img.c`

```c
#define OPENSBI_DTB_ADDR  (CONFIG_MEM_LINUX_SYS_BASE + 0x2000000)   /* 0x0A000000 */
#define RAMDISK_ADDR      (CONFIG_MEM_LINUX_SYS_BASE + 0x2000000 + 0x100000)  /* 0x0A100000 */
```

U-Boot loads the Linux uImage containing kernel + DTB + initrd, decompresses the kernel to its load address, then copies DTB to 0x0A000000 and initrd to 0x0A100000 before jumping into OpenSBI/kernel.

## 6. MMZ (Media Memory Zone)

MMZ is the large contiguous buffer pool managed by RT-Smart's MPP framework for media operations (camera capture, video encode/decode, display, AI inference, etc.).

```
 0x10000000 +-----------+
            |           |
            |    MMZ    |  ~252 MB   Media buffers (VB pools, DMA buffers)
            |           |           Managed by mmz_init() in mpp_init.c
            |           |
 0x1FBFF000 +-----------+
            | Tail Guard|  ~4 MB    Unused / boundary protection
 0x20000000 +-----------+
```

### Source: `mpp_init.c`

```c
#define MEM_MMZ_BASE 0x10000000UL
#define MEM_MMZ_SIZE 0xfbff000UL    /* 252 MB - 4 KB (boundary subtracted) */

ret = mmz_init(MEM_MMZ_BASE, MEM_MMZ_SIZE);
```

Note: `CONFIG_MEM_MMZ_SIZE` in k_autoconf_comm.h is `0x0FC00000` (exactly 252 MB). The actual initialization in `mpp_init.c` uses `0x0FBFF000` (252 MB - 4 KB), subtracting `CONFIG_MEM_BOUNDARY_RESERVED_SIZE` (4 KB) as a safety guard.

MMZ memory is allocated via the VB (Video Buffer) subsystem and is accessible from both RT-Smart user processes (via `/dev/mmz_userdev`) and kernel-space MPP drivers.

## 7. MMIO / Peripheral Address Map

Peripherals are mapped above 0x80000000 (outside DDR space). From `board.h` and the K230 Technical Reference Manual:

| Address Range | Size | Peripheral |
|--------------|------|------------|
| `0x80000000 - 0x801FFFFF` | 2 MB | KPU L2 Cache |
| `0x80200000 - 0x803FFFFF` | 2 MB | SRAM |
| `0x80400000 - 0x804007FF` | 2 KB | KPU Configuration |
| `0x80400800 - 0x80400BFF` | 1 KB | FFT |
| `0x80400C00 - 0x80400FFF` | 1 KB | AI 2D Engine |
| `0x80800000 - 0x80803FFF` | 16 KB | GSDMA |
| `0x80804000 - 0x80807FFF` | 16 KB | DMA |
| `0x80808000 - 0x8080BFFF` | 16 KB | GZIP Decompress |
| `0x8080C000 - 0x8080FFFF` | 16 KB | Non-AI 2D |
| `0x90000000 - 0x90007FFF` | 32 KB | ISP |
| `0x90008000 - 0x90008FFF` | 4 KB | DeWarp |
| `0x90009000 - 0x9000AFFF` | 8 KB | RX CSI |
| `0x90400000 - 0x9040FFFF` | 64 KB | H264/HEVC/JPEG Codec |
| `0x90800000 - 0x9083FFFF` | 256 KB | 2.5D GPU |
| `0x90840000 - 0x9084FFFF` | 64 KB | VO (Video Output) |
| `0x90850000 - 0x90850FFF` | 4 KB | DSI |
| `0x90A00000 - 0x90A007FF` | 2 KB | 3D Engine |
| `0x91000000 - 0x91000BFF` | 3 KB | PMU |
| `0x91000C00 - 0x91000FFF` | 1 KB | RTC |
| `0x91100000 - 0x91100FFF` | 4 KB | CMU (Clock) |
| `0x91101000 - 0x91101FFF` | 4 KB | RMU (Reset) |
| `0x91102000 - 0x91102FFF` | 4 KB | BOOT Control |
| `0x91103000 - 0x91103FFF` | 4 KB | PWR (Power) |
| `0x91104000 - 0x91104FFF` | 4 KB | Mailbox |
| `0x91105000 - 0x911057FF` | 2 KB | IOMUX |
| `0x91105800 - 0x91105FFF` | 2 KB | Hardware Timer |
| `0x91106000 - 0x911067FF` | 2 KB | WDT0 |
| `0x91106800 - 0x91106FFF` | 2 KB | WDT1 |
| `0x91107000 - 0x911077FF` | 2 KB | Temperature Sensor |
| `0x91107800 - 0x91107FFF` | 2 KB | HDI |
| `0x91108000 - 0x91108FFF` | 4 KB | STC (System Timer) |
| `0x91200000 - 0x9120FFFF` | 64 KB | Boot ROM |
| `0x91210000 - 0x91217FFF` | 32 KB | Security |
| `0x91400000 - 0x91404FFF` | 20 KB | UART0-4 (4 KB each) |
| `0x91405000 - 0x91409FFF` | 20 KB | I2C0-4 (4 KB each) |
| `0x9140A000 - 0x9140AFFF` | 4 KB | PWM |
| `0x9140B000 - 0x9140CFFF` | 8 KB | GPIO0-1 (4 KB each) |
| `0x9140D000 - 0x9140DFFF` | 4 KB | ADC |
| `0x9140E000 - 0x9140EFFF` | 4 KB | Audio CODEC |
| `0x9140F000 - 0x9140FFFF` | 4 KB | I2S Audio |
| `0x91500000 - 0x9157FFFF` | 512 KB | USB 2.0 OTG x2 |
| `0x91580000 - 0x91581FFF` | 8 KB | SD/MMC HC x2 |
| `0x91582000 - 0x91583FFF` | 8 KB | SPI QSPI x2 |
| `0x91584000 - 0x91584FFF` | 4 KB | SPI OPI |
| `0x91585000 - 0x915853FF` | 1 KB | HI SYS Config |
| `0x98000000 - 0x99FFFFFF` | 32 MB | DDRC Configuration |
| `0xC0000000 - 0xC7FFFFFF` | 128 MB | SPI OPI XIP Flash |

## 8. SD Card Partition Layout

The SD card image is generated by `genimage-sdcard.cfg` with GPT partitioning:

```
 Offset      Size     Content                  Partition
 ======================================================================
   1 MB      512 KB   U-Boot SPL (copy 1)      (raw, not in GPT)
   1.5 MB    512 KB   U-Boot SPL (copy 2)      (raw, not in GPT)
   ~1.875 MB 128 KB   U-Boot env               (raw, not in GPT)
   2 MB      1.5 MB   U-Boot proper            (raw, not in GPT)
  10 MB       20 MB   RT-Smart firmware         GPT partition "rtt"
  30 MB       50 MB   Linux firmware            GPT partition "linux"
 128 MB       var.    rootfs (ext4)             GPT partition "rootfs"
   after rootfs        sharefs (FAT32, 256 MB)  GPT partition "fat32appfs"
```

The boot flow reads RT-Smart firmware from the "rtt" partition and Linux firmware from the "linux" partition, loading them into their respective DDR regions.

### Source: `genimage-sdcard.cfg`

```
partition rtt {
    offset = 10M
    image = "big-core/rtt_system.bin"
    size = 20M
}
partition linux {
    offset = 30M
    image = "little-core/linux_system.bin"
    size = 50M
}
partition rootfs {
    offset = 128M
    partition-type-uuid = "L"
    image = "little-core/rootfs.ext4"
}
partition fat32appfs {
    partition-type-uuid = "F"
    image = "app.vfat"
}
```

## 9. Real Device Verification

The following data was captured from a running CanMV-K230 board via serial console (USB CDC-ACM).

### 9.1 Little Core (Linux) - `/dev/ttyACM0`

**Kernel version:**
```
Linux version 5.10.4 (riscv64-unknown-linux-gnu-gcc Xuantie-900 V2.6.0)
```

**Boot command line:**
```
root=/dev/mmcblk1p3 loglevel=8 rw rootdelay=4 rootfstype=ext4
console=ttyS0,115200 crashkernel=256M-:128M earlycon=sbi
```

**Memory info (`free -m`):**
```
              total     used     free   shared  buff/cache  available
Mem:           103       36       13        0        53         62
```

**Key `/proc/meminfo` values:**
```
MemTotal:       105876 kB    (~103.4 MB usable)
CmaTotal:        53248 kB    (52 MB reserved for CMA/DMA)
```

**`/proc/iomem` (System RAM):**
```
08200000-0fffefff : System RAM
```

**DTS memory node (raw hex from `/sys/firmware/devicetree/base/memory@0/reg`):**
```
00000000 08200000 00000000 07dff000
  base = 0x08200000    size = 0x07DFF000 (132,116,480 bytes)
```

**`dmesg` memory-related lines:**
```
cma: Reserved 52 MiB at 0x000000000c800000
Zone ranges:
  DMA32    [mem 0x0000000008200000-0x000000000fffefff]
node   0: [mem 0x0000000008200000-0x000000000fffefff]
Memory: 26588K/129020K available (9099K kernel code, 4868K rwdata,
        4096K rodata, 271K init, 370K bss, 49184K reserved,
        53248K cma-reserved)
```

**Disk layout (`df -h`):**
```
/dev/root          62.0G   72.2M   61.9G   0%  /
/dev/mmcblk1p4    174.1G   92.1M  174.0G   0%  /sharefs
```

### 9.2 Big Core (RT-Smart msh) - `/dev/ttyACM1`

**`free` output:**
```
memheap           pool size   max used size  available size
heap             33554432     702464         32866304
```

- Pool size: 33,554,432 bytes = exactly 32 MB (matches `RT_HW_HEAP_END - RT_HW_HEAP_BEGIN = 0x2000000`)
- Max used: 702,464 bytes (~686 KB peak usage)
- Available: 32,866,304 bytes (~31.3 MB free)

**`list_device` (MPP/IPCM devices confirmed present):**
```
mmz_userdev          Character Device     0
ipcm_user            Character Device     0
vb_device            Character Device     0
log                  Character Device     0
sys                  Character Device     0
```

Plus 40+ media/sensor devices (vicap, venc, vdec, vo, ai, ao, dma, fft, etc.)

**`list_thread` (active threads):**
```
tshell                20  running    (shell)
sharefs_client         5  suspend    (shared filesystem)
ipcm-discovery         5  suspend    (IPCM auto-discovery)
ipcm-recv              5  suspend    (IPCM message receiver)
mcm_task               0  suspend    (media control)
```

### 9.3 Analysis: Source Code vs. Real Device

| Parameter | Source Code | Real Device | Match? |
|-----------|------------|-------------|--------|
| Linux System RAM start | `0x08200000` (DTS) | `0x08200000` (`/proc/iomem`) | Yes |
| Linux System RAM size | `0x07DFF000` (DTS) | `129020 KB` (dmesg) = `0x07DFF000` | Yes |
| Linux MemTotal | ~126 MB (DTS) | 103.4 MB (`/proc/meminfo`) | Expected: kernel overhead ~23 MB |
| CMA reservation | (kernel config) | 52 MiB at `0x0C800000` | Within Linux range |
| RT-Smart heap size | `0x2000000` (board.h) | 33,554,432 bytes (`free`) | Yes, exactly 32 MB |
| crashkernel=256M-:128M | bootargs | Logged but **no effect** (only 126 MB available) | As expected |
| MMZ region visible from Linux | Not expected | Not in `/proc/iomem` | Correct - MMZ is RT-Smart only |
| OpenSBI region | `0x08000000-0x081FFFFF` | Not in `/proc/iomem` (RAM starts at `0x08200000`) | Correct - excluded from Linux |
| IPCM devices on RT-Smart | ipcm_user, sharefs | Present in `list_device` / `list_thread` | Yes |
| MMZ device on RT-Smart | mmz_userdev | Present in `list_device` | Yes |

**Key observations:**

1. **MemTotal vs DTS size gap**: Linux reports 105,876 KB usable out of 129,020 KB total RAM. The ~23 MB difference is consumed by kernel code (9 MB), rwdata (5 MB), rodata (4 MB), and other kernel structures. This is normal.

2. **CMA 52 MB**: A large CMA region is reserved within Linux's memory for DMA-capable allocations (likely used by WiFi driver `bcmdhd` and USB). This further reduces available memory.

3. **crashkernel parameter is ineffective**: The `crashkernel=256M-:128M` parameter requests 128 MB for kdump when total RAM >= 256 MB. Since Linux only has ~126 MB, the threshold is never met and no crash kernel memory is reserved.

4. **OpenSBI is invisible to Linux**: The 2 MB OpenSBI region (0x08000000-0x081FFFFF) does not appear in `/proc/iomem` because the DTS deliberately starts the memory node at 0x08200000, excluding OpenSBI's resident firmware.

5. **RT-Smart heap utilization is low**: Only ~686 KB out of 32 MB heap is used at idle. The heap is for kernel `rt_malloc()` allocations; user processes use the page allocator (the remaining ~93 MB), and media buffers use MMZ (~252 MB).

## References

| File | Content |
|------|---------|
| `k230_sdk/src/big/mpp/include/comm/k_autoconf_comm.h` | All `CONFIG_MEM_*` defines (memory partition master config) |
| `k230_sdk/src/big/rt-smart/kernel/bsp/maix3/board/board.h` | RT-Smart heap/page boundaries + full peripheral address map |
| `k230_sdk/src/big/rt-smart/kernel/bsp/maix3/link.lds` | RT-Smart linker script (entry at 0x220000) |
| `k230_sdk/src/little/linux/arch/riscv/boot/dts/kendryte/k230_canmv.dts` | Linux DDR region (0x8200000, 0x7dff000) |
| `k230_sdk/src/little/uboot/board/canaan/common/k230_img.c` | DTB/initrd placement address calculation |
| `k230_sdk/src/common/cdk/kernel/ipcm/arch/k230/configs/k230_riscv_rtsmart_config` | IPCM shared memory physical addresses |
| `k230_sdk/src/common/cdk/kernel/ipcm/arch/k230/configs/k230_riscv_linux_config` | IPCM Linux-side role configuration |
| `k230_sdk/src/big/rt-smart/kernel/bsp/maix3/board/mpp/mpp_init.c` | MMZ initialization (0x10000000, 0x0fbff000) |
| `k230_sdk/board/common/gen_image_cfg/genimage-sdcard.cfg` | SD card partition layout |
