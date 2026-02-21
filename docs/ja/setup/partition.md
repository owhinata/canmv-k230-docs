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
    fat32appfs (p4) のデータは p5 ステージング方式により保護されます。

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

## Step 1: 現状確認・GPT 修正

K230 のシリアルコンソールに接続し、パーティション構成を確認します。
GPT が切り詰められている場合（Fix/Ignore? が表示された場合）は同時に修正します。

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
  expect "(parted)"

  send "print\r"
  expect {
    -re "Fix/Ignore\\?" {
      send "Fix\r"
      expect "(parted)"
      send "print\r"
      expect "(parted)"
    }
    "(parted)" { }
  }

  send "quit\r"
  expect "]#"
'
```

!!! info "Fix/Ignore? プロンプトについて"
    `print` 実行時に "Fix/Ignore?" と表示された場合、GPT バックアップヘッダがディスク末尾に
    配置されていません（この手順が必要な状態です）。`Fix` を送信してバックアップヘッダを修正します。
    Fix/Ignore? が出ない場合は GPT は正常（既に拡張済み等）です。

## Step 2: p5 一時パーティション作成

K230 の fat32appfs (p4) データを保護するため、p5 一時パーティションを作成します。
p5 の開始セクタ（134479872s）を新 p4 と同一にすることで、
Step 4 のパーティション再編後もディスク上の vfat データが保持されます。

!!! tip "p5 と新 p4 の開始セクタを必ず一致させること"
    p5 の開始セクタ（134479872s）と最終的な p4 の開始セクタを同一にします。
    これにより、Step 4 でパーティション再編後もディスク上の vfat データが
    保持されたまま GPT エントリのみ更新されます。
    セクタがズレるとデータ消失するため、**必ず 134479872s** を使用してください。

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
  expect "(parted)"

  send "mkpart fat32appfs_tmp fat32 134479872s 100%\r"
  expect "(parted)"

  send "quit\r"
  expect "]#"
'
```

## Step 3: p5 フォーマット・マウント・データコピー

fat32appfs (p4) の全データを p5 にコピーします。
データ量によって数分かかることがあります。コピー完了（"]#" 復帰）まで待機してください。

```sh
expect -c '
  log_user 1
  set timeout 300
  set serial [open /dev/ttyACM0 r+]
  fconfigure $serial -mode 115200,n,8,1 -translation binary -buffering none
  spawn -open $serial

  send "\r"
  expect "]#"

  send "mkfs.vfat -F 32 /dev/mmcblk1p5\r"
  expect "]#"

  send "mkdir -p /mnt/p5_tmp\r"
  expect "]#"

  send "mount /dev/mmcblk1p5 /mnt/p5_tmp\r"
  expect "]#"

  send "cp -a /sharefs/. /mnt/p5_tmp/\r"
  expect "]#"
'
```

## Step 4: アンマウント・パーティション再編

p5 と /sharefs をアンマウントし、parted でパーティションを再編します。
新しい p4 は p5 と同一の開始セクタ（134479872s）で作成するため、
p5 のコピー済みデータがそのまま保持されます。フォーマットは不要です。

!!! danger "parted -s（スクリプトモード）は使用不可"
    parted -s はルートファイルシステム上のパーティション削除を拒否します。
    必ず **対話モード** を使用してください。

!!! warning "開始セクタ 262144 は変更するな"
    p3（rootfs）の開始セクタを変更すると、既存の ext4 データが破壊されます。
    **必ず 262144s から開始してください。**

!!! warning "パーティション名は元の名前と一致させること"
    parted の mkpart で指定する名前を **必ず元の名前に一致させてください**。
    例えば p4 の名前は fat32appfs であり、sharefs に変更してはなりません。
    誤った名前にすると fstab や起動スクリプトが破損する可能性があります。

```sh
expect -c '
  log_user 1
  set timeout 120
  set serial [open /dev/ttyACM0 r+]
  fconfigure $serial -mode 115200,n,8,1 -translation binary -buffering none
  spawn -open $serial

  send "\r"
  expect "]#"

  send "umount /mnt/p5_tmp\r"
  expect "]#"

  send "umount /sharefs\r"
  expect "]#"

  send "parted /dev/mmcblk1\r"
  expect "(parted)"

  # p5 削除
  send "rm 5\r"
  expect "(parted)"

  # p4 削除
  send "rm 4\r"
  expect "(parted)"

  # p3 削除（ルートFS使用中のため Yes/No? + Ignore/Cancel? が出る）
  send "rm 3\r"
  expect -re "Yes/No\\?"
  send "Yes\r"
  expect {
    -re "Ignore/Cancel\\?" {
      send "Ignore\r"
      expect "(parted)"
    }
    "(parted)" { }
  }

  # p3 再作成（開始セクタ 262144s 維持、64GB）
  send "mkpart rootfs ext4 262144s 134479871s\r"
  expect {
    -re "Ignore/Cancel\\?" {
      send "Ignore\r"
      expect "(parted)"
    }
    "(parted)" { }
  }

  # p4 再作成（p5 と同一開始セクタ 134479872s、残り全部）
  send "mkpart fat32appfs fat32 134479872s 100%\r"
  expect "(parted)"

  send "print\r"
  expect "(parted)"

  send "quit\r"
  expect "]#"
'
```

!!! info "カーネル通知失敗エラーについて"
    rm 3 や mkpart 後に
    "Error: Partition(s) ... have been written, but we have been unable to inform the kernel"
    が表示された場合、Ignore を選択してください。
    カーネルへの通知は Step 5 の再起動で行われます。

## Step 5: 再起動

カーネルが新しいパーティションテーブルを認識するために再起動します。
ブート完了（約 35 秒）まで待機します。

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
  expect {
    -re "login:" {
      send "root\r"
      expect "]#"
    }
    "]#" { }
    timeout { puts "TIMEOUT: reboot did not complete"; exit 1 }
  }
'
```

## Step 6: rootfs 拡張（resize2fs）

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

## Step 7: 確認

```sh
expect -c '
  log_user 1
  set timeout 30
  set serial [open /dev/ttyACM0 r+]
  fconfigure $serial -mode 115200,n,8,1 -translation binary -buffering none
  spawn -open $serial

  send "\r"
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

## トラブルシューティング

### resize2fs がエラーを返す場合

```sh
# fsck を実行してから resize2fs を試す
e2fsck -f /dev/mmcblk1p3
resize2fs /dev/mmcblk1p3
```

### パーティション変更後もサイズが変わらない

Step 5 の再起動が完了していることを確認してください。
カーネルは再起動するまで古いパーティションテーブルを保持します。

### parted で "Error: Partition(s) 3, 4 on /dev/mmcblk1 have been written..."

このメッセージはカーネルにパーティション変更を通知できなかったことを示します。
**Step 5 の再起動で解決します。** 再起動前に resize2fs を実行しても正常に動作します。

### parted で p3 削除時に "Partition is being used" 警告が出る

rootfs がマウント中のため警告が出ます。Yes を選択して続行してください。
再起動後にカーネルが新しいパーティションテーブルを認識します。
