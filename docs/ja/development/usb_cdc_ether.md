# USB CDC-Ether (littlecore Linux)

K230 の littleコア (Linux) で USB CDC Ethernet (ECM/RNDIS) を使い、USB ケーブル経由のネットワーク接続を実現するための調査結果と手順です。

!!! warning "現状では使用不可"
    デフォルトのカーネル設定では CDC Ethernet 機能がすべて無効化されています。使用するにはカーネルの再ビルドが必要です。

## ハードウェア状況

K230 は DWC2 OTG コントローラを搭載しており、USB デバイスモードに対応しています。

| 項目 | 値 |
|------|-----|
| UDC コントローラ | `91500000.usb-otg` |
| DWC2 ドライバ | ロード済み (91500000, 91540000) |
| usbotg0 | OTG モード (デバイスモード可能) |
| usbotg1 | ホスト専用 (`dr_mode = "host"`) |

デバイスツリー (`k230_canmv.dts`) で `usbotg0` が OTG モードで有効になっており、USB ガジェットとして使用可能です。

## 現在のカーネル設定

USB ガジェットフレームワーク自体は有効ですが、CDC Ethernet 関連はすべて無効です。

**有効な機能:**

```
CONFIG_USB_DWC2=y
CONFIG_USB_DWC2_DUAL_ROLE=y
CONFIG_USB_GADGET=y
CONFIG_USB_CONFIGFS=y
CONFIG_USB_CONFIGFS_MASS_STORAGE=y
CONFIG_USB_CONFIGFS_F_LB_SS=y
CONFIG_USB_CONFIGFS_F_HID=y
CONFIG_USB_CONFIGFS_F_UVC=y
```

**無効な機能 (CDC Ethernet 関連):**

```
# CONFIG_USB_CONFIGFS_NCM is not set
# CONFIG_USB_CONFIGFS_ECM is not set
# CONFIG_USB_CONFIGFS_ECM_SUBSET is not set
# CONFIG_USB_CONFIGFS_RNDIS is not set
# CONFIG_USB_CONFIGFS_EEM is not set
# CONFIG_USB_ETH is not set
```

## 有効化手順

### 1. defconfig の変更

対象ファイル: `k230_sdk/src/little/linux/arch/riscv/configs/k230_canmv_defconfig`

ECM (Linux/macOS 標準対応) を追加:

```
CONFIG_USB_CONFIGFS_ECM=y
```

Windows 対応も必要な場合は RNDIS も追加:

```
CONFIG_USB_CONFIGFS_RNDIS=y
```

既存の USB configfs 設定 (`CONFIG_USB_CONFIGFS_F_UVC=y` の行) の後に追記してください。

### 2. カーネル再ビルド

SDK Docker 環境内で:

```bash
make linux       # Linux カーネル再ビルド
make build-image # SD カードイメージ再生成
```

!!! note "SDK ビルドについて"
    K230 SDK のビルド手順は [SDK ビルド](sdk_build.md) を参照してください。

### 3. 実機での configfs セットアップ

カーネル再ビルド後、実機上で configfs をマウントし ECM ガジェットを作成します。

```bash
#!/bin/sh
# gadget-ecm.sh — USB CDC ECM ガジェット セットアップ

mount -t configfs none /sys/kernel/config

mkdir -p /sys/kernel/config/usb_gadget/g1
cd /sys/kernel/config/usb_gadget/g1

echo 0x1d6b > idVendor    # Linux Foundation
echo 0x0104 > idProduct    # Multifunction Composite Gadget

mkdir -p strings/0x409
echo "Canaan Inc." > strings/0x409/manufacturer
echo "CDC Ether" > strings/0x409/product
echo "20230618" > strings/0x409/serialnumber

mkdir -p configs/c.1/strings/0x409
echo "ECM" > configs/c.1/strings/0x409/configuration

mkdir -p functions/ecm.usb0
ln -s functions/ecm.usb0 configs/c.1/

echo 91500000.usb-otg > UDC

ifconfig usb0 192.168.7.2 netmask 255.255.255.0 up
```

### 4. ホスト側の設定

K230 を USB ケーブルでホスト PC に接続した後、ホスト側で IP アドレスを設定します。

```bash
# Linux ホストの場合
sudo ifconfig usb0 192.168.7.1 netmask 255.255.255.0 up

# 疎通確認
ping 192.168.7.2
```

## ECM vs RNDIS

| 方式 | Linux | macOS | Windows | 備考 |
|------|-------|-------|---------|------|
| ECM | ドライバ不要 | ドライバ不要 | 非対応 | 推奨 |
| RNDIS | ドライバ不要 | 非対応 | ドライバ不要 | Windows 向け |

Linux/macOS のみの場合は ECM だけで十分です。Windows 対応が必要な場合は RNDIS も有効にしてください。

## 参考

- 既存のガジェットスクリプト例:
    - `k230_sdk/src/little/buildroot-ext/package/usb_test/src/gadget-storage.sh`
    - `k230_sdk/src/little/buildroot-ext/package/usb_test/src/gadget-hid.sh`
