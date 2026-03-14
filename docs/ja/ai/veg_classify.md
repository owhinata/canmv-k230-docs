# veg_classify — 野菜分類アプリケーション

`veg_classify` は K230 向けの野菜画像分類チュートリアルです。PC 上での ResNet-18 モデル学習から、kmodel 変換、K230 実機デプロイまでの **エンドツーエンドのワークフロー** を step-by-step で学習できます。

## 前提条件

- K230 SDK がビルド済みであること（ツールチェーン展開済み、MPP ライブラリコンパイル済み）
- SDK がリポジトリルートの `k230_sdk/` に配置されていること
- Python 3.8 以上（`requirements.txt` 参照）
- ホスト OS: x86_64 Linux
- CMake 3.16 以降

!!! note "SDK のビルド"
    K230 SDK のビルド手順については [SDK ビルド](../development/sdk_build.md) を参照してください。

## 全体ワークフロー

```
データセット (画像フォルダ)
  │
  ├─ split_data.py: train/val/test 分割
  │
  ├─ train.py: ResNet-18 学習 (CPU)
  │     ├─ best.pth  (PyTorch weights)
  │     ├─ best.onnx (ONNX export)
  │     └─ best.kmodel (nncase compile)
  │
  ├─ step1: ONNX モデル解析
  ├─ step2: ONNX 簡略化 (onnxsim)
  ├─ step3: kmodel コンパイル (PTQ量子化)
  ├─ step4: kmodel シミュレーション
  └─ step5: ONNX Runtime との精度比較
                  │
              kmodel ──→ K230 実機アプリ
                          │
                    ┌─────┴─────┐
                    │ カメラ入力  │
                    │ AI2D → KPU │
                    │ 分類結果表示 │
                    └───────────┘
```

---

## Part 1: モデル学習 (PC)

### 環境セットアップ

リポジトリルートの `.venv` を使用します:

```bash
source .venv/bin/activate
pip install -r requirements.txt
```

!!! tip "GPU なしで学習可能"
    `config.yaml` で `device: cpu` に設定されています。サンプルデータセット (5 カテゴリ × 50 枚) であれば CPU でも数分で学習が完了します。

### データセットの構成

