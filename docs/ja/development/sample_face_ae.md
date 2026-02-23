# sample_face_ae

このガイドでは、K230 の `sample_face_ae` アプリケーションを CMake out-of-tree ビルドで構築する方法を説明します。このサンプルは顔検出モデル (MobileRetinaface) を使用して検出した顔領域を ISP の AE (Auto Exposure) ROI に反映し、顔に最適化された露出制御を実現するデモアプリケーションです。

## 前提条件

- K230 SDK がビルド済みであること（ツールチェーン展開済み、MPP ライブラリコンパイル済み）
- SDK がリポジトリルートの `k230_sdk/` に配置されていること
- 顔検出用の kmodel ファイル（`mobile_retinaface.kmodel`）
- ホスト OS: x86_64 Linux
- CMake 3.16 以降

!!! note "SDK のビルド"
    K230 SDK のビルド手順については [SDK ビルド](sdk_build.md) を参照してください。

## 概要

`sample_face_ae` は K230 SDK の顔検出サンプル (`sample_face_ae`) をベースにした、AI 推論と ISP AE ROI 制御を組み合わせたアプリケーションです。以下を実演します:

- VICAP API によるカメラセンサーの設定（2チャネル: 表示用 YUV420 + AI推論用 RGB888P）
- NNCASE ランタイムによる kmodel の読み込みと AI2D 前処理
- MobileRetinaface モデルによる顔検出（バウンディングボックス + ランドマーク）
- 検出した顔領域の ISP AE ROI への反映（面積比重み付け）
- VO ディスプレイへのリアルタイムプレビューと顔枠描画

### ソースファイル

| ファイル | 説明 |
|---------|------|
| [`main.cc`][main] | メインアプリケーション — VB/VICAP/VO 初期化、AI 推論ループ、クリーンアップ |
| [`model.h`][model-h] / [`model.cc`][model-cc] | `Model` 抽象基底クラス — kmodel ロードと推論パイプライン |
| [`mobile_retinaface.h`][mr-h] / [`mobile_retinaface.cc`][mr-cc] | `MobileRetinaface` クラス — 顔検出モデル（AI2D 前処理、アンカーデコード、NMS） |
| [`face_ae_roi.h`][far-h] / [`face_ae_roi.cc`][far-cc] | `FaceAeRoi` クラス — 顔座標を ISP AE ROI に反映 |
| [`util.h`][util-h] / [`util.cc`][util-cc] | ユーティリティ型（`box_t`、`face_coordinate`）とヘルパー |
| [`anchors_320.cc`][anchors] | 320x320 入力用の事前計算済みアンカーボックス |
| [`vo_test_case.h`][vo-h] | VO レイヤーヘルパー型（`layer_info`）の宣言 |

[main]: https://github.com/owhinata/canmv-k230/blob/4f9b08c/apps/sample_face_ae/src/main.cc
[model-h]: https://github.com/owhinata/canmv-k230/blob/4f9b08c/apps/sample_face_ae/src/model.h
[model-cc]: https://github.com/owhinata/canmv-k230/blob/4f9b08c/apps/sample_face_ae/src/model.cc
[mr-h]: https://github.com/owhinata/canmv-k230/blob/4f9b08c/apps/sample_face_ae/src/mobile_retinaface.h
[mr-cc]: https://github.com/owhinata/canmv-k230/blob/4f9b08c/apps/sample_face_ae/src/mobile_retinaface.cc
[far-h]: https://github.com/owhinata/canmv-k230/blob/4f9b08c/apps/sample_face_ae/src/face_ae_roi.h
[far-cc]: https://github.com/owhinata/canmv-k230/blob/4f9b08c/apps/sample_face_ae/src/face_ae_roi.cc
[util-h]: https://github.com/owhinata/canmv-k230/blob/4f9b08c/apps/sample_face_ae/src/util.h
[util-cc]: https://github.com/owhinata/canmv-k230/blob/4f9b08c/apps/sample_face_ae/src/util.cc
[anchors]: https://github.com/owhinata/canmv-k230/blob/4f9b08c/apps/sample_face_ae/src/anchors_320.cc
[vo-h]: https://github.com/owhinata/canmv-k230/blob/4f9b08c/apps/sample_face_ae/src/vo_test_case.h

## 処理フロー

アプリケーションは以下のデータフローに沿って動作します:

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
                              ┌─────┴─────┐
                              │ AE ROI 更新 │
                              │ (FaceAeRoi) │
                              └─────┬─────┘
                                    │
                                    ↓
                              ISP AE エンジン
