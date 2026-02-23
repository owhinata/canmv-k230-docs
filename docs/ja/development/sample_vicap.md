# sample_vicap

このガイドでは、K230 の `sample_vicap` アプリケーションを CMake out-of-tree ビルドで構築する方法を説明します。このサンプルはカメラセンサー (VICAP) から映像フレームをキャプチャし、ディスプレイ (VO) に表示する、K230 MPP メディアパイプラインのデモアプリケーションです。

## 前提条件

- K230 SDK がビルド済みであること（ツールチェーン展開済み、MPP ライブラリコンパイル済み）
- SDK がリポジトリルートの `k230_sdk/` に配置されていること
- ホスト OS: x86_64 Linux
- CMake 3.16 以降

!!! note "SDK のビルド"
    K230 SDK のビルド手順については [SDK ビルド](sdk_build.md) を参照してください。

## 概要

`sample_vicap` は K230 SDK 公式の Video Input Capture (VICAP) サンプルです。以下を実演します:

- VICAP API によるカメラセンサーの設定
- Video Output (VO) ディスプレイパイプラインの構築
- VICAP 出力チャネルと VO レイヤーのバインドによるリアルタイムプレビュー
- オプションの GDMA ベース回転表示
- デバッグ用フレームダンプ

### ソースファイル

| ファイル | 説明 |
|---------|------|
| `sample_vicap.c` | メインアプリケーション — 引数パース、VICAP/VO セットアップ、フレームダンプループ |
| `vo_test_case.c` | VO ディスプレイヘルパー — レイヤー/OSD 作成（`vo_creat_layer_test`、`vo_creat_osd_test`） |
| `vo_test_case.h` | VO ヘルパー型（`osd_info`、`layer_info`）と関数宣言のヘッダ |

これらのファイルは SDK からコピーしたものです:

- `sample_vicap.c`: `k230_sdk/src/big/mpp/userapps/sample/sample_vicap/`
- `vo_test_case.c`, `vo_test_case.h`: `k230_sdk/src/big/mpp/userapps/sample/sample_vo/`

## 処理フロー

アプリケーションは以下のパイプラインに沿って動作します:

```
センサー → VI → ISP → Dewarp → [GDMA 回転] → VO → ディスプレイ
```