サンプルデータセットは `apps/veg_classify/data/` に同梱されています（[kendryte/K230_training_scripts](https://github.com/kendryte/K230_training_scripts/tree/main/end2end_cls_doc/data/veg_cls) より）:

```
data/
├── broccoli/    # ブロッコリー (50枚)
├── carrot/      # にんじん (50枚)
├── eggplant/    # ナス (50枚)
├── spinach/     # ほうれん草 (50枚)
└── tomato/      # トマト (50枚)
```

カスタムデータセットを使う場合は、同じフォルダ構成（カテゴリ名ディレクトリ + 画像ファイル）で配置してください。

### 学習設定 (config.yaml)

| パラメータ | デフォルト値 | 説明 |
|-----------|------------|------|
| `dataset.root_folder` | `../data` | データセットルート |
| `dataset.split` | `true` | データ分割を実行するか |
| `dataset.train_ratio` | `0.7` | 学習用比率 |
| `dataset.val_ratio` | `0.15` | 検証用比率 |
| `dataset.test_ratio` | `0.15` | テスト用比率 |
| `train.device` | `cpu` | 学習デバイス |
| `train.image_size` | `[224, 224]` | 入力画像サイズ |
| `train.mean` | `[0.485, 0.456, 0.406]` | ImageNet 正規化 mean |
| `train.std` | `[0.229, 0.224, 0.225]` | ImageNet 正規化 std |
| `train.epochs` | `10` | エポック数 |
| `train.batch_size` | `8` | バッチサイズ |
| `train.learning_rate` | `0.001` | 学習率 |
| `deploy.target` | `k230` | ターゲットチップ |
| `deploy.ptq_option` | `0` | 量子化タイプ (0=uint8) |

### 学習の実行

#### 直接実行

```bash
cd apps/veg_classify/scripts
python train.py
```

#### CMake train ターゲット

CMake の `train` ターゲットを使うと、venv 作成・依存パッケージインストール・データ変更検知を自動で行います:

```bash
cmake -B build/veg_classify -S apps/veg_classify \
  -DCMAKE_TOOLCHAIN_FILE="$(pwd)/cmake/toolchain-k230-rtsmart.cmake"
cmake --build build/veg_classify --target train
```

`train` ターゲットの動作:

1. `.venv` が無ければ作成し、`requirements.txt` の依存パッケージをインストール
2. データセットのファイル構成をハッシュ化し、前回と比較
3. 変更がなければ学習をスキップ（`Dataset unchanged. Skipping training.`）
4. 変更があれば `train.py` を実行

外部データセットを使う場合は `DATA_DIR` オプションで指定できます:

```bash
cmake -B build/veg_classify -S apps/veg_classify \
  -DCMAKE_TOOLCHAIN_FILE="$(pwd)/cmake/toolchain-k230-rtsmart.cmake" \
  -DDATA_DIR=/path/to/custom/dataset
```

!!! tip "変更検知の仕組み"
    `check_data_hash.sh` がデータディレクトリ内の全ファイルのパスとサイズから MD5 ハッシュを計算します。ファイル内容は読まないため、大量データでも高速に動作します。ファイルの追加・削除・サイズ変更を検知できます。

`train.py` は以下を一貫して実行します:

1. データセット分割 (`split_data.py`)
2. ResNet-18 学習 (PyTorch)
3. ONNX エクスポート
4. kmodel 変換 (nncase がインストール済みの場合)

出力ファイル (`build/veg_classify/output/`):

| ファイル | 説明 |
|---------|------|
| `labels.txt` | カテゴリ名一覧 |
| `train.txt` / `val.txt` / `test.txt` | データ分割結果 |
| `samples.txt` | キャリブレーション用サンプルパス |
| `best.pth` | ベスト検証精度の重み |
| `last.pth` | 最終エポックの重み |
| `best.onnx` | ONNX モデル |
| `best.kmodel` | K230 向け kmodel |

---

## Part 2: kmodel 変換 (PC)

`train.py` で自動的に kmodel まで変換されますが、個別にステップを実行して精度を確認・改善することもできます。

### スクリプト一覧

| スクリプト | 説明 |
|-----------|------|
| `step1_parse_model.py` | ONNX モデルの入出力解析 |
| `step2_simplify_model.py` | onnxsim による ONNX 簡略化 |
| `step3_compile_kmodel.py` | kmodel コンパイル (PTQ量子化) |
| `step4_simulate_kmodel.py` | kmodel シミュレーション実行 |
| `step5_compare_results.py` | ONNX Runtime との精度比較 |

### Step 1: モデル解析

```bash
python apps/veg_classify/scripts/step1_parse_model.py
```

確認できる情報:

- 入力: float32 `[1, 3, 224, 224]` (NCHW)
- 出力: 1 テンソル `[1, num_classes]`

### Step 2: モデル簡略化

```bash
python apps/veg_classify/scripts/step2_simplify_model.py
```

出力: `apps/veg_classify/output/simplified.onnx`

### Step 3: kmodel コンパイル

#### コンパイル設定

| 項目 | 値 |
|------|-----|
| preprocess | True |
| input_type | uint8 |
| input_range | [0, 1] |
| mean | [0.485, 0.456, 0.406] (ImageNet) |
| std | [0.229, 0.224, 0.225] (ImageNet) |
| quant_type | uint8 |
| calibrate_method | Kld |

!!! tip "preprocess=True の意味"
    kmodel 内部で uint8→float32 変換と mean/std 正規化を行います。
    実機ではカメラの生データ (uint8) をそのまま入力可能です。

#### ランダムデータでコンパイル (初回)

```bash
python apps/veg_classify/scripts/step3_compile_kmodel.py
```

#### キャプチャ画像でコンパイル (精度改善)

```bash
python apps/veg_classify/scripts/step3_compile_kmodel.py --calib-dir /path/to/captures/
```

出力: `apps/veg_classify/output/dump/veg_classify.kmodel`

### Step 4: シミュレーション

```bash
python apps/veg_classify/scripts/step4_simulate_kmodel.py
python apps/veg_classify/scripts/step4_simulate_kmodel.py --image photo.jpg
```

### Step 5: 精度比較

```bash
python apps/veg_classify/scripts/step5_compare_results.py
```

精度の目安:

| コサイン類似度 | 評価 |
|--------------|------|
| 0.999 以上 | excellent — 非常に良好 |
| 0.99 以上 | good — 良好 |
| 0.95 以上 | acceptable — 許容範囲 |
| 0.95 未満 | poor — 要改善 |

### キャリブレーション改善サイクル

!!! note "精度を改善するには"
    1. ランダムデータで初回コンパイル (step3) → 実機で動作確認
    2. 実機アプリで 'c' キーを使い実環境の画像をキャプチャ
    3. キャプチャ画像で `step3 --calib-dir` → 再コンパイル
    4. `step5` で精度を確認
    5. 必要に応じて 2–4 を繰り返す

---

## Part 3: C++ アプリケーション (K230)

### ソースファイル

| ファイル | 説明 |
|---------|------|
| [`main.cc`][main] | メインアプリケーション — VICAP/VO 初期化、推論ループ、キャプチャ機能 |
| [`model.h`][model-h] / [`model.cc`][model-cc] | `Model` 抽象基底クラス — kmodel ロードと推論パイプライン |
| [`classifier.h`][cls-h] / [`classifier.cc`][cls-cc] | `Classifier` クラス — AI2D リサイズ前処理、softmax 後処理 |
| [`util.h`][util-h] / [`util.cc`][util-cc] | ユーティリティ (`ScopedTiming` 等) |
| [`vo_test_case.h`][vo-h] | VO レイヤーヘルパー型宣言 |

[main]: https://github.com/owhinata/canmv-k230/blob/be53063/apps/veg_classify/src/main.cc
[model-h]: https://github.com/owhinata/canmv-k230/blob/be53063/apps/veg_classify/src/model.h
[model-cc]: https://github.com/owhinata/canmv-k230/blob/be53063/apps/veg_classify/src/model.cc
[cls-h]: https://github.com/owhinata/canmv-k230/blob/be53063/apps/veg_classify/src/classifier.h
[cls-cc]: https://github.com/owhinata/canmv-k230/blob/be53063/apps/veg_classify/src/classifier.cc
[util-h]: https://github.com/owhinata/canmv-k230/blob/be53063/apps/veg_classify/src/util.h
[util-cc]: https://github.com/owhinata/canmv-k230/blob/be53063/apps/veg_classify/src/util.cc
[vo-h]: https://github.com/owhinata/canmv-k230/blob/be53063/apps/veg_classify/src/vo_test_case.h

### 推論パイプライン

```
センサー (OV5647)
  │
  ├─ CHN0 (1920x1080 YUV420) ──→ VO レイヤー ──→ HDMI ディスプレイ
  │
  └─ CHN1 (1280x720 RGB888P) ──→ AI 推論
                                    │
                              ┌─────┴─────┐
                              │ AI2D 前処理 │
                              │ (224x224   │
                              │ ストレッチ)  │
                              └─────┬─────┘
                                    │
                              ┌─────┴─────┐
                              │ KPU 推論    │
                              │ (ResNet-18) │
                              └─────┬─────┘
                                    │
                              ┌─────┴─────┐
                              │ 後処理      │
                              │ (softmax    │
                              │  + argmax)  │
                              └─────┬─────┘
                                    │
                          ┌─────────┼────────┐
                          │                  │
                    コンソール出力        キャプチャ
                    (分類結果)           ('c' キー)
                          │                  │
                          ↓                  ↓
                    Class: bocai       PNG 保存 (OpenCV)
                    (95.3%)
```

### ビルド手順

#### 1. 設定

```bash
cmake -B build/veg_classify -S apps/veg_classify \
  -DCMAKE_TOOLCHAIN_FILE="$(pwd)/cmake/toolchain-k230-rtsmart.cmake"
```

#### 2. ビルド

```bash
cmake --build build/veg_classify
```

#### 3. 確認

```bash
file build/veg_classify/veg_classify
```

期待される出力:

```
veg_classify: ELF 64-bit LSB executable, UCB RISC-V, RVC, double-float ABI, version 1 (SYSV), statically linked, ...
```

### コマンドライン引数

```
./veg_classify <kmodel> <labels.txt> [capture_dir]
```

| 引数 | 説明 |
|------|------|
| `<kmodel>` | 分類用 kmodel ファイルのパス |
| `<labels.txt>` | カテゴリラベルファイル (1行1ラベル) |
| `[capture_dir]` | キャプチャ画像の保存先ディレクトリ（省略可） |

### キー操作

| キー | 動作 |
|------|------|
| c + Enter | 現在のフレームを PNG 保存（`capture_dir` 指定時のみ） |
| q + Enter | アプリ終了 |

### K230 への転送・実行

#### SCP で転送

```bash
scp build/veg_classify/veg_classify root@<K230_IP_ADDRESS>:/sharefs/
scp apps/veg_classify/output/best.kmodel root@<K230_IP_ADDRESS>:/sharefs/veg_classify.kmodel
scp apps/veg_classify/output/labels.txt root@<K230_IP_ADDRESS>:/sharefs/
```

#### K230 bigcore (msh) で実行

```
msh /> /sharefs/veg_classify /sharefs/veg_classify.kmodel /sharefs/labels.txt
```

#### キャプチャモードで実行

```
msh /> mkdir /sharefs/calib
msh /> /sharefs/veg_classify /sharefs/veg_classify.kmodel /sharefs/labels.txt /sharefs/calib
```

!!! tip "キャリブレーション用キャプチャ"
    `capture_dir` を指定して実行し、実環境で 'c' + Enter を数回押して画像をキャプチャします。
    キャプチャした画像を PC に転送して `step3 --calib-dir` のキャリブレーションデータとして使用します。

    ```bash
    scp root@<K230_IP_ADDRESS>:/sharefs/calib/*.png ./calib/
    python apps/veg_classify/scripts/step3_compile_kmodel.py --calib-dir ./calib/
    ```

!!! tip "シリアル接続"
    - **Bigcore (RT-Smart msh)**: `/dev/ttyACM1`、115200 bps

    ```bash
    minicom -D /dev/ttyACM1 -b 115200
    ```
