# K230 DDR メモリレイアウト

## 1. 概要

K230は512 MB DDR (0x00000000 - 0x1FFFFFFF) を搭載したデュアルコアRISC-V SoCである。メモリは以下の3つの主要領域に分割される:

- **RT-Smart** (ビッグコア, C908) - MPPメディアフレームワーク搭載のリアルタイムOS
- **Linux** (リトルコア, C908) - rootfs付きの汎用Linux
- **MMZ** (Media Memory Zone) - MPPが管理する共有メディアバッファプール

メモリ境界は `k230_sdk/src/big/mpp/include/comm/k_autoconf_comm.h` でビルド時に定義され、実行時には変更できない。

## 2. DDR物理メモリマップ

```
 物理アドレス             サイズ     領域
 ========================================================================
 0x00000000 +---------+
            | 予約    |  1 MB     SoC予約 (ブートROMスクラッチ領域)
 0x00100000 +---------+
            |  IPCM   |  1 MB     コア間通信 (共有メモリ)
 0x00200000 +---------+
            |         |
            | RT-Smart| 126 MB    ビッグコアOS (カーネル + ヒープ + ページ)
            |         |
 0x08000000 +---------+
            |         |
            |  Linux  | 128 MB    リトルコアOS (OpenSBI + カーネル)
            |         |
 0x10000000 +---------+
            |         |
            |   MMZ   | ~252 MB   メディアメモリゾーン (MPPバッファ)
            |         |
 0x1FBFF000 +---------+
            | ガード  | ~4 MB     末尾ガード / 未使用
 0x20000000 +---------+
            合計: 512 MB
```

### 出典: `k_autoconf_comm.h`

| 定義 | 値 | 意味 |
|------|-----|------|
| `CONFIG_MEM_TOTAL_SIZE` | `0x20000000` | DDR合計 512 MB |
| `CONFIG_MEM_IPCM_BASE` | `0x00100000` | IPCM領域開始 |
| `CONFIG_MEM_IPCM_SIZE` | `0x00100000` | 1 MB |
| `CONFIG_MEM_RTT_SYS_BASE` | `0x00200000` | RT-Smart領域開始 |
| `CONFIG_MEM_RTT_SYS_SIZE` | `0x07E00000` | 126 MB |
| `CONFIG_MEM_LINUX_SYS_BASE` | `0x08000000` | Linux領域開始 |
| `CONFIG_MEM_LINUX_SYS_SIZE` | `0x08000000` | 128 MB |
| `CONFIG_MEM_MMZ_BASE` | `0x10000000` | MMZ領域開始 |
| `CONFIG_MEM_MMZ_SIZE` | `0x0FC00000` | 252 MB (設定値) |
| `CONFIG_MEM_BOUNDARY_RESERVED_SIZE` | `0x00001000` | 4 KB 境界ガード |

## 3. IPCM (プロセッサ間通信) 共有メモリ

0x00100000にある1 MBのIPCM領域は以下のように分割される:

```
 0x00100000 +-----------+
            | RTT->Linux|  512 KB   共有メモリ (ノード1 -> ノード0)
 0x00180000 +-----------+
            | Linux->RTT|  484 KB   共有メモリ (ノード0 -> ノード1)
 0x001F9000 +-----------+
            | 仮想TTY   |   16 KB   IPCM経由の仮想シリアル
 0x001FD000 +-----------+
            | 予約      |   12 KB   IPCMノード記述子 + ガード
 0x00200000 +-----------+
```

### 出典: IPCM設定ファイル

**RT-Smart側** (`k230_riscv_rtsmart_config`):
```
node_id=1               # RT-Smartはノード1
top_role="slave"
shm_phys_1to0=0x100000  # RTT(1)->Linux(0) ベースアドレス
shm_size_1to0=0x80000   # 512 KB
shm_phys_0to1=0x180000  # Linux(0)->RTT(1) ベースアドレス
shm_size_0to1=0x79000   # 484 KB
virt_tty_phys=0x1f9000  # 仮想TTYベースアドレス
virt_tty_size=0x4000    # 16 KB
```

**Linux側** (`k230_riscv_linux_config`):
```
node_id=0               # Linuxはノード0
top_role="master"
virt_tty_role="server"
```

Linux側はSHMアドレスを直接設定しない。RT-Smart（スレーブ）側が物理アドレスの定義を管理し、LinuxカーネルのIPCMドライバがIPCMプロトコル経由で実行時に読み取る。

## 4. RT-Smart 内部メモリレイアウト

