# K230 SDK ビルド

K230 SDK をソースからビルドし、ツールチェーンとファームウェアイメージを生成する手順です。

## 前提条件

- Docker がインストール済みであること
- git がインストール済みであること
- x86_64 Linux ホスト

## 1. SDK クローン

```bash
git clone https://github.com/kendryte/k230_sdk
cd k230_sdk
```

## 2. ツールチェーン・ソースコードの取得

SDK に含まれるスクリプトを使用して、ツールチェーンと依存ソースコードをダウンロードします。

```bash
source tools/get_download_url.sh
make prepare_sourcecode
```

完了すると `toolchain/` ディレクトリにクロスコンパイラが展開されます。

## 3. Docker イメージのビルド

SDK 付属の Dockerfile からビルド用 Docker イメージを作成します。

```bash
docker build -f tools/docker/Dockerfile -t k230_sdk tools/docker
```

## 4. SDK ビルド（Docker 内）

Docker コンテナ内で SDK をビルドします。ホストのユーザ ID・グループ ID をそのまま使用して、ファイルの権限問題を避けます。

```bash
docker run -it --rm \
    --user $(id -u):$(id -g) \
    -v /etc/passwd:/etc/passwd:ro \
    -v /etc/group:/etc/group:ro \
    -v $(pwd):$(pwd) \
    -v $(pwd)/toolchain:/opt/toolchain \
    -w $(pwd) \
    k230_sdk \
    bash -c "make CONF=k230_canmv_defconfig"
```

!!! note "ビルド時間"
    初回ビルドには数十分〜数時間かかる場合があります。

## ビルド成果物

ビルドが完了すると `output/` ディレクトリにファームウェアイメージが生成されます。
また `toolchain/` にはクロスコンパイラが配置されており、[bigコア Hello World ビルド](hello_world.md)などのアプリケーション開発に使用します。

| パス | 内容 |
|------|------|
| `output/` | ファームウェアイメージ（SD カード書き込み用） |
| `toolchain/` | RISC-V クロスコンパイラ |

!!! tip "ビルドスクリプト"
    上記の手順をまとめたビルドスクリプトも利用できます:
    [owhinata/canmv-k230](https://github.com/owhinata/canmv-k230)