VICAP ハードウェアモジュール（Sensor、VI、ISP、Dewarp）の詳細については [K230 VICAP API リファレンス](https://www.kendryte.com/k230/en/dev/01_software/board/mpp/K230_VICAP_API_Reference.html) を参照してください。

### 初期化シーケンス

#### 1. 引数のパース

コマンドライン引数を解析し、動作モード、コネクタタイプ、デバイス/チャネルパラメータ、出力設定を構成します。

**Source:** [`main()` L557–L913][vicap-main-557]

MPP API 呼び出しなし — 標準 C の引数解析（`strcmp`、`atoi`）を使用。

#### 2. コネクタ情報の取得

ディスプレイコネクタ情報を取得し、出力解像度を決定します。

**Source:** [`main()` L921–L928][vicap-main-921]

| API コール | 目的 |
|-----------|------|
| `kd_mpi_get_connector_info()` | コネクタの解像度と設定を取得 |

#### 3. センサー情報の取得

設定されたセンサータイプのセンサー能力（解像度、フォーマット）を取得します。

**Source:** [`main()` L930–L952][vicap-main-930]

| API コール | 目的 |
|-----------|------|
| `kd_mpi_vicap_get_sensor_info()` | センサーの解像度と情報を取得 |

#### 4. VICAP デバイス属性の設定

入力ウィンドウ、ISP パイプライン制御（AE、AWB、HDR、DNR3）、動作モード、オプションの Dewarp を設定します。ロードイメージモードでは RAW 画像ファイルをデバイスに読み込みます。

**Source:** [`main()` L957–L1049][vicap-main-957]

| API コール | 目的 |
|-----------|------|
| `kd_mpi_vicap_set_dev_attr()` | デバイスの取得ウィンドウ、動作モード、ISP パイプラインを設定 |
| `kd_mpi_vicap_load_image()` | RAW 画像データを読み込み（ロードイメージモードのみ） |

#### 5. VO コネクタの初期化

ディスプレイコネクタハードウェアをオープンし、初期化します。

**Source:** [`main()` L1098–L1102][vicap-main-1098] → [`sample_vicap_vo_init()`][vicap-120]

| API コール | 目的 |
|-----------|------|
| `kd_mpi_get_connector_info()` | コネクタの詳細を取得 |
| `kd_mpi_connector_open()` | コネクタデバイスをオープン |
| `kd_mpi_connector_power_set()` | コネクタの電源を有効化 |
| `kd_mpi_connector_init()` | コネクタハードウェアを初期化 |

#### 6. VB (Video Buffer) の初期化

ピクセルフォーマットと解像度に基づいてチャネルごとのバッファサイズを計算し、ビデオバッファプールを初期化します。`vb_exit()` を `atexit()` で登録します。

**Source:** [`main()` L1104–L1109][vicap-main-1104]

- [`sample_vicap_vb_init()`][vicap-258] — バッファプールサイズを計算し VB を初期化
- [`vb_exit()`][vicap-518] — クリーンアップ用に `atexit` で登録

| API コール | 目的 |
|-----------|------|
| `kd_mpi_vb_set_config()` | バッファプールの数とサイズを設定 |
| `kd_mpi_vb_set_supplement_config()` | JPEG サプリメントバッファの設定 |
| `kd_mpi_vb_init()` | ビデオバッファプールを初期化 |

#### 7. VICAP チャネル属性の設定

有効な各チャネルの出力ウィンドウ、クロップ領域、ピクセルフォーマット、バッファ数、フレームレートを設定します。

**Source:** [`main()` L1112–L1168][vicap-main-1112]

| API コール | 目的 |
|-----------|------|
| `kd_mpi_vicap_set_dump_reserved()` | チャネルのダンプバッファを予約 |
| `kd_mpi_vicap_set_chn_attr()` | チャネルの出力フォーマット、サイズ、クロップ、バッファを設定 |

#### 8. VICAP と VO のバインド

VICAP 出力チャネルを VO ディスプレイレイヤーに接続します。回転値 17〜19 の場合、VI と VO の間に GDMA チャネルが挿入されます。それ以外の場合、VI が VO に直接バインドされます。

**Source:** [`main()` L1170–L1283][vicap-main-1170]

- [`sample_vicap_bind_vo()`][vicap-358] — VI から VO への直接バインド（GDMA なし）
- [`dma_dev_attr_init()`][vicap-402] — GDMA デバイスの初期化（回転パス）

**直接バインド（GDMA なし）:**

| API コール | 目的 |
|-----------|------|
| `kd_mpi_sys_bind()` | VI チャネルを VO チャネルにバインド |

**GDMA 回転パス（回転値 17〜19）:**

| API コール | 目的 |
|-----------|------|
| `kd_mpi_dma_set_dev_attr()` | GDMA デバイスを設定 |
| `kd_mpi_dma_start_dev()` | GDMA デバイスを開始 |
| `kd_mpi_dma_request_chn()` | GDMA チャネルを要求 |
| `kd_mpi_sys_bind()` | VI → GDMA をバインド |
| `kd_mpi_sys_bind()` | GDMA → VO をバインド |
| `kd_mpi_dma_set_chn_attr()` | GDMA チャネルの回転とフォーマットを設定 |
| `kd_mpi_dma_start_chn()` | GDMA チャネルを開始 |

#### 9. VO レイヤーの設定

ディスプレイレイヤーと OSD をセットアップします。マージンを計算してレイヤーを画面中央に配置します。

**Source:** [`main()` L1289–L1293][vicap-main-1289]

- [`sample_vicap_vo_layer_init()`][vicap-148] — レイヤー/OSD 作成のオーケストレーション
- [`vo_creat_layer_test()`][vo-75] — ビデオレイヤーの作成
- [`vo_creat_osd_test()`][vo-34] — OSD レイヤーの作成

| API コール | 目的 |
|-----------|------|
| `kd_mpi_vo_set_video_layer_attr()` | レイヤーのサイズ、位置、回転を設定 |
| `kd_mpi_vo_enable_video_layer()` | ビデオレイヤーを有効化 |
| `kd_mpi_vo_set_video_osd_attr()` | OSD 属性を設定 |
| `kd_mpi_vo_osd_enable()` | OSD レイヤーを有効化 |

#### 10. VICAP の初期化と開始

有効な各 VICAP デバイスを初期化し、フレームキャプチャを開始します。

**Source:** [`main()` L1295–L1315][vicap-main-1295]

| API コール | 目的 |
|-----------|------|
| `kd_mpi_vicap_init()` | VICAP デバイスを初期化 |
| `kd_mpi_vicap_start_stream()` | キャプチャストリームを開始 |

#### 11. VO の有効化

ディスプレイ出力を有効化します。

**Source:** [`main()` L1317][vicap-main-1317] → [`sample_vicap_vo_enable()`][vicap-248]

| API コール | 目的 |
|-----------|------|
| `kd_mpi_vo_enable()` | VO ディスプレイ出力を有効化 |

#### 12. スレーブモード設定（任意）

スレーブモードが有効な場合、外部同期信号生成のために VICAP スレーブタイミングパラメータを設定します。

**Source:** [`main()` L1319–L1333][vicap-main-1319]

| API コール | 目的 |
|-----------|------|
| `kd_mpi_vicap_set_slave_attr()` | スレーブタイミング（vsync 周期、ハイ期間）を設定 |
| `kd_mpi_vicap_set_slave_enable()` | スレーブ vsync/hsync 出力を有効化 |

### クリーンアップシーケンス

アプリケーション終了時（ユーザーが `q` を押下）、リソースは逆順で解放されます:

#### 1. スレーブモード無効化

**Source:** [`main()` L1604–L1611][vicap-main-1604]

| API コール | 目的 |
|-----------|------|
| `kd_mpi_vicap_set_slave_enable()` | vsync/hsync 出力を無効化 |

#### 2. VICAP ストリーム停止

**Source:** [`main()` L1613–L1621][vicap-main-1613]

| API コール | 目的 |
|-----------|------|
| `kd_mpi_vicap_stop_stream()` | キャプチャストリームを停止 |

#### 3. VICAP デバイス解放

**Source:** [`main()` L1623–L1627][vicap-main-1623]

| API コール | 目的 |
|-----------|------|
| `kd_mpi_vicap_deinit()` | VICAP デバイスを終了 |

#### 4. VO レイヤー無効化

ビデオディスプレイレイヤーと OSD オーバーレイを無効化します。

**Source:** [`main()` L1636–L1670][vicap-main-1636]

- [`sample_vicap_disable_vo_layer()`][vicap-250]
- [`sample_vicap_disable_vo_osd()`][vicap-254]

| API コール | 目的 |
|-----------|------|
| `kd_mpi_vo_disable_video_layer()` | ビデオレイヤーを無効化 |
| `kd_mpi_vo_osd_disable()` | OSD レイヤーを無効化 |

#### 5. GDMA 解放（使用時）

**Source:** [`main()` L1671–L1699][vicap-main-1671]

| API コール | 目的 |
|-----------|------|
| `kd_mpi_dma_stop_chn()` | GDMA チャネルを停止 |
| `kd_mpi_sys_unbind()` | VI → GDMA および GDMA → VO のバインドを解除 |
| `kd_mpi_dma_release_chn()` | GDMA チャネルを解放 |

#### 6. VI–VO アンバインド（GDMA 未使用時）

**Source:** [`main()` L1700–L1702][vicap-main-1700] → [`sample_vicap_unbind_vo()`][vicap-380]

| API コール | 目的 |
|-----------|------|
| `kd_mpi_sys_unbind()` | VI と VO のバインドを解除 |

#### 7. GDMA デバイス停止

**Source:** [`main()` L1707–L1712][vicap-main-1707]

| API コール | 目的 |
|-----------|------|
| `kd_mpi_dma_stop_dev()` | GDMA デバイスを停止 |

#### 8. VB 解放

**Source:** registered via [`atexit()` L1109][vicap-main-1109] → [`vb_exit()`][vicap-518]

| API コール | 目的 |
|-----------|------|
| `kd_mpi_vb_exit()` | VB サブシステムを終了 |

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

## ビルド手順

### 1. 設定

```bash
cmake -B build/sample_vicap -S apps/sample_vicap \
  -DCMAKE_TOOLCHAIN_FILE="$(pwd)/cmake/toolchain-k230-rtsmart.cmake"
```

### 2. ビルド

```bash
cmake --build build/sample_vicap
```

### 3. 確認

```bash
file build/sample_vicap/sample_vicap
```

期待される出力:

```
sample_vicap: ELF 64-bit LSB executable, UCB RISC-V, RVC, double-float ABI, version 1 (SYSV), statically linked, ...
```

## CMakeLists.txt の詳細

`apps/sample_vicap/CMakeLists.txt` は以下を処理します:

- **MPP インクルードパス**: `mpp/include/`、`mpp/include/comm/`、`mpp/include/ioctl/`、`mpp/userapps/api/` のヘッダ
- **MPP 静的ライブラリ**: 全46個の MPP ライブラリを `--start-group` / `--end-group` で循環依存を解決してリンク
- **`-Wno-error`**: SDK サンプルコードに警告があるため、ツールチェーンが設定する `-Werror` をこのターゲットで無効化

## コマンドライン引数

```
./sample_vicap -mode <mode> -dev <dev> -sensor <sensor> -chn <chn> -ow <width> -oh <height> [options]
```

### グローバルオプション

| オプション | 説明 | デフォルト |
|-----------|------|-----------|
| `-mode <n>` | 動作モード: 0=online, 1=offline, 2=sw_tile | 0 |
| `-conn <n>` | コネクタタイプ（[`-conn` 詳細](#-conn-詳細)参照） | 0 |

### デバイスオプション（`-dev <n>` の後）

| オプション | 説明 | デフォルト |
|-----------|------|-----------|
| `-dev <n>` | VICAP デバイス ID (0, 1, 2) | 0 |
| `-sensor <n>` | センサータイプ（[`-sensor` (OV5647) 詳細](#-sensor-ov5647-詳細)参照） | — |
| `-ae <0\|1>` | AE の有効/無効 | 1 |
| `-awb <0\|1>` | AWB の有効/無効 | 1 |
| `-hdr <0\|1>` | HDR の有効/無効 | 0 |
| `-dw <0\|1>` | Dewarp の有効化 | 0 |
| `-mirror <n>` | ミラー: 0=なし, 1=水平, 2=垂直, 3=両方 | 0 |

### チャネルオプション（`-chn <n>` の後）

| オプション | 説明 | デフォルト |
|-----------|------|-----------|
| `-chn <n>` | 出力チャネル ID (0, 1, 2) | 0 |
| `-ow <width>` | 出力幅（16にアラインメント） | センサー幅 |
| `-oh <height>` | 出力高さ | センサー高さ |
| `-ofmt <n>` | ピクセルフォーマット: 0=YUV420, 1=RGB888, 2=RGB888P, 3=RAW | 0 |
| `-preview <0\|1>` | ディスプレイプレビューの有効化 | 1 |
| `-rotation <n>` | 回転: 0=0°, 1=90°, 2=180°, 3=270°, 4=なし, 17-19=GDMA回転 | 0 |
| `-crop <0\|1>` | クロップの有効化 | 0 |
| `-fps <n>` | フレームレート制限（0=無制限） | 0 |

### 使用例

```bash
# OV5647 1080p30 (CSI0) → HDMI 1080p60、垂直ミラー
./sample_vicap -mode 0 -conn 1 -dev 0 -sensor 24 -chn 0 -mirror 2

# OV5647 720p60 (CSI0) → HDMI 1080p60、垂直ミラー
./sample_vicap -mode 0 -conn 1 -dev 0 -sensor 44 -chn 0 -mirror 2

# OV5647 720p60 (CSI0) → HDMI 720p60、垂直ミラー
./sample_vicap -mode 0 -conn 5 -dev 0 -sensor 44 -chn 0 -mirror 2
```

### `-conn` 詳細

| 値 | チップ | インタフェース | 解像度 | FPS |
|----|--------|---------------|--------|-----|
| 0 | HX8399 | MIPI DSI 4-lane LCD | 1080x1920 | 30 |
| 1 | LT9611 | HDMI（MIPI-to-HDMI ブリッジ） | 1920x1080 | 60 |
| 2 | LT9611 | HDMI（MIPI-to-HDMI ブリッジ） | 1920x1080 | 30 |
| 3 | ST7701 | MIPI DSI 2-lane LCD | 480x800 | 30 |
| 4 | ILI9806 | MIPI DSI 2-lane LCD | 480x800 | 30 |
| 5 | LT9611 | HDMI（MIPI-to-HDMI ブリッジ） | 1280x720 | 60 |
| 6 | LT9611 | HDMI（MIPI-to-HDMI ブリッジ） | 1280x720 | 30 |
| 7 | LT9611 | HDMI（MIPI-to-HDMI ブリッジ） | 640x480 | 60 |

### `-sensor` (OV5647) 詳細

全モード 10-bit Linear、MIPI CSI 2-lane です。

| 値 | 解像度 | FPS | CSI ポート | 備考 |
|----|--------|-----|-----------|------|
| 21 | 1920x1080 | 30 | — | レガシー（CSI ポート未指定） |
| 22 | 2592x1944 | 10 | — | フル解像度、レガシー |
| 23 | 640x480 | 90 | — | レガシー |
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

### インタラクティブコマンド

実行中、アプリケーションはキーボードコマンドを受け付けます:

| キー | 動作 |
|------|------|
| `d` | 現在のフレームをファイルにダンプ |
| `h` | HDR バッファをダンプ |
| `s` | ISP AE ROI を設定 |
| `g` | ISP AE ROI を取得 |
| `t` | テストパターンの切り替え |
| `r` | ISP レジスタ設定をファイルにダンプ |
| `q` | 終了 |

## K230 への転送・実行

### SCP で転送

```bash
scp build/sample_vicap/sample_vicap root@<K230_IP_ADDRESS>:/sharefs/sample_vicap
```

### K230 bigcore (msh) で実行

K230 のシリアルコンソール (ACM1) で実行します:

```
msh /> /sharefs/sample_vicap -mode 0 -conn 1 -dev 0 -sensor 24 -chn 0 -mirror 2
```

!!! tip "シリアル接続"
    - **Bigcore (RT-Smart msh)**: `/dev/ttyACM1`、115200 bps

    ```bash
    minicom -D /dev/ttyACM1 -b 115200
    ```
