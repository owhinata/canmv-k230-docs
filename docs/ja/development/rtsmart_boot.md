# RT-Smart 起動カスタマイズ

RT-Smart (bigcore) の起動シーケンスと、msh シェル待ちイメージの作成方法を説明します。

## RT-Smart 起動シーケンス

K230 の bigcore (RT-Smart) は以下の順序で起動します。

```
RT-Smart カーネル起動
  ↓
コンポーネント初期化 (INIT_BOARD_EXPORT → ... → INIT_APP_EXPORT)
  ├── finsh_system_init() → msh シェルスレッド "tshell" を作成・起動
  └── ...
  ↓
ROMFS を / にマウント
  ↓
main() 実行 → msh_exec("/bin/init.sh")
  ↓
init.sh の内容に従ってアプリケーションを起動
```

### msh シェルスレッド

msh シェルスレッド (`tshell`) はコンポーネント初期化の段階で自動的に起動されます（`finsh_system_init`）。
`main()` がブロッキングアプリケーションを起動しなければ、シリアルコンソールで msh シェルが対話的に使えます。

### main() の動作

`main()` は `RT_SHELL_PATH`（デフォルト: `/bin/init.sh`）を `msh_exec()` で実行します。

```c
// k230_sdk/src/big/rt-smart/kernel/bsp/maix3/applications/main.c
#ifndef RT_SHELL_PATH
#define RT_SHELL_PATH "/bin/init.sh"
#endif

int main(void)
{
    // ...
    msh_exec(RT_SHELL_PATH, strlen(RT_SHELL_PATH)+1);
    return 0;
}
```

## デフォルト動作

デフォルトの `init.sh` は顔検出デモアプリ (`fastboot_app.elf`) を起動します。

```sh
# k230_sdk/src/big/rt-smart/init.sh（デフォルト）
/bin/fastboot_app.elf /bin/test.kmodel
```

`fastboot_app.elf` はカメラ・映像出力・顔検出を開始し、ブロッキングで動作し続けます。
そのため msh シェルは入力を受け付けますが、コンソール出力がアプリケーションに占有されます。

### init.sh のコピー経路

`init.sh` は以下の経路でファームウェアイメージに組み込まれます。

```
k230_sdk/src/big/rt-smart/init.sh
  ↓ (make mpp でコピー)
k230_sdk/src/big/rt-smart/userapps/root/bin/init.sh
  ↓ (mkromfs.py で ROMFS に変換)
k230_sdk/src/big/rt-smart/kernel/bsp/maix3/applications/romfs.c
  ↓ (カーネルビルドで組み込み)
RT-Smart カーネルバイナリ
```

## msh シェル待ちイメージの作成

### 方法 1: ビルドスクリプトを使う

`scripts/build_sdk.sh` に `--no-fastboot` オプションを渡すと、`fastboot_app.elf` を起動しない msh シェル待ちイメージが生成されます。

```bash
scripts/build_sdk.sh --no-fastboot
```

### 方法 2: init.sh を手動で変更する

`init.sh` を直接編集して、`fastboot_app.elf` の起動行をコメントまたは空にします。

```sh
# k230_sdk/src/big/rt-smart/init.sh
# msh shell ready
```

変更後、SDK を再ビルドします。

## 部分ビルド

`init.sh` を変更した後にフルビルド (`make`) を行うこともできますが、以下の部分ビルドで時間を節約できます。
Docker コンテナ内の `k230_sdk/` ディレクトリで実行してください。

```bash
# 1. MPP ビルド — init.sh を userapps/root/bin/ にコピー
make mpp

# 2. RT-Smart ユーザアプリ + ROMFS 再生成 — mkromfs.py → romfs.c
make rt-smart-apps

# 3. RT-Smart カーネル再ビルド — romfs.c の更新を反映
make rt-smart-kernel

# 4. OpenSBI 再ビルド — RT-Smart バイナリをペイロードとして組み込み
make big-core-opensbi

# 5. 最終 SD カードイメージ生成
make build-image
```

フルビルド (`make`) はこれらすべてに加えて Linux (littlecore) 等のビルドも含むため、変更が `init.sh` のみであれば部分ビルドの方が高速です。

## 関連ファイル

| ファイル | 役割 |
|---------|------|
| `k230_sdk/src/big/rt-smart/init.sh` | 起動スクリプトのソース |
| `k230_sdk/src/big/rt-smart/kernel/bsp/maix3/applications/main.c` | RT-Smart main()。`msh_exec(RT_SHELL_PATH)` を呼ぶ |
| `k230_sdk/src/big/rt-smart/kernel/bsp/maix3/applications/romfs.c` | 自動生成。mkromfs.py で `userapps/root/` から生成 |
| `k230_sdk/src/big/rt-smart/kernel/bsp/maix3/rtconfig.h` | RT-Smart カーネル設定 (`RT_USING_MSH` 等) |
| `k230_sdk/src/big/rt-smart/kernel/rt-thread/components/finsh/shell.c` | msh シェルスレッド実装 |
| `k230_sdk/Makefile` | ビルドパイプライン (`mpp` → `rt-smart-apps` → `rt-smart-kernel`) |
