# SDカード パーティション拡張

CanMV K230 のデフォルト GPT はセクタ 1,048,575（約 512MB）で打ち切られています。
この手順で **rootfs を 64GB、fat32appfs を残り全部（FAT32）** に拡張します。

拡張後の構成:

| パーティション | ファイルシステム | サイズ |
|---------------|----------------|--------|
| rootfs (mmcblk1p3) | ext4 | 64 GB |
| fat32appfs (mmcblk1p4) | FAT32 | 約 174 GB |

## 前提条件

- シリアル接続で K230 にアクセスできること（`/dev/ttyACM0`、115200 bps）
- ホスト側に `expect` がインストール済みであること
- 使用ツール（K230 上）: `parted`, `resize2fs`, `mkfs.vfat`

!!! danger "fdisk は使用不可"
    K230 の busybox fdisk は **GPT 非対応** です。必ず `parted` を使用してください。

!!! warning "データバックアップを推奨"
    この手順では rootfs パーティション (p3) を再作成します。
    開始セクタを変更しないためデータは保護されますが、事前バックアップを推奨します。
    **fat32appfs (p4) のデータを保持したい場合は、後述の「p5 ステージング方式」を使用してください。**

## パーティション構成（変更前→後）

### 変更前

```
Disk /dev/mmcblk1: 499744768 sectors (約 238GB)
Logical sector size: 512 bytes

#  Start (sector)    End (sector)   Size    Name
1           20480          61439    20.0M   rtt
2           61440         163839    50.0M   linux
3          262144         524287    128M    rootfs
4          524288        1048575    256M    fat32appfs
```

GPT バックアップヘッダがセクタ 1,048,575 で終端しており、実際のディスク末尾 (499,744,734) まで未使用です。

### 変更後

```
#  Start (sector)    End (sector)   Size    Name
1           20480          61439    20.0M   rtt          ← 変更なし
2           61440         163839    50.0M   linux         ← 変更なし
3          262144      134479871    64GB    rootfs        ← 64GB に拡張
4       134479872      499742719    174G    fat32appfs    ← 残り全部
```

## Step 1: 現状確認

K230 のシリアルコンソールに接続し、現在のパーティション構成を確認します。

```sh
expect -c '
  log_user 1
  set timeout 30
  set serial [open /dev/ttyACM0 r+]
  fconfigure $serial -mode 115200,n,8,1 -translation binary -buffering none
  spawn -open $serial

  send "\r"
  expect "]#"

  send "parted /dev/mmcblk1 unit s print\r"
  expect "]#"
'
```

`Last sector:` が `1048575` と表示される場合、GPT が切り詰められています（この手順が必要です）。

## Step 2: GPT 修正（parted 対話モード）

GPT バックアップヘッダをディスク末尾に移動します。

```sh
expect -c '
  log_user 1
  set timeout 30
  set serial [open /dev/ttyACM0 r+]
  fconfigure $serial -mode 115200,n,8,1 -translation binary -buffering none
  spawn -open $serial

  send "\r"
  expect "]#"

  send "parted /dev/mmcblk1\r"
  expect -re "(Fix|Ignore)"
  send "Fix\r"
  expect "(parted)"
  send "quit\r"
  expect "]#"
'
```

!!! info "Fix/Ignore プロンプトについて"
    `parted` 起動時に "Fix/Ignore?" と聞かれます。`Fix` を入力することで GPT バックアップヘッダがディスク末尾に修正されます。

## Step 3: パーティション再作成（parted 対話モード）

!!! danger "parted -s（スクリプトモード）は使用不可"
    `parted -s` はルートファイルシステム上のパーティション削除を拒否します。
    必ず **対話モード** を使用し、"Yes" を送信してください。

!!! warning "パーティション名は元の名前と一致させること"
    `parted` の `mkpart` で指定する名前を **必ず元の名前に一致させてください**。
    例えば p4 の名前は `fat32appfs` であり、`sharefs` に変更してはなりません。
    誤った名前にすると fstab や起動スクリプトが破損する可能性があります。

p4（fat32appfs）を削除 → p3（rootfs）を削除 → p3 を 64GB で再作成 → p4 を残り全部で再作成します。

```sh
expect -c '
  log_user 1
  set timeout 60
  set serial [open /dev/ttyACM0 r+]
  fconfigure $serial -mode 115200,n,8,1 -translation binary -buffering none
  spawn -open $serial

  send "\r"
  expect "]#"

  send "parted /dev/mmcblk1\r"
  expect -re "(Fix|\\(parted\\))"
  if {[string match "*Fix*" $expect_out(0,string)]} {
    send "Fix\r"
    expect "(parted)"
  }

  # p4 削除
  send "rm 4\r"
  expect "(parted)"

  # p3 削除（ルートFS警告が出る）
  send "rm 3\r"
  expect -re "(Yes/No|\\(parted\\))"
  if {[string match "*Yes*" $expect_out(0,string)]} {
    send "Yes\r"
    expect "(parted)"
  }

  # p3 再作成（開始セクタは元のまま 262144 を維持、64GB）
  send "mkpart rootfs ext4 262144s 134479871s\r"
  expect "(parted)"

  # p4 再作成（残り全部）
  send "mkpart fat32appfs fat32 134479872s 100%\r"
  expect "(parted)"

  # 確認
  send "print\r"
  expect "(parted)"

  send "quit\r"
  expect "]#"
'
```

!!! warning "開始セクタ 262144 は変更するな"
    p3（rootfs）の開始セクタを変更すると、既存の ext4 データが破壊されます。
    **必ず 262144s から開始してください。**

