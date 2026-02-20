# CanMV K230 ガイド

CanMV K230 v1.1 のセットアップと使い方をまとめたガイドです。

## ボード概要

| 項目 | 値 |
|------|-----|
| ボード | CanMV K230 v1.1 |
| SoC | Kendryte K230 (RISC-V デュアルコア) |
| OS | Linux canaan 5.10.4 (riscv64) |
| WiFi チップ | Broadcom (bcmdhd ドライバ、2.4GHz のみ) |
| シリアル接続 | USB 経由 `/dev/ttyACM0` 115200baud |

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
`expect` を使ってシリアルコンソールにアクセスできます。

```sh
expect -c '
  log_user 1
  set timeout 10
  set serial [open /dev/ttyACM0 r+]
  fconfigure $serial -mode 115200,n,8,1 -translation binary -buffering none
  spawn -open $serial

  send "\r"
  expect "]#"

  send "uname -a\r"
  expect "]#"
'
```

!!! tip "expect のインストール"
    Debian/Ubuntu の場合: `sudo apt install expect`

ログインプロンプトが表示されたら、`root` でログインします（パスワードなし）。