RT-Smartは0x00200000 - 0x07FFFFFF (126 MB) を使用する。内部構造は以下の通り:

```
 0x00200000 +-----------+
            | ブート    |  128 KB   U-BootがRT-Smartイメージをここにロード
 0x00220000 +-----------+
            | .text     |           カーネルコード (0x220000にリンク)
            | .rodata   |
            | .data     |
            | .bss      |
   __bss_end +-----------+
            |           |
            | カーネル  |  32 MB    RT_HW_HEAP (rt_mallocプール)
            | ヒープ    |
            |           |
  +0x2000000 +-----------+
            |           |
            | ページ    |  ~93 MB   ページアロケータ (LWPユーザプロセス用)
            | アロケータ|
            |           |
 0x07FFF000 +-----------+
            | 予約      |  4 KB     境界ガード (MEMORY_RESERVED)
 0x08000000 +-----------+
```

### 出典: `board.h`

```c
#define MEMORY_RESERVED     0x1000
#define RAM_END             0x7fff000

#define RT_HW_HEAP_BEGIN    ((void *)&__bss_end)
#define RT_HW_HEAP_END      ((void *)(((rt_size_t)RT_HW_HEAP_BEGIN) + 0x2000000))

#define RT_HW_PAGE_START    ((void *)((rt_size_t)RT_HW_HEAP_END + sizeof(rt_size_t)))
#define RT_HW_PAGE_END      ((void *)(RAM_END))
```

### 出典: `link.lds`

```
MEMORY
{
   SRAM : ORIGIN = 0x220000, LENGTH = 128895K
}
/* 0x00200000 - 0x00220000: Bootloader */
/* 0x00220000 - 0x08000000: Kernel     */
```

## 5. Linux 内部メモリレイアウト

Linuxは0x08000000 - 0x0FFFFFFF (128 MB) を使用する。内訳は以下の通り:

```
 0x08000000 +-----------+
            | OpenSBI   |  2 MB     SBIファームウェア
 0x08200000 +-----------+
            | Linux     | ~126 MB   カーネルコード + データ + ユーザメモリ
            | カーネル  |           DTS宣言: reg = <0x8200000 0x7dff000>
            |           |
            |  0x0A000000: DTB配置アドレス
            |  0x0A100000: initrd配置アドレス
            |  0x0C800000: CMA領域 (52 MB)
            |           |
 0x0FFFF000 +-----------+
            | ガード    |  4 KB     境界
 0x10000000 +-----------+
```

### 出典: `k230_canmv.dts`

```dts
&ddr {
    reg = <0x0 0x8200000 0x0 0x7dff000>;  /* 132,116,480バイト = ~126 MB */
};

chosen {
    linux,initrd-start = <0x0 0xa100000>;
};
```

### 出典: `k230_img.c`

```c
#define OPENSBI_DTB_ADDR  (CONFIG_MEM_LINUX_SYS_BASE + 0x2000000)   /* 0x0A000000 */
#define RAMDISK_ADDR      (CONFIG_MEM_LINUX_SYS_BASE + 0x2000000 + 0x100000)  /* 0x0A100000 */
```

U-Bootはカーネル + DTB + initrdを含むLinux uImageをロードし、カーネルをロードアドレスに展開した後、DTBを0x0A000000に、initrdを0x0A100000にコピーしてからOpenSBI/カーネルにジャンプする。

## 6. MMZ (メディアメモリゾーン)

MMZはRT-SmartのMPPフレームワークが管理する大規模な連続バッファプールで、カメラキャプチャ、ビデオエンコード/デコード、ディスプレイ、AI推論などのメディア操作に使用される。

```
 0x10000000 +-----------+
            |           |
            |    MMZ    |  ~252 MB   メディアバッファ (VBプール, DMAバッファ)
            |           |           mpp_init.cのmmz_init()で管理
            |           |
 0x1FBFF000 +-----------+
            | 末尾ガード|  ~4 MB    未使用 / 境界保護
 0x20000000 +-----------+
```

### 出典: `mpp_init.c`

```c
#define MEM_MMZ_BASE 0x10000000UL
#define MEM_MMZ_SIZE 0xfbff000UL    /* 252 MB - 4 KB (境界分を減算) */

ret = mmz_init(MEM_MMZ_BASE, MEM_MMZ_SIZE);
```

注意: k_autoconf_comm.hの`CONFIG_MEM_MMZ_SIZE`は`0x0FC00000` (正確に252 MB)。mpp_init.cでの実際の初期化は`0x0FBFF000` (252 MB - 4 KB) を使用し、安全ガードとして`CONFIG_MEM_BOUNDARY_RESERVED_SIZE` (4 KB) を差し引いている。

