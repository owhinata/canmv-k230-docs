# ビッグコアソフトウェア構成 (RT-Smart / MPP)

K230 ビッグコア上で動作する RT-Smart（リアルタイム OS）と MPP（メディア処理プラットフォーム）の構成を説明します。

## 概要

K230 のビッグコアでは、RT-Smart がカーネル（OS）として動作し、その上で MPP がマルチメディアハードウェアを制御します。

```
┌─────────────────────────────────────────────────────┐
│              ユーザ空間 (LWP プロセス)                │
│  ┌──────────┐  ┌──────────┐  ┌───────────────────┐  │
│  │ MPP      │  │ ミドル   │  │ ユーザアプリ      │  │
│  │ MPI API  │  │ ウェア   │  │ (sample_*.elf)    │  │
│  │ (mpi_*)  │  │ (FFmpeg, │  │                   │  │
│  │          │  │  RTSP等) │  │                   │  │
│  └────┬─────┘  └────┬─────┘  └───────────────────┘  │
│───────┼──────────────┼───────────────────────────────│
│       │  カーネル空間 │                               │
│  ┌────┴─────┐  ┌─────┴──────────────────────────┐    │
│  │ MPP      │  │ RT-Smart カーネル              │    │
│  │ カーネル │  │ (スケジューラ, IPC, DFS,       │    │
│  │ モジュール│  │  メモリ管理, LWP, ドライバ)    │    │
│  └──────────┘  └────────────────────────────────┘    │
│───────────────────────────────────────────────────────│
│                K230 SoC ハードウェア                   │
│  RISC-V C908 / ISP / VPU / GPU / DPU / Audio / FFT   │
└─────────────────────────────────────────────────────┘
```

| 要素 | 役割 |
|------|------|
| **RT-Smart** | リアルタイム OS。プロセス管理、メモリ保護、デバイスドライバ、ファイルシステムなどカーネル機能を提供 |
| **MPP** | K230 SoC のマルチメディアハードウェア（ISP, VPU, GPU, DPU, Audio 等）を制御するライブラリ群 |

## RT-Smart (リアルタイム OS)

### RT-Smart とは

