# sample_vicap ビルド

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
| `vo_test_case.c` | VO ディスプレイヘルパー関数 — DSI 初期化、レイヤー/OSD 作成、コネクタ設定 |
| `vo_test_case.h` | VO ヘルパー関数の型定義とヘッダ |
| `vo_bind_test.c` | VVI-VO バインドテスト（`vo_layer_bind_config`、`vdss_bind_vo_config` で使用） |

これらのファイルは SDK からコピーしたものです:

- `sample_vicap.c`: `k230_sdk/src/big/mpp/userapps/sample/sample_vicap/`
- `vo_test_case.c`, `vo_test_case.h`, `vo_bind_test.c`: `k230_sdk/src/big/mpp/userapps/sample/sample_vo/`

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
| `-conn <n>` | コネクタ: 0=HX8377, 1=LT9611-1080p60, 2=LT9611-1080p30, 3=ST7701, 4=ILI9806 | 0 |

### デバイスオプション（`-dev <n>` の後）

| オプション | 説明 | デフォルト |
|-----------|------|-----------|
| `-dev <n>` | VICAP デバイス ID (0, 1, 2) | 0 |
| `-sensor <n>` | センサータイプ（SDK のセンサーリスト参照） | — |
| `-ae <0\|1>` | AE の有効/無効 | 1 |
| `-awb <0\|1>` | AWB の有効/無効 | 1 |
| `-hdr <0\|1>` | HDR の有効/無効 | 0 |
| `-dw <0\|1>` | Dewarp の有効化 | 0 |

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
# OV9732 センサー、640x480 出力、90度回転、プレビュー有効
./sample_vicap -mode 0 -dev 0 -sensor 0 -chn 0 -ow 640 -oh 480 -preview 1 -rotation 1
```

## 処理フロー

アプリケーションは以下のパイプラインに沿って動作します:

```
センサー → VICAP (dev) → VICAP (chn) → [GDMA 回転] → VO (layer) → ディスプレイ
```

### 初期化シーケンス

1. **引数のパース** — デバイス/チャネルパラメータの設定
2. **コネクタ情報の取得** — ディスプレイ解像度の取得
3. **センサー情報の取得** — カメラ解像度の取得
4. **VICAP デバイス属性の設定** — 入力、ISP パイプライン、動作モードの設定
5. **VO コネクタの初期化** — ディスプレイハードウェアのセットアップ
6. **VB (Video Buffer) の初期化** — バッファプールの確保
7. **VICAP チャネル属性の設定** — 出力フォーマット、サイズ、クロップの設定
8. **VICAP と VO のバインド** — キャプチャ出力をディスプレイレイヤーに接続（オプションで GDMA 経由の回転）
9. **VO レイヤーの設定** — ディスプレイレイヤーのサイズ、位置、回転の設定
10. **VICAP ストリームの開始** — キャプチャ開始
11. **VO の有効化** — ディスプレイ出力の開始

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

!!! warning "/sharefs/ について"
    正しい転送先は `/sharefs/sample_vicap` であり、`/root/sharefs/sample_vicap` **ではありません**。
    `/sharefs/` は vfat パーティション (`/dev/mmcblk1p4`) で、bigcore から直接アクセスできます。

### K230 bigcore (msh) で実行

K230 のシリアルコンソール (ACM1) で実行します:

```
msh /> /sharefs/sample_vicap -mode 0 -dev 0 -sensor 0 -chn 0 -ow 640 -oh 480 -preview 1 -rotation 1
```

!!! tip "シリアル接続"
    - **Bigcore (RT-Smart msh)**: `/dev/ttyACM1`、115200 bps

    ```bash
    minicom -D /dev/ttyACM1 -b 115200
    ```