MMZメモリはVB (Video Buffer) サブシステム経由で割り当てられ、RT-Smartユーザプロセス (`/dev/mmz_userdev` 経由) とカーネル空間のMPPドライバの両方からアクセス可能。

## 7. MMIO / ペリフェラルアドレスマップ

ペリフェラルは0x80000000以降 (DDR空間外) にマッピングされる。出典: `board.h` およびK230テクニカルリファレンスマニュアル:

| アドレス範囲 | サイズ | ペリフェラル |
|-------------|--------|-------------|
| `0x80000000 - 0x801FFFFF` | 2 MB | KPU L2キャッシュ |
| `0x80200000 - 0x803FFFFF` | 2 MB | SRAM |
| `0x80400000 - 0x804007FF` | 2 KB | KPU設定 |
| `0x80400800 - 0x80400BFF` | 1 KB | FFT |
| `0x80400C00 - 0x80400FFF` | 1 KB | AI 2Dエンジン |
| `0x80800000 - 0x80803FFF` | 16 KB | GSDMA |
| `0x80804000 - 0x80807FFF` | 16 KB | DMA |
| `0x80808000 - 0x8080BFFF` | 16 KB | GZIP展開 |
| `0x8080C000 - 0x8080FFFF` | 16 KB | Non-AI 2D |
| `0x90000000 - 0x90007FFF` | 32 KB | ISP |
| `0x90008000 - 0x90008FFF` | 4 KB | DeWarp |
| `0x90009000 - 0x9000AFFF` | 8 KB | RX CSI |
| `0x90400000 - 0x9040FFFF` | 64 KB | H264/HEVC/JPEGコーデック |
| `0x90800000 - 0x9083FFFF` | 256 KB | 2.5D GPU |
| `0x90840000 - 0x9084FFFF` | 64 KB | VO (ビデオ出力) |
| `0x90850000 - 0x90850FFF` | 4 KB | DSI |
| `0x90A00000 - 0x90A007FF` | 2 KB | 3Dエンジン |
| `0x91000000 - 0x91000BFF` | 3 KB | PMU |
| `0x91000C00 - 0x91000FFF` | 1 KB | RTC |
| `0x91100000 - 0x91100FFF` | 4 KB | CMU (クロック) |
| `0x91101000 - 0x91101FFF` | 4 KB | RMU (リセット) |
| `0x91102000 - 0x91102FFF` | 4 KB | BOOT制御 |
| `0x91103000 - 0x91103FFF` | 4 KB | PWR (電源) |
| `0x91104000 - 0x91104FFF` | 4 KB | Mailbox |
| `0x91105000 - 0x911057FF` | 2 KB | IOMUX |
| `0x91105800 - 0x91105FFF` | 2 KB | ハードウェアタイマ |
| `0x91106000 - 0x911067FF` | 2 KB | WDT0 |
| `0x91106800 - 0x91106FFF` | 2 KB | WDT1 |
| `0x91107000 - 0x911077FF` | 2 KB | 温度センサ |
| `0x91107800 - 0x91107FFF` | 2 KB | HDI |
| `0x91108000 - 0x91108FFF` | 4 KB | STC (システムタイマ) |
| `0x91200000 - 0x9120FFFF` | 64 KB | ブートROM |
| `0x91210000 - 0x91217FFF` | 32 KB | セキュリティ |
| `0x91400000 - 0x91404FFF` | 20 KB | UART0-4 (各4 KB) |
| `0x91405000 - 0x91409FFF` | 20 KB | I2C0-4 (各4 KB) |
| `0x9140A000 - 0x9140AFFF` | 4 KB | PWM |
| `0x9140B000 - 0x9140CFFF` | 8 KB | GPIO0-1 (各4 KB) |
| `0x9140D000 - 0x9140DFFF` | 4 KB | ADC |
| `0x9140E000 - 0x9140EFFF` | 4 KB | オーディオCODEC |
| `0x9140F000 - 0x9140FFFF` | 4 KB | I2Sオーディオ |
| `0x91500000 - 0x9157FFFF` | 512 KB | USB 2.0 OTG x2 |
| `0x91580000 - 0x91581FFF` | 8 KB | SD/MMC HC x2 |
| `0x91582000 - 0x91583FFF` | 8 KB | SPI QSPI x2 |
| `0x91584000 - 0x91584FFF` | 4 KB | SPI OPI |
| `0x91585000 - 0x915853FF` | 1 KB | HI SYS設定 |
| `0x98000000 - 0x99FFFFFF` | 32 MB | DDRC設定空間 |
| `0xC0000000 - 0xC7FFFFFF` | 128 MB | SPI OPI XIPフラッシュ |