## Step 4: 再起動

カーネルが新しいパーティションテーブルを認識するために再起動します。

```sh
expect -c '
  log_user 1
  set timeout 120
  set serial [open /dev/ttyACM0 r+]
  fconfigure $serial -mode 115200,n,8,1 -translation binary -buffering none
  spawn -open $serial

  send "\r"
  expect "]#"

  send "reboot\r"
  expect "]#"
'
```

## Step 5: rootfs 拡張（resize2fs）

再起動後、`resize2fs` で ext4 ファイルシステムを新しいパーティションサイズまで拡張します。
**マウント中（オンライン）でも実行可能です。**

```sh
expect -c '
  log_user 1
  set timeout 120
  set serial [open /dev/ttyACM0 r+]
  fconfigure $serial -mode 115200,n,8,1 -translation binary -buffering none
  spawn -open $serial

  send "\r"
  expect "]#"

  send "resize2fs /dev/mmcblk1p3\r"
  expect "]#"
'
```

成功時の出力例:

```
The filesystem on /dev/mmcblk1p3 is now 67108864 blocks long.
```

## Step 6: fat32appfs フォーマット

p4 を FAT32 でフォーマットします。

!!! warning "fat32appfs のデータは消去されます"
    `mkfs.vfat` は p4 の全データを消去します。必要なデータは事前にバックアップしてください。
    **fat32appfs にデータが残っている場合は、次節の「p5 ステージング方式」を使用してください。**

```sh
expect -c '
  log_user 1
  set timeout 60
  set serial [open /dev/ttyACM0 r+]
  fconfigure $serial -mode 115200,n,8,1 -translation binary -buffering none
  spawn -open $serial

  send "\r"
  expect "]#"

  send "umount /dev/mmcblk1p4\r"
  expect "]#"

  send "mkfs.vfat -F 32 /dev/mmcblk1p4\r"
  expect "]#"
'
```

## Step 7: 確認

再起動して最終確認します。

```sh
expect -c '
  log_user 1
  set timeout 120
  set serial [open /dev/ttyACM0 r+]
  fconfigure $serial -mode 115200,n,8,1 -translation binary -buffering none
  spawn -open $serial

  send "\r"
  expect "]#"

  send "reboot\r"
  expect "]#"

  send "df -h\r"
  expect "]#"
'
```

期待される出力:

```
Filesystem        Size  Used Avail Use% Mounted on
/dev/root        62.0G   72.2M  61.9G   0% /
/dev/mmcblk1p4  174.1G   92.1M  174.0G   0% /sharefs
```

!!! info "自動マウント"
    `/sharefs` は起動時に自動マウントされます（`/etc/fstab` に設定済み）。

## p5 ステージング方式（fat32appfs データ保護）

fat32appfs (p4) にデータが残っており、かつ **rootfs の空き容量 < fat32appfs の使用量** の場合、
rootfs への退避が不可能です。この場合、p5 一時パーティションを経由してデータを保護します。

!!! tip "重要: p5 と新 p4 の開始セクタを一致させること"
    p5 の開始セクタ（134479872s）と p4 再作成時の開始セクタを同一にすることで、
    vfat データはディスク上に保持されたまま GPT エントリのみ変更できます。
    セクタがズレるとデータ消失するため、**開始セクタは必ず 134479872s** を使用してください。

### p5 ステージング手順

**1. p5 を一時パーティションとして作成**（最終的な p4 と同じ開始セクタ）

```sh
# parted 対話モードで実行
parted /dev/mmcblk1
(parted) mkpart fat32appfs_tmp fat32 134479872s 100%
(parted) quit
```

**2. p5 をフォーマット・マウントして fat32appfs のデータをコピー**

```sh
mkfs.vfat -F 32 /dev/mmcblk1p5
mkdir -p /mnt/p5_tmp
mount /dev/mmcblk1p5 /mnt/p5_tmp
cp -a /sharefs/. /mnt/p5_tmp/
```

**3. アンマウント後にパーティション再編**

```sh
umount /mnt/p5_tmp
umount /sharefs
```

```sh
# parted 対話モードで実行
parted /dev/mmcblk1
(parted) rm 5
(parted) rm 4
(parted) rm 3
(parted) mkpart rootfs ext4 262144s 134479871s
(parted) mkpart fat32appfs fat32 134479872s 100%
(parted) quit
```

**4. 再起動して resize2fs を実行**（Step 4〜5 と同様）

p5 と新 p4 の開始セクタが一致しているため、vfat データは保持されたままです。
新 p4 をフォーマットする必要はありません。

## トラブルシューティング

### resize2fs がエラーを返す場合

```sh
# fsck を実行してから resize2fs を試す
e2fsck -f /dev/mmcblk1p3
resize2fs /dev/mmcblk1p3
```

### パーティション変更後もサイズが変わらない

Step 4 の再起動が完了していることを確認してください。
カーネルは再起動するまで古いパーティションテーブルを保持します。

### parted で "Error: Partition(s) 3, 4 on /dev/mmcblk1 have been written..."

このメッセージはカーネルにパーティション変更を通知できなかったことを示します。
**Step 4 の再起動で解決します。** 再起動前に resize2fs/mkfs.vfat を実行しても正常に動作します。

### parted で p3 削除時に "Partition is being used" 警告が出る

rootfs がマウント中のため警告が出ます。`Ignore` で続行してください。
再起動後にカーネルが新しいパーティションテーブルを認識します。
