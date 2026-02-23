# rtt-ctrl — Linux から RT-Smart を操作する

`rtt-ctrl` は Linux (littlecore) から RT-Smart (bigcore) の msh シェルコマンドをリモート実行するためのユーティリティです。
IPCM (Inter-Processor Communication) を介して RT-Smart カーネル内の `rc_server` にコマンドを送信し、実行結果を受け取ります。

## 仕組み

### アーキテクチャ

```
Linux (littlecore)                    RT-Smart (bigcore)
┌──────────────┐                     ┌──────────────┐
│ rtt-ctrl     │  IPCM port 7       │ rc_server    │
│ (user app)   │ ──────────────────→ │ (kernel)     │
│              │ ←────────────────── │              │
└──────┬───────┘   /dev/ipcm_user   └──────┬───────┘
       │                                    │
       │                              msh_exec(cmd)
       │                                    │
  response (ret code)              RT-Smart msh シェル
```

### 通信の流れ

1. Linux 側: `rtt-ctrl` コマンドが `/dev/ipcm_user` を open
2. IPCM port 7 で RT-Smart の `rc_server` に接続
3. `RC_CMD_EXEC` メッセージでコマンド文字列を送信（最大 128 バイト）
4. RT-Smart 側: `rc_server` が `msh_exec()` でコマンド実行
5. 実行結果（リターンコード）を応答として返却

### 有効化

`scripts/build_sdk.sh` はデフォルトで `RT_USING_RTT_CTRL` を有効化します。

```bash
# デフォルト（rtt-ctrl 有効）
scripts/build_sdk.sh

# rtt-ctrl を無効にする場合
scripts/build_sdk.sh --no-rtt-ctrl
```

内部的には defconfig ソース (`configs/common_rttlinux.config`) に `#define RT_USING_RTT_CTRL` を追加します。
ビルド時に `menuconfig_to_code.sh` がこのファイルを `rtconfig.h` にコピーするため、`rtconfig.h` を直接編集しても上書きされます。
未定義の場合、weak symbol のスタブが使われ `"no rtt ctrl device library!"` と表示されます。

### ソースコード

| ファイル (SDK 内パス) | 役割 |
|----------------------|------|
| `src/common/cdk/kernel/ipcm/class/rtt-ctrl/rc_command.h` | コマンドプロトコル定義 (`RC_CMD_EXEC` 等) |
| `src/common/cdk/kernel/ipcm/class/rtt-ctrl/rc_common.h` | 共通ヘッダ |
| `src/common/cdk/kernel/ipcm/class/rtt-ctrl/rt-smart/rc_server.c` | RT-Smart 側サーバ実装 |
| `src/common/cdk/kernel/ipcm/class/rtt-ctrl/linux/rc_client.c` | Linux 側クライアント実装 |
| `src/common/cdk/kernel/ipcm/class/rtt-ctrl/server_ipc.c` | RT-Smart 側 IPC 処理 |
| `src/common/cdk/kernel/ipcm/class/rtt-ctrl/client_ipc.c` | Linux 側 IPC 処理 |
| `src/big/rt-smart/kernel/bsp/maix3/board/ipcm/rtt_ctrl_init.c` | RT-Smart カーネル初期化 |

## 使い方

### 基本

```bash
rtt-ctrl "コマンド"
```

### 例

```bash
# RT-Smart のスレッド一覧を表示
rtt-ctrl "list_thread"

# メモリ使用状況を表示
rtt-ctrl "free"

# 利用可能な msh コマンド一覧を表示
rtt-ctrl "help"
```

### 注意事項

- コマンド文字列は **128 バイト以内**
- 接続タイムアウト: 約 1 秒
- コマンド応答タイムアウト: 約 1 秒
- IPCM ドライバがロードされている必要がある (`/dev/ipcm_user` が存在すること)

## RT-Smart イメージのみを更新する

`build_sdk.sh` でフルビルド済みの環境で `RT_USING_RTT_CTRL` の有効/無効を切り替えたい場合、`rtconfig.h` を直接編集して部分ビルドできます。

!!! note
    フルビルド時は `menuconfig_to_code.sh` が `common_rttlinux.config` → `rtconfig.h` にコピーするため直接編集は上書きされます。
    部分ビルド (`make rt-smart-kernel` 等) ではこのコピーは発生しないため、`rtconfig.h` の直接編集が有効です。

### 部分ビルド

Docker コンテナ内の `k230_sdk/` ディレクトリで実行してください。

```bash
# rtconfig.h を編集して RT_USING_RTT_CTRL を追加/削除
vi src/big/rt-smart/kernel/bsp/maix3/rtconfig.h

# 1. RT-Smart カーネル再ビルド — rtconfig.h の変更を反映
make rt-smart-kernel

# 2. OpenSBI 再ビルド — RT-Smart バイナリをペイロードとして組み込み
make big-core-opensbi

# 3. 最終 SD カードイメージ生成
make build-image
```

`init.sh` やユーザアプリの変更を伴わないため、`mpp` や `rt-smart-apps` のビルドは不要です。

### SD カードへの書き込み

フルイメージを書き直す代わりに、RT-Smart パーティションのみを更新できます。
SD カードの GPT パーティション構造は以下の通りです:

| パーティション | オフセット | サイズ | 内容 |
|--------------|-----------|--------|------|
| rtt | 10 MB | 20 MB | RT-Smart (big-core) ファームウェア |
| linux | 30 MB | 50 MB | Linux (little-core) ファームウェア |
| rootfs | 128 MB | 可変 | ext4 ルートファイルシステム |
| fat32appfs | rootfs の後 | 残り | FAT32 アプリケーション領域 |

ビルド後、`rtt_system.bin` を SD カードの rtt パーティションに `dd` で書き込みます:

```bash
# /dev/sdX は SD カードのデバイスを指定（例: /dev/sdb）
sudo dd if=output/k230_canmv_defconfig/images/big-core/rtt_system.bin \
    of=/dev/sdX bs=1M seek=10 conv=notrunc status=progress
```

!!! warning
    `/dev/sdX` はお使いの環境の SD カードデバイスに読み替えてください。誤ったデバイスを指定するとデータが破壊されます。

## 関連ファイル

| ファイル | 役割 |
|---------|------|
| `scripts/build_sdk.sh` | `RT_USING_RTT_CTRL` の有効化処理 |
| `k230_sdk/src/big/rt-smart/kernel/bsp/maix3/configs/common_rttlinux.config` | defconfig ソース（ビルド時に `rtconfig.h` へコピーされる） |
| `k230_sdk/src/big/rt-smart/kernel/bsp/maix3/rtconfig.h` | RT-Smart カーネル設定（自動生成） |