## 8. SDカードパーティションレイアウト

SDカードイメージは `genimage-sdcard.cfg` によりGPTパーティションで生成される:

```
 オフセット    サイズ    内容                      パーティション
 ======================================================================
   1 MB       512 KB   U-Boot SPL (コピー1)       (raw, GPT外)
   1.5 MB     512 KB   U-Boot SPL (コピー2)       (raw, GPT外)
   ~1.875 MB  128 KB   U-Boot env                 (raw, GPT外)
   2 MB       1.5 MB   U-Boot本体                 (raw, GPT外)
  10 MB        20 MB   RT-Smartファームウェア      GPTパーティション "rtt"
  30 MB        50 MB   Linuxファームウェア         GPTパーティション "linux"
 128 MB        可変    rootfs (ext4)              GPTパーティション "rootfs"
   rootfs後            sharefs (FAT32, 256 MB)    GPTパーティション "fat32appfs"
```

ブートフローは「rtt」パーティションからRT-Smartファームウェアを、「linux」パーティションからLinuxファームウェアを読み込み、それぞれのDDR領域にロードする。

### 出典: `genimage-sdcard.cfg`

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

## 9. 実機検証結果

以下のデータは、稼働中のCanMV-K230ボードからシリアルコンソール (USB CDC-ACM) 経由で取得した。

### 9.1 リトルコア (Linux) - `/dev/ttyACM0`

**カーネルバージョン:**
```
Linux version 5.10.4 (riscv64-unknown-linux-gnu-gcc Xuantie-900 V2.6.0)
```

**ブートコマンドライン:**
```
root=/dev/mmcblk1p3 loglevel=8 rw rootdelay=4 rootfstype=ext4
console=ttyS0,115200 crashkernel=256M-:128M earlycon=sbi
```

**メモリ情報 (`free -m`):**
```
              total     used     free   shared  buff/cache  available
Mem:           103       36       13        0        53         62
```

**主要な `/proc/meminfo` の値:**
```
MemTotal:       105876 kB    (~103.4 MB 使用可能)
CmaTotal:        53248 kB    (52 MB CMA/DMA予約済み)
```

**`/proc/iomem` (システムRAM):**
```
08200000-0fffefff : System RAM
```

**DTSメモリノード (`/sys/firmware/devicetree/base/memory@0/reg` の生バイナリ):**
```
00000000 08200000 00000000 07dff000
  base = 0x08200000    size = 0x07DFF000 (132,116,480バイト)
```

**`dmesg` メモリ関連行:**
```
cma: Reserved 52 MiB at 0x000000000c800000
Zone ranges:
  DMA32    [mem 0x0000000008200000-0x000000000fffefff]
node   0: [mem 0x0000000008200000-0x000000000fffefff]
Memory: 26588K/129020K available (9099K kernel code, 4868K rwdata,
        4096K rodata, 271K init, 370K bss, 49184K reserved,
        53248K cma-reserved)
```

**ディスクレイアウト (`df -h`):**
```
/dev/root          62.0G   72.2M   61.9G   0%  /
/dev/mmcblk1p4    174.1G   92.1M  174.0G   0%  /sharefs
```

### 9.2 ビッグコア (RT-Smart msh) - `/dev/ttyACM1`

**`free` 出力:**
```
memheap           pool size   max used size  available size
heap             33554432     702464         32866304
```

- プールサイズ: 33,554,432バイト = 正確に32 MB (`RT_HW_HEAP_END - RT_HW_HEAP_BEGIN = 0x2000000` と一致)
- 最大使用量: 702,464バイト (~686 KBピーク使用量)
- 利用可能: 32,866,304バイト (~31.3 MB空き)

**`list_device` (MPP/IPCMデバイス確認済み):**
```
mmz_userdev          Character Device     0
ipcm_user            Character Device     0
vb_device            Character Device     0
log                  Character Device     0
sys                  Character Device     0
```

他にも40以上のメディア/センサデバイス (vicap, venc, vdec, vo, ai, ao, dma, fft等) が存在。

**`list_thread` (アクティブスレッド):**
```
tshell                20  running    (シェル)
sharefs_client         5  suspend    (共有ファイルシステム)
ipcm-discovery         5  suspend    (IPCM自動検出)
ipcm-recv              5  suspend    (IPCMメッセージ受信)
mcm_task               0  suspend    (メディア制御)
```

