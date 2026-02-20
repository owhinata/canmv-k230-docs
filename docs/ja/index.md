# CanMV K230 ガイド

CanMV K230 v1.1 のセットアップと使い方をまとめたガイドです。

## ボード概要

| 項目 | 値 |
|------|-----|
| ボード | CanMV K230 v1.1 |
| SoC | Kendryte K230 |
| RAM | 512 MB LPDDR3 |
| WiFi チップ | Broadcom (bcmdhd ドライバ、2.4GHz のみ) |
| シリアル接続 | USB 経由 `/dev/ttyACM0` 115200baud |

### デュアルコア構成

K230 は 2 つの Xuantie C908 (RISC-V 64-bit) コアを搭載し、それぞれ異なる OS が動作するヘテロジニアス構成です。

| | Big コア (CPU1) | Little コア (CPU0) |
|------|----------------|-------------------|
| クロック | 1.6 GHz | 800 MHz |
| OS | RT-Smart (リアルタイム OS) | Linux 5.10.4 |
| 役割 | AI 推論 (KPU)、メディア処理 | システム制御、ネットワーク、ユーザー操作 |
| 特徴 | RISC-V Vector 1.0 (128-bit) | Big コアの起動を制御 |

両コア間は共有ファイルシステム (`/sharefs`) を通じて通信します。

!!! info "KPU (AI アクセラレータ)"
    INT8/INT16 推論に対応し、nncase コンパイラで ONNX/TFLite モデルを kmodel に変換して実行します。
    KPU が対応しないオペレータ（softmax 等）は Big コアの RVV 1.0 で高速に処理されます。

## リンク集

### 公式ドキュメント・SDK

- [K230 SDK (GitHub)](https://github.com/kendryte/k230_sdk) -- Linux + RT-Smart デュアル OS SDK
- [K230 ドキュメント (GitHub)](https://github.com/kendryte/k230_docs) -- SDK リファレンスドキュメント
- [K230 SDK ドキュメント (Web)](https://www.kendryte.com/k230/en/dev/index.html) -- ハードウェア設計ガイド・データシート・API リファレンス
- [K230 Linux SDK (GitHub)](https://github.com/kendryte/k230_linux_sdk) -- Linux 専用 SDK（Debian / Ubuntu イメージ対応）

### CanMV (MicroPython)

- [CanMV K230 ファームウェア (GitHub)](https://github.com/kendryte/canmv_k230) -- MicroPython ファームウェア（リリースページからイメージをダウンロード可能）
- [CanMV K230 ドキュメント (Web)](https://www.kendryte.com/k230_canmv/en/main/index.html) -- MicroPython API リファレンス・サンプル集

### AI 開発

- [nncase (GitHub)](https://github.com/kendryte/nncase) -- ONNX / TFLite モデルを KPU 用 kmodel に変換するコンパイラ
- [K230 AI 開発チュートリアル](https://www.kendryte.com/ai_docs/en/dev/Development_Basics.html) -- モデル推論・AI2D 前処理・デプロイの解説
- [K230 学習スクリプト (GitHub)](https://github.com/kendryte/K230_training_scripts) -- モデル学習からオンボード推論までの E2E サンプル

### ハードウェア

- [K230 製品ページ](https://www.kendryte.com/en/proDetail/230) -- 回路図・PCB データ・IOMUX ツール

### ファームウェアダウンロード

- [Canaan ファームウェア一覧](https://kendryte-download.canaan-creative.com/developer/k230/) -- CanMV / Debian / Ubuntu 等のビルド済みイメージ

## OS イメージの準備

### ダウンロード

以下の URL から SD カード用イメージをダウンロードします。

```
https://kendryte-download.canaan-creative.com/k230/release/sdk_images/v2.0/k230_canmv_defconfig/CanMV-K230_sdcard_v2.0_nncase_v2.10.0.img.gz
```

```sh
wget https://kendryte-download.canaan-creative.com/k230/release/sdk_images/v2.0/k230_canmv_defconfig/CanMV-K230_sdcard_v2.0_nncase_v2.10.0.img.gz
gunzip CanMV-K230_sdcard_v2.0_nncase_v2.10.0.img.gz
```

### SD カードへの書き込み

!!! warning "デバイス名の確認"
    `of=` に指定するデバイス名は環境によって異なります（`/dev/sda`, `/dev/sdb`, `/dev/mmcblk0` 等）。
    **書き込み先を間違えるとデータが消失します。** `lsblk` 等で必ず確認してください。

```sh
sudo dd if=CanMV-K230_sdcard_v2.0_nncase_v2.10.0.img of=/dev/sdX bs=1M oflag=sync
```

## ボードの起動

1. 書き込み済み SD カードを K230 に挿入
2. USB ケーブルでホスト PC に接続
3. 電源が入ると自動的に Linux が起動

## シリアル接続

USB 接続すると、ホスト PC 上に `/dev/ttyACM0` が現れます。
`picocom` を使ってシリアルコンソールにアクセスできます。

```sh
picocom -b 115200 /dev/ttyACM0
```

!!! tip "picocom のインストール"
    Debian/Ubuntu の場合: `sudo apt install picocom`

!!! info "picocom の終了"
    `Ctrl-a Ctrl-x` で picocom を終了できます。

ログインプロンプトが表示されたら、`root` でログインします（パスワードなし）。
