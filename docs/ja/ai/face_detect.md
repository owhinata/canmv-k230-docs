# face_detect — 顔検出アプリケーション

`face_detect` は K230 向けの顔検出アプリケーションです。Python スクリプトによる kmodel コンパイル・精度評価と、C++ 実機アプリケーションの 2 つのワークフローで構成されます。

## 前提条件

- K230 SDK がビルド済みであること（ツールチェーン展開済み、MPP ライブラリコンパイル済み）
- SDK がリポジトリルートの `k230_sdk/` に配置されていること
- Python 3.8 以上（`requirements.txt` 参照）
- ホスト OS: x86_64 Linux
- CMake 3.16 以降

!!! note "SDK のビルド"
    K230 SDK のビルド手順については [SDK ビルド](../development/sdk_build.md) を参照してください。

## 概要

`face_detect` は [`sample_face_ae`](../development/sample_face_ae.md) をベースに、以下の機能を追加したアプリケーションです:

- **Python スクリプト**: ONNX モデルから K230 向け kmodel へのコンパイルと精度評価
- **キャプチャ機能**: 実機で 'c' キーを押して現在のフレームを PNG 保存
- **入力スレッド**: 'c' キーでキャプチャ、'q' キーで終了
- **OpenCV リンク**: PNG エンコード・保存のため OpenCV を使用

### sample_face_ae との差分

| 機能 | sample_face_ae | face_detect |
|------|---------------|-------------|
| 顔検出 + AE ROI | ○ | ○ |
| キャプチャ機能 | × | ○ (OpenCV) |
| 入力スレッド | × | ○ ('c'/'q') |
| OpenCV リンク | × | ○ |
| Python スクリプト | × | ○ |

### 全体ワークフロー

```
ONNX モデル
  │
  ├─ step1: モデル解析 (入出力情報の確認)
  ├─ step2: モデル簡略化 (onnxsim)
  ├─ step3: kmodel コンパイル (PTQ量子化)
  ├─ step4: シミュレーション実行
  └─ step5: ONNX Runtime との精度比較
                    │
                kmodel ──→ 実機アプリ ──→ キャプチャ画像
                                              │
                                    step3 --calib-dir で再コンパイル
                                              │
                                    evaluate_kmodel.py で精度確認
```

---

## Part 1: kmodel コンパイル (PC)

### nncase とは

nncase は K230 の KPU (Knowledge Process Unit) 向けニューラルネットワークコンパイラです。ONNX モデルを PTQ (Post-Training Quantization) で量子化し、kmodel 形式に変換します。

参考資料:

- [CanMV K230 チュートリアル — Model Compilation](https://www.kendryte.com/k230/en/dev/CanMV_K230_Tutorial.html#model-compilation-and-simulator-inference)
- [nncase examples — K230 Simulate (GitHub)](https://github.com/kendryte/nncase/blob/master/examples/user_guide/k230_simulate-EN.ipynb)
- SDK 同梱の公式スクリプト: `k230_sdk/src/big/nncase/examples/scripts/`

### Python 環境セットアップ

リポジトリルートの `.venv` を使用します（MkDocs と共通）:

```bash
source .venv/bin/activate
pip install -r requirements.txt
```

### スクリプト一覧

| スクリプト | 説明 |
|-----------|------|
| `step1_parse_model.py` | ONNX モデルの入出力解析 |
| `step2_simplify_model.py` | onnxsim による ONNX 簡略化 |
| `step3_compile_kmodel.py` | kmodel コンパイル (PTQ量子化) |
| `step4_simulate_kmodel.py` | kmodel シミュレーション実行 |
| `step5_compare_results.py` | ONNX Runtime との精度比較 |
| `evaluate_kmodel.py` | バッチ精度評価 |

### Step 1: モデル解析

ONNX モデルの入出力情報を確認します。

```bash
python apps/face_detect/scripts/step1_parse_model.py
```

確認できる情報:

- 入力: float32 `[1, 3, 320, 320]` (NCHW)
- 出力: 9 テンソル — 3 スケール x (classification, bbox, landmark)

### Step 2: モデル簡略化

onnxsim を使って ONNX モデルを最適化します。冗長なノードが削除され、コンパイルの安定性が向上します。

```bash
python apps/face_detect/scripts/step2_simplify_model.py
```

出力: `apps/face_detect/scripts/output/simplified.onnx`

### Step 3: kmodel コンパイル

簡略化した ONNX を K230 向け kmodel にコンパイルします。

#### コンパイル設定

| 項目 | 値 |
|------|-----|
| preprocess | True |
| input_type | uint8 |
| input_range | [0, 255] |
| mean | [123, 117, 104] (RGB順) |
| std | [1, 1, 1] |
| quant_type | uint8 |
| calibrate_method | Kld |

!!! tip "preprocess=True の意味"
    kmodel 内部で uint8→float32 変換と mean/std 正規化を行います。
    実機ではカメラの生データ (uint8) をそのまま入力可能です。

#### ランダムデータでコンパイル (初回)

```bash
python apps/face_detect/scripts/step3_compile_kmodel.py
```

#### キャプチャ画像でコンパイル (精度改善)

```bash
python apps/face_detect/scripts/step3_compile_kmodel.py --calib-dir /path/to/captures/
```

出力: `apps/face_detect/scripts/output/dump/mobile_retinaface.kmodel`

### Step 4: シミュレーション

コンパイルした kmodel を PC 上でシミュレーション実行します。

```bash
# SDK 同梱のテスト画像を使用
python apps/face_detect/scripts/step4_simulate_kmodel.py

# 指定画像を使用
python apps/face_detect/scripts/step4_simulate_kmodel.py --image photo.png
```

出力: `apps/face_detect/scripts/output/dump/` に各出力テンソルの `.npy` ファイルが保存されます。

### Step 5: 精度比較

kmodel のシミュレーション結果を ONNX Runtime の推論結果とコサイン類似度で比較します。

```bash
python apps/face_detect/scripts/step5_compare_results.py
```

精度の目安:

| コサイン類似度 | 評価 |
|--------------|------|
| 0.999 以上 | excellent — 非常に良好 |
| 0.99 以上 | good — 良好 |
| 0.95 以上 | acceptable — 許容範囲 |
| 0.95 未満 | poor — 要改善 |

### バッチ精度評価

ディレクトリ内の全画像を対象に、kmodel と ONNX Runtime の出力をバッチ比較します。

```bash
python apps/face_detect/scripts/evaluate_kmodel.py /path/to/images/
```

出力ごとの min/mean/max コサイン類似度が表示されます。

### キャリブレーション改善サイクル

!!! note "精度を改善するには"
    1. ランダムデータで初回コンパイル (step3) → 実機で動作確認
    2. 実機アプリで 'c' キーを使い実環境の画像をキャプチャ
    3. キャプチャ画像で `step3 --calib-dir` → 再コンパイル
    4. `evaluate_kmodel.py` で精度を確認
    5. 必要に応じて 2–4 を繰り返す

---

## Part 2: 実機アプリケーション (C++)

### ソースファイル

| ファイル | 説明 |
|---------|------|
| [`main.cc`][main] | メインアプリケーション — VICAP/VO 初期化、推論ループ、キャプチャ機能 |
| [`model.h`][model-h] / [`model.cc`][model-cc] | `Model` 抽象基底クラス — kmodel ロードと推論パイプライン |
| [`mobile_retinaface.h`][mr-h] / [`mobile_retinaface.cc`][mr-cc] | `MobileRetinaface` クラス — 顔検出モデル（AI2D 前処理、アンカーデコード、NMS） |
| [`face_ae_roi.h`][far-h] / [`face_ae_roi.cc`][far-cc] | `FaceAeRoi` クラス — 顔座標を ISP AE ROI に反映 |
| [`util.h`][util-h] / [`util.cc`][util-cc] | ユーティリティ型（`box_t`、`face_coordinate`）とヘルパー |
| [`anchors_320.cc`][anchors] | 320x320 入力用の事前計算済みアンカーボックス |
| [`vo_test_case.h`][vo-h] | VO レイヤーヘルパー型（`layer_info`）の宣言 |

[main]: https://github.com/owhinata/canmv-k230/blob/2dd0691/apps/face_detect/src/main.cc
[model-h]: https://github.com/owhinata/canmv-k230/blob/2dd0691/apps/face_detect/src/model.h
[model-cc]: https://github.com/owhinata/canmv-k230/blob/2dd0691/apps/face_detect/src/model.cc
[mr-h]: https://github.com/owhinata/canmv-k230/blob/2dd0691/apps/face_detect/src/mobile_retinaface.h
[mr-cc]: https://github.com/owhinata/canmv-k230/blob/2dd0691/apps/face_detect/src/mobile_retinaface.cc
[far-h]: https://github.com/owhinata/canmv-k230/blob/2dd0691/apps/face_detect/src/face_ae_roi.h
[far-cc]: https://github.com/owhinata/canmv-k230/blob/2dd0691/apps/face_detect/src/face_ae_roi.cc
[util-h]: https://github.com/owhinata/canmv-k230/blob/2dd0691/apps/face_detect/src/util.h
[util-cc]: https://github.com/owhinata/canmv-k230/blob/2dd0691/apps/face_detect/src/util.cc
[anchors]: https://github.com/owhinata/canmv-k230/blob/2dd0691/apps/face_detect/src/anchors_320.cc
[vo-h]: https://github.com/owhinata/canmv-k230/blob/2dd0691/apps/face_detect/src/vo_test_case.h

### 処理フロー

```
センサー (OV5647)
  │
  ├─ CHN0 (1920x1080 YUV420) ──→ VO レイヤー ──→ HDMI ディスプレイ
  │                                    ↑
  │                              顔枠描画 (kd_mpi_vo_draw_frame)
  │
  └─ CHN1 (1280x720 RGB888P) ──→ AI 推論
                                    │
                              ┌─────┴─────┐
                              │ AI2D 前処理 │
                              │ (リサイズ+パッド) │
                              └─────┬─────┘
                                    │
                              ┌─────┴─────┐
                              │ KPU 推論    │
                              │ (MobileRetinaface) │
                              └─────┬─────┘
                                    │
                              ┌─────┴─────┐
                              │ 後処理      │
                              │ (デコード+NMS) │
                              └─────┬─────┘
                                    │
                          ┌─────────┼─────────┐
                          │         │         │
                    AE ROI 更新   顔枠描画   キャプチャ
                    (FaceAeRoi)             ('c' キー)
                          │                   │
                          ↓                   ↓
                    ISP AE エンジン      PNG 保存 (OpenCV)
```

### ビルド手順

#### 1. 設定

```bash
cmake -B build/face_detect -S apps/face_detect \
  -DCMAKE_TOOLCHAIN_FILE="$(pwd)/cmake/toolchain-k230-rtsmart.cmake"
```

#### 2. ビルド

```bash
cmake --build build/face_detect
```

#### 3. 確認

```bash
file build/face_detect/face_detect
```

期待される出力:

```
face_detect: ELF 64-bit LSB executable, UCB RISC-V, RVC, double-float ABI, version 1 (SYSV), statically linked, ...
```

### CMakeLists.txt の詳細

`apps/face_detect/CMakeLists.txt` は以下を処理します:

- **MPP インクルードパス**: `mpp/include/`、`mpp/include/comm/`、`mpp/include/ioctl/`、`mpp/userapps/api/` のヘッダ
- **NNCASE インクルードパス**: `nncase/include/`、`nncase/include/nncase/runtime/`、`rvvlib/include/` のヘッダ
- **OpenCV インクルードパス**: `opencv_thead/include/opencv4/` のヘッダ
- **MPP 静的ライブラリ**: 全 MPP ライブラリを `--start-group` / `--end-group` で循環依存を解決してリンク
- **NNCASE ライブラリ**: `Nncase.Runtime.Native`、`nncase.rt_modules.k230`、`functional_k230`、`rvv`
- **OpenCV ライブラリ**: `opencv_imgcodecs`、`opencv_imgproc`、`opencv_core` および 3rdparty ライブラリ
- **C++20**: `target_compile_features` で C++20 を要求

### コマンドライン引数

```
./face_detect <kmodel> <ae_roi> [capture_dir]
```

| 引数 | 説明 |
|------|------|
| `<kmodel>` | 顔検出用 kmodel ファイルのパス（例: `/sharefs/mobile_retinaface.kmodel`） |
| `<ae_roi>` | AE ROI の有効化: `1` = 有効、`0` = 無効 |
| `[capture_dir]` | キャプチャ画像の保存先ディレクトリ（省略可） |

### キー操作

| キー | 動作 |
|------|------|
| c + Enter | 現在のフレームを PNG 保存（`capture_dir` 指定時のみ） |
| q + Enter | アプリ終了 |

### K230 への転送・実行

CMake の `deploy` / `run` ターゲットで転送・実行をワンコマンドで行えます（詳細は [CMake ターゲット](#cmake-targets) を参照）:

```bash
cmake --build build/face_detect --target deploy   # ビルド + kmodel + SCP 転送
cmake --build build/face_detect --target run      # シリアル経由で実行 (Ctrl+C で終了)
```

#### 手動で転送・実行する場合

??? note "SCP + minicom による手動操作"
    ##### SCP で転送

    ```bash
    scp build/face_detect/face_detect root@<K230_IP_ADDRESS>:/sharefs/face_detect/
    scp apps/face_detect/scripts/output/dump/mobile_retinaface.kmodel root@<K230_IP_ADDRESS>:/sharefs/face_detect/
    ```

    ##### K230 bigcore (msh) で実行

    K230 のシリアルコンソール (ACM1) で実行します:

    ```
    msh /> /sharefs/face_detect/face_detect /sharefs/face_detect/mobile_retinaface.kmodel 1
    ```

    AE ROI を無効にして実行する場合:

    ```
    msh /> /sharefs/face_detect/face_detect /sharefs/face_detect/mobile_retinaface.kmodel 0
    ```

    ##### シリアル接続

    - **Bigcore (RT-Smart msh)**: `/dev/ttyACM1`、115200 bps

    ```bash
    minicom -D /dev/ttyACM1 -b 115200
    ```

#### キャプチャモードで実行

キャリブレーション用の画像をキャプチャするには、`capture_dir` を指定して実行します:

```
msh /> mkdir /sharefs/calib
msh /> /sharefs/face_detect/face_detect /sharefs/face_detect/mobile_retinaface.kmodel 1 /sharefs/calib
```

!!! tip "キャリブレーション用キャプチャ"
    `capture_dir` を指定して実行し、実環境で 'c' + Enter を数回押して画像をキャプチャします。
    キャプチャした画像を PC に転送して `step3 --calib-dir` のキャリブレーションデータとして使用します。

    ```bash
    # K230 から PC へキャプチャ画像を転送
    scp root@<K230_IP_ADDRESS>:/sharefs/calib/*.png ./calib/

    # キャプチャ画像でキャリブレーション再コンパイル
    python apps/face_detect/scripts/step3_compile_kmodel.py --calib-dir ./calib/
    ```

---

## CMake ターゲット { #cmake-targets }

### 設定

```bash
cmake -B build/face_detect -S apps/face_detect \
  -DCMAKE_TOOLCHAIN_FILE="$(pwd)/cmake/toolchain-k230-rtsmart.cmake"
```

### ターゲット一覧

| ターゲット | コマンド | 説明 |
|-----------|---------|------|
| (デフォルト) | `cmake --build build/face_detect` | C++ バイナリのビルド |
| `kmodel` | `cmake --build build/face_detect --target kmodel` | kmodel コンパイル (step2 + step3) |
| `deploy` | `cmake --build build/face_detect --target deploy` | ビルド + kmodel + K230 への SCP 転送 |
| `run` | `cmake --build build/face_detect --target run` | シリアル経由で K230 実行 (Ctrl+C で終了) |

### kmodel

ONNX モデルの簡略化と kmodel コンパイルを自動で行います:

```bash
cmake --build build/face_detect --target kmodel
```

### deploy

バイナリのビルド、kmodel コンパイル、K230 への SCP 転送を一括実行します:

```bash
cmake --build build/face_detect --target deploy
```

転送されるファイル:

| ローカル | K230 上のパス |
|---------|-------------|
| `build/face_detect/face_detect` | `/sharefs/face_detect/face_detect` |
| `apps/face_detect/scripts/output/dump/mobile_retinaface.kmodel` | `/sharefs/face_detect/mobile_retinaface.kmodel` |

### run

シリアルポート経由で K230 bigcore (msh) にコマンドを送信し、出力をリアルタイム表示します:

```bash
cmake --build build/face_detect --target run
```

- キーボード入力はそのまま K230 に転送されます（`q` + Enter でアプリ終了）
- **Ctrl+C** でシリアル接続を切断

### K230 接続設定

CMake キャッシュ変数で接続先をカスタマイズできます:

| 変数 | デフォルト | 説明 |
|------|-----------|------|
| `K230_IP` | (空 = 自動検出) | littlecore の IP アドレス |
| `K230_USER` | `root` | SSH ユーザー |
| `K230_SERIAL` | `/dev/ttyACM1` | bigcore シリアルポート (run 用) |
| `K230_SERIAL_LC` | `/dev/ttyACM0` | littlecore シリアル (IP 自動検出用) |
| `K230_BAUD` | `115200` | ボーレート |