### 9.3 分析: ソースコード vs. 実機

| パラメータ | ソースコード | 実機 | 一致? |
|-----------|------------|------|-------|
| Linux System RAM開始 | `0x08200000` (DTS) | `0x08200000` (`/proc/iomem`) | はい |
| Linux System RAMサイズ | `0x07DFF000` (DTS) | `129020 KB` (dmesg) = `0x07DFF000` | はい |
| Linux MemTotal | ~126 MB (DTS) | 103.4 MB (`/proc/meminfo`) | 想定通り: カーネルオーバーヘッド ~23 MB |
| CMA予約 | (カーネル設定) | 52 MiB at `0x0C800000` | Linux範囲内 |
| RT-Smartヒープサイズ | `0x2000000` (board.h) | 33,554,432バイト (`free`) | はい、正確に32 MB |
| crashkernel=256M-:128M | bootargs | 記録されたが**効果なし** (126 MBしかない) | 想定通り |
| Linux側からMMZ領域が見えるか | 見えない想定 | `/proc/iomem`に無し | 正しい - MMZはRT-Smart専用 |
| OpenSBI領域 | `0x08000000-0x081FFFFF` | `/proc/iomem`に無し (RAMは`0x08200000`から) | 正しい - Linuxから除外 |
| RT-Smart上のIPCMデバイス | ipcm_user, sharefs | `list_device` / `list_thread`に存在 | はい |
| RT-Smart上のMMZデバイス | mmz_userdev | `list_device`に存在 | はい |

**主要な所見:**

1. **MemTotalとDTSサイズの差**: Linuxは129,020 KBの総RAMのうち105,876 KBを使用可能と報告。~23 MBの差はカーネルコード (9 MB)、rwdata (5 MB)、rodata (4 MB)、その他のカーネル構造体によるもの。これは正常。

2. **CMA 52 MB**: DMA可能な割り当てのためにLinuxのメモリ内に大きなCMA領域が確保されている (WiFiドライバ`bcmdhd`やUSBで使用される可能性が高い)。これにより利用可能メモリがさらに減少。

3. **crashkernelパラメータは無効**: `crashkernel=256M-:128M`パラメータは総RAM >= 256 MBの場合に128 MBのkdump用メモリを要求する。Linuxには~126 MBしかないため、閾値に達せずクラッシュカーネルメモリは確保されない。

4. **OpenSBIはLinuxから不可視**: 2 MBのOpenSBI領域 (0x08000000-0x081FFFFF) は`/proc/iomem`に表示されない。DTSが意図的にメモリノードを0x08200000から開始し、OpenSBIの常駐ファームウェアを除外しているため。

5. **RT-Smartヒープ使用率は低い**: アイドル時で32 MBヒープのうち~686 KBのみ使用。ヒープはカーネルの`rt_malloc()`割り当て用。ユーザプロセスはページアロケータ (残り~93 MB)、メディアバッファはMMZ (~252 MB) を使用。

## 参考ファイル

| ファイル | 内容 |
|---------|------|
| `k230_sdk/src/big/mpp/include/comm/k_autoconf_comm.h` | 全`CONFIG_MEM_*`定義 (メモリ分割マスタ設定) |
| `k230_sdk/src/big/rt-smart/kernel/bsp/maix3/board/board.h` | RT-Smartヒープ/ページ境界 + 全ペリフェラルアドレスマップ |
| `k230_sdk/src/big/rt-smart/kernel/bsp/maix3/link.lds` | RT-Smartリンカスクリプト (0x220000エントリ) |
| `k230_sdk/src/little/linux/arch/riscv/boot/dts/kendryte/k230_canmv.dts` | Linux DDR領域 (0x8200000, 0x7dff000) |
| `k230_sdk/src/little/uboot/board/canaan/common/k230_img.c` | DTB/initrd配置アドレス計算 |
| `k230_sdk/src/common/cdk/kernel/ipcm/arch/k230/configs/k230_riscv_rtsmart_config` | IPCM共有メモリ物理アドレス |
| `k230_sdk/src/common/cdk/kernel/ipcm/arch/k230/configs/k230_riscv_linux_config` | IPCM Linux側ロール設定 |
| `k230_sdk/src/big/rt-smart/kernel/bsp/maix3/board/mpp/mpp_init.c` | MMZ初期化 (0x10000000, 0x0fbff000) |
| `k230_sdk/board/common/gen_image_cfg/genimage-sdcard.cfg` | SDカードパーティションレイアウト |