RT-Smart は [RT-Thread](https://www.rt-thread.io/) の拡張版で、MMU を利用したユーザ空間プロセス（LWP: Lightweight Process）をサポートするリアルタイム OS です。

- **LWP (Lightweight Process)**: カーネル空間とユーザ空間を分離し、メモリ保護を提供
- **200+ システムコール**: POSIX 互換の musl libc ベース
- **ツールチェーン**: `riscv64-unknown-linux-musl-gcc`

!!! info "RT-Thread と RT-Smart"
    RT-Thread はマイクロコントローラ向け RTOS、RT-Smart はその上位版で MMU によるプロセス分離をサポートします。
    K230 ビッグコアでは RT-Smart が使われています。

### ディレクトリ構成

RT-Smart のソースコードは `k230_sdk/src/big/rt-smart/` に配置されています。

| パス | 内容 |
|------|------|
| `kernel/rt-thread/src/` | カーネルコア（スケジューラ、IPC、タイマー等） |
| `kernel/rt-thread/components/` | カーネルコンポーネント（DFS, finsh, lwp, drivers, net 等） |
| `kernel/rt-thread/libcpu/` | CPU アーキテクチャ依存コード（RISC-V 含む） |
| `kernel/bsp/maix3/` | K230 ボードサポートパッケージ（BSP） |
| `userapps/` | ユーザ空間アプリケーション、SDK、テストケース |
| `init.sh` | 起動スクリプトのソース |

### カーネル主要コンポーネント

`kernel/rt-thread/components/` に含まれる主要コンポーネント:

| コンポーネント | 説明 |
|---------------|------|
| `lwp/` | Lightweight Process — ユーザ空間プロセスの管理 |
| `dfs/` | Distributed File System — 複数ファイルシステム対応（ROMFS, FAT, JFFS2, tmpfs 等） |
| `finsh/` | msh シェル — コンソールインターフェース |
| `drivers/` | デバイスドライバフレームワーク |
| `net/` | ネットワークスタック |
| `libc/` | C ライブラリサポート |
| `cplusplus/` | C++ サポート（Thread, Mutex, Semaphore 等） |

### K230 BSP (`kernel/bsp/maix3/`)

K230 固有のドライバとボード設定は BSP に含まれます。

#### 内蔵ドライバ (`board/interdrv/`)

SoC 内蔵ペリフェラルのドライバ:

| ドライバ | 対象 |
|---------|------|
| `uart/`, `uart_canaan/` | UART シリアル通信 |
| `i2c/` | I2C バス |
| `spi/` | SPI バス |
| `gpio/` | GPIO ピン制御 |
| `pwm/` | PWM 出力 |
| `adc/` | ADC (アナログ-デジタル変換) |
| `hwtimer/` | ハードウェアタイマー |
| `wdt/` | ウォッチドッグタイマー |
| `rtc/` | リアルタイムクロック |
| `sdio/` | SDIO (SD カード等) |
| `cipher/` | 暗号化エンジン |
| `gnne/` | ニューラルネットワークエンジン |
| `hardlock/` | ハードウェアロック |
| `pdma/` | DMA コントローラ |
| `sysctl/` | システム制御 |
| `tsensor/` | 温度センサー |

#### 外部ドライバ (`board/extdrv/`)

外部接続デバイスのドライバ:

| ドライバ | 対象 |
|---------|------|
| `cyw43xx/` | CYW43 WiFi/Bluetooth モジュール |
| `realtek/` | Realtek WiFi ドライバ |
| `nand/` | NAND フラッシュ |
| `eeprom/` | EEPROM |
| `touch/` | タッチパネル |
| `regulator/` | 電圧レギュレータ |

#### プロセッサ間通信 (`board/ipcm/`)

ビッグコア (RT-Smart) とリトルコア (Linux) 間の通信:

| ファイル | 機能 |
|---------|------|
| `sharefs_init.c` | sharefs — コア間ファイル共有 |
| `virt_tty_init.c` | 仮想 TTY — コア間コンソール転送 |
| `rtt_ctrl_init.c` | RT-Thread 制御初期化 |

#### USB サポート (`board/extcomponents/CherryUSB/`)

CherryUSB スタックによる USB デバイス/ホスト対応。CDC, HID, MSC, Audio, Video 等のクラスをサポートします。

### ビルドシステム

RT-Smart カーネルは **SCons + Kconfig** でビルドされます。

| 項目 | 値 |
|------|------|
| ビルドツール | SCons |
| コンフィグ | Kconfig (`rtconfig.h`) |
| ツールチェーン | `riscv64-unknown-linux-musl-gcc` |
| カーネル設定ファイル | `kernel/bsp/maix3/rtconfig.h` |

!!! note "ビルド手順"
    SDK 全体のビルド方法については [SDK ビルド](sdk_build.md) を参照してください。
    RT-Smart カーネルの部分ビルドについては [RT-Smart 起動カスタマイズ](rtsmart_boot.md) を参照してください。

## MPP (Media Processing Platform)

### MPP とは

MPP は K230 SoC のマルチメディアハードウェアを制御するためのライブラリ群です。カメラ入力、映像エンコード/デコード、ディスプレイ出力、音声処理、AI 推論前処理などの機能を提供します。

MPP は 3 層構成になっています:

```
ユーザアプリケーション
    ↓ (MPI API 呼び出し)
MPI ユーザ空間ライブラリ (mpi_*_api)
    ↓ (ioctl / システムコール)
MPP カーネルモジュール
    ↓
K230 ハードウェア
```

| 層 | 場所 | 説明 |
|----|------|------|
| カーネルモジュール | `mpp/kernel/` | ハードウェアに直接アクセスするドライバ |
| MPI API | `mpp/userapps/api/` | ユーザ空間向け C API (`mpi_vicap_api.h` 等) |
| ミドルウェア | `mpp/middleware/` | FFmpeg, RTSP, MP4 等の高レベルライブラリ |

### ディレクトリ構成

MPP のソースコードは `k230_sdk/src/big/mpp/` に配置されています。

| パス | 内容 |
|------|------|
| `kernel/` | カーネル空間ドライバ（センサー、コネクタ、FFT、GPU、PM 等） |
| `kernel/lib/` | プリコンパイル済みカーネルライブラリ（21 個） |
| `userapps/api/` | MPI API ヘッダ（25 個） |
| `userapps/lib/` | プリコンパイル済みユーザ空間ライブラリ（50+ 個） |
| `userapps/sample/` | サンプルアプリケーション（55+ 個） |
| `userapps/src/` | ユーザ空間ライブラリのソースコード |
| `include/` | 公開ヘッダ（型定義、エラーコード、モジュール共通定義） |
| `middleware/` | マルチメディアミドルウェア（FFmpeg, Live555 等） |

### 主要モジュール一覧

| モジュール | MPI API | 機能 |
|-----------|---------|------|
| **VICAP** | `mpi_vicap_api.h` | カメラ入力（Video Capture） |
| **ISP** | `mpi_isp_api.h` | 画像信号処理 |
| **VENC** | `mpi_venc_api.h` | 映像エンコード（H.264/H.265） |
| **VDEC** | `mpi_vdec_api.h` | 映像デコード |
| **VO** | `mpi_vo_api.h` | 映像出力（Video Output） |
| **AI** | `mpi_ai_api.h` | 音声入力（Audio Input） |
| **AO** | `mpi_ao_api.h` | 音声出力（Audio Output） |
| **AENC** | `mpi_aenc_api.h` | 音声エンコード |
| **ADEC** | `mpi_adec_api.h` | 音声デコード |
| **DPU** | `mpi_dpu_api.h` | 深度処理ユニット |
| **DMA** | `mpi_dma_api.h` | DMA 転送 |
| **GPU** | `vg_lite.h` | 2D グラフィックス（VGLite） |
| **FFT** | `mpi_fft_api.h` | FFT アクセラレータ |
| **PM** | `mpi_pm_api.h` | 電力管理 |
| **Sensor** | `mpi_sensor_api.h` | イメージセンサー制御 |
| **Connector** | `mpi_connector_api.h` | ディスプレイコネクタ（LCD/HDMI） |
| **VDSS** | `mpi_vdss_api.h` | ビデオサブシステム |
| **NonAI 2D** | `mpi_nonai_2d_api.h` | 非 AI 2D 画像処理 |
| **Dewarp** | `mpi_dewarp_api.h` | レンズ歪み補正 |
| **Cipher** | `mpi_cipher_api.h` | 暗号化処理 |

#### 対応イメージセンサー

`mpp/kernel/sensor/src/` に以下のセンサードライバが含まれます:

GC2053, GC2093, IMX335, OS08A20, OV5647, OV9286, OV9732, SC035HGS, SC132GS, SC201CS 等

#### 対応ディスプレイコネクタ

`mpp/kernel/connector/src/` に以下のドライバが含まれます:

HX8399, ILI9806, LT9611 (HDMI), NT35516, ST7701 等

### ミドルウェア

`mpp/middleware/` に含まれる高レベルマルチメディアライブラリ:

| ライブラリ | 説明 |
|-----------|------|
| **FFmpeg** | マルチメディアフレームワーク（エンコード/デコード/変換） |
| **Live555** | RTSP/RTP ストリーミングライブラリ |
| **x264** | H.264 ソフトウェアエンコーダ |
| **kdmedia** | Canaan 独自メディアフレームワーク |
| **mp4_format** | MP4 ファイルフォーマット処理 |
| **rtsp_server** | RTSP サーバー実装 |
| **rtsp_client** | RTSP クライアント実装 |
| **rtsp_pusher** | RTSP ストリーミング配信 |
| **mp4_player** | MP4 プレーヤー実装 |

### サンプルアプリケーション

`mpp/userapps/sample/` と `mpp/middleware/sample/` に多数のサンプルが含まれます。

**映像関連:**

- `sample_vicap.elf` — カメラキャプチャ
- `sample_venc.elf` — 映像エンコード
- `sample_vdec.elf` — 映像デコード
- `sample_vo.elf` — 映像出力

**音声関連:**

- `sample_audio.elf` — 音声入出力
- `sample_av.elf` — 音声映像統合

**AI / 画像処理:**

- `sample_face_detect.elf` — 顔検出
- `sample_face_ae.elf` — 顔認識 + 自動露出
- `sample_dpu.elf` — 深度処理
- `sample_dpu_vicap.elf` — 深度処理 + カメラ入力

**グラフィックス:**

- `sample_gpu_cube.elf` — GPU 3D レンダリング
- `sample_lvgl.elf` — LVGL GUI フレームワーク

**ストリーミング（ミドルウェア）:**

- `sample_rtspserver` — RTSP サーバー
- `sample_rtspclient` — RTSP クライアント
- `sample_player` — メディアプレーヤー
- `sample_muxer` / `sample_demuxer` — メディア多重化/分離

**ペリフェラル:**

- `sample_gpio.elf`, `sample_pwm.elf`, `sample_adc.elf`, `sample_i2c_slave.elf`, `sample_hwtimer.elf` 等

## RT-Smart と MPP の関係

MPP は RT-Smart の上で動作するライブラリ群です。カーネル空間とユーザ空間にまたがって構成されています。

```
┌──────────────────────────────────────────────────┐
│  ユーザ空間 (RT-Smart LWP プロセス)               │
│                                                  │
│  アプリケーション                                 │
│    ↓                                             │
│  MPI API (mpi_vicap_api, mpi_venc_api, ...)      │
│    ↓ ioctl                                       │
│══════════════════════════════════════════════════│
│  カーネル空間                                     │
│                                                  │
│  MPP カーネルモジュール                           │
│  (VICAP, VPU, ISP, VO, AI/AO, DPU, FFT, GPU)    │
│    ↓                                             │
│  RT-Smart カーネル                                │
│  (スケジューラ, メモリ管理, ドライバFW, DFS, IPC) │
│    ↓                                             │
│  K230 SoC ハードウェア                            │
└──────────────────────────────────────────────────┘
```

- **MPP カーネルモジュール** は RT-Smart のカーネル空間で動作し、SoC のマルチメディアハードウェアに直接アクセスします
- **MPI API** はユーザ空間のライブラリで、ioctl を通じてカーネルモジュールを制御します
- ユーザアプリケーションは **LWP プロセス** として実行され、MPI API を呼び出してマルチメディア機能を利用します
- RT-Smart はメモリ保護（MMU）により、ユーザ空間のバグがカーネルを破壊することを防ぎます

!!! note "関連ドキュメント"
    - [SDK ビルド](sdk_build.md) — K230 SDK 全体のビルド方法
    - [RT-Smart 起動カスタマイズ](rtsmart_boot.md) — 起動シーケンスと init.sh の変更方法
    - [Hello World](hello_world.md) — ビッグコア / リトルコア向けアプリケーションのビルド