```

## クラスリファレンス

### Model — 抽象基底クラス

**Source:** [`model.h` L13–L39][model-class] / [`model.cc`][model-cc]

kmodel のロードと推論パイプラインを管理する抽象基底クラス。サブクラスは `Preprocess()` と `Postprocess()` を実装します。

| メソッド | 説明 |
|---------|------|
| `Model(model_name, kmodel_file)` | kmodel ファイルを読み込み、入力テンソルを作成 |
| `Run(vaddr, paddr)` | `Preprocess` → `KpuRun` → `Postprocess` の推論パイプラインを実行 |
| `Preprocess(vaddr, paddr)` | 純粋仮想 — 入力データの前処理 |
| `KpuRun()` | kmodel を KPU で実行 |
| `Postprocess()` | 純粋仮想 — 推論結果の後処理 |
| `InputTensor(idx)` / `OutputTensor(idx)` | 入出力テンソルへのアクセス |
| `InputShape(idx)` / `OutputShape(idx)` | 入出力形状の取得 |

**メンバ変数:**

| 変数 | 説明 |
|------|------|
| `ai2d_builder_` | AI2D 前処理パイプラインビルダー |
| `ai2d_in_tensor_` / `ai2d_out_tensor_` | AI2D 入出力テンソル |
| `interp_` | NNCASE ランタイムインタプリタ |

[model-class]: https://github.com/owhinata/canmv-k230/blob/4f9b08c/apps/sample_face_ae/src/model.h#L13-L39

### MobileRetinaface — 顔検出モデル

**Source:** [`mobile_retinaface.h` L13–L48][mr-class] / [`mobile_retinaface.cc`][mr-cc]

`Model` を継承し、MobileRetinaface 顔検出モデルの前処理・後処理を実装します。

| メソッド | 説明 |
|---------|------|
| `MobileRetinaface(kmodel_file, channel, height, width)` | AI2D 前処理パイプラインを構築（リサイズ + パディング） |
| `GetResult()` | 検出結果（バウンディングボックス + ランドマーク）を返す |
| `Preprocess(vaddr, paddr)` | VICAP フレームから AI2D テンソルを作成し、前処理を実行 |
| `Postprocess()` | 9 個の出力テンソルをデコードし、NMS でフィルタリング |

**後処理の流れ:**

1. **Confidence デコード** (`DealConfOpt`) — 3 スケールの信頼度を softmax で処理し、閾値 (`obj_threshold_` = 0.6) 以上のオブジェクトを選択
2. **Location デコード** (`DealLocOpt`) — 選択されたオブジェクトの位置情報をアンカーボックスを使ってデコード
3. **Landmark デコード** (`DealLandmsOpt`) — 5 点の顔ランドマークをデコード
4. **NMS** — IoU 閾値 (`nms_threshold_` = 0.5) で重複ボックスを除去
5. **座標変換** — モデル座標からカメラ入力座標に変換

[mr-class]: https://github.com/owhinata/canmv-k230/blob/4f9b08c/apps/sample_face_ae/src/mobile_retinaface.h#L13-L48

### FaceAeRoi — AE ROI 制御

**Source:** [`face_ae_roi.h` L9–L21][far-class] / [`face_ae_roi.cc`][far-cc]

検出された顔座標を ISP AE ROI ウィンドウに変換し、顔領域に最適化された自動露出制御を実現します。

| メソッド | 説明 |
|---------|------|
| `FaceAeRoi(dev, model_w, model_h, sensor_w, sensor_h)` | ISP デバイスとモデル/センサー解像度を設定 |
| `SetEnable(enable)` | ISP AE ROI 機能の有効/無効を切り替え (`kd_mpi_isp_ae_roi_set_enable`) |
| `Update(boxes)` | 顔バウンディングボックスを AE ROI ウィンドウに変換して設定 |

**`Update()` の処理:**

1. 顔座標をモデル解像度からセンサー解像度にスケーリング
2. 最大 8 個の ROI ウィンドウを設定
3. 各 ROI の重みを面積比で算出（大きい顔 = 大きい重み）
4. `kd_mpi_isp_ae_set_roi()` で ISP に反映

[far-class]: https://github.com/owhinata/canmv-k230/blob/4f9b08c/apps/sample_face_ae/src/face_ae_roi.h#L9-L21

## ビルド手順

### 1. 設定

```bash
cmake -B build/sample_face_ae -S apps/sample_face_ae \
  -DCMAKE_TOOLCHAIN_FILE="$(pwd)/cmake/toolchain-k230-rtsmart.cmake"
```

### 2. ビルド

```bash
cmake --build build/sample_face_ae
```

### 3. 確認

```bash
file build/sample_face_ae/sample_face_ae
```

期待される出力:

```
sample_face_ae: ELF 64-bit LSB executable, UCB RISC-V, RVC, double-float ABI, version 1 (SYSV), statically linked, ...
```

## CMakeLists.txt の詳細

`apps/sample_face_ae/CMakeLists.txt` は以下を処理します:

- **MPP インクルードパス**: `mpp/include/`、`mpp/include/comm/`、`mpp/include/ioctl/`、`mpp/userapps/api/` のヘッダ
- **NNCASE インクルードパス**: `nncase/include/`、`nncase/include/nncase/runtime/`、`rvvlib/include/` のヘッダ
- **MPP 静的ライブラリ**: 全 MPP ライブラリを `--start-group` / `--end-group` で循環依存を解決してリンク
- **NNCASE ライブラリ**: `Nncase.Runtime.Native`、`nncase.rt_modules.k230`、`functional_k230`、`rvv`
- **C++20**: `target_compile_features` で C++20 を要求

## コマンドライン引数

```
./sample_face_ae <kmodel> <roi_enable>
```

| 引数 | 説明 |
|------|------|
| `<kmodel>` | 顔検出用 kmodel ファイルのパス（例: `/sharefs/mobile_retinaface.kmodel`） |
| `<roi_enable>` | AE ROI の有効化: `1` = 有効、`0` = 無効 |

## K230 への転送・実行

### SCP で転送

```bash
scp build/sample_face_ae/sample_face_ae root@<K230_IP_ADDRESS>:/sharefs/sample_face_ae
scp mobile_retinaface.kmodel root@<K230_IP_ADDRESS>:/sharefs/
```

### K230 bigcore (msh) で実行

K230 のシリアルコンソール (ACM1) で実行します:

```
msh /> /sharefs/sample_face_ae /sharefs/mobile_retinaface.kmodel 1
```

AE ROI を無効にして実行する場合:

```
msh /> /sharefs/sample_face_ae /sharefs/mobile_retinaface.kmodel 0
```

!!! tip "シリアル接続"
    - **Bigcore (RT-Smart msh)**: `/dev/ttyACM1`、115200 bps

    ```bash
    minicom -D /dev/ttyACM1 -b 115200
    ```
