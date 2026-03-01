"""kmodel 精度評価 (バッチ)
=========================
ディレクトリ内の画像を使って kmodel と ONNX Runtime の出力を比較する。
step3 の --calib-dir でキャプチャ画像を使ってコンパイルした後、
同じディレクトリを指定して精度を評価する想定。

使い方:
  python evaluate_kmodel.py <image_dir>
  python evaluate_kmodel.py /sharefs/calib/
"""

import argparse
import glob
import os
import subprocess
import sys
import numpy as np

# nncase.simulator.k230.sc を PATH に通す
result = subprocess.run([sys.executable, "-m", "pip", "show", "nncase"],
                        capture_output=True, text=True)
for line in result.stdout.splitlines():
    if line.startswith("Location:"):
        site_packages = line.split(": ", 1)[1]
        os.environ["PATH"] = site_packages + os.pathsep + os.environ.get("PATH", "")
        break

from PIL import Image
import nncase
import onnxruntime as rt
from sklearn.metrics.pairwise import cosine_similarity

SCRIPT_DIR = os.path.dirname(__file__)
PROJECT_ROOT = os.path.join(SCRIPT_DIR, "..", "..", "..")
OUTPUT_DIR = os.path.join(SCRIPT_DIR, "output")
DUMP_PATH = os.path.join(OUTPUT_DIR, "dump")
KMODEL_PATH = os.path.join(DUMP_PATH, "mobile_retinaface.kmodel")
SIMPLIFIED_PATH = os.path.join(OUTPUT_DIR, "simplified.onnx")
MODEL_PATH = os.path.join(
    PROJECT_ROOT,
    "k230_sdk", "src", "big", "nncase", "examples", "models",
    "mobile_retinaface.onnx",
)

INPUT_H, INPUT_W = 320, 320

# kmodel の preprocess 設定と同じ mean/std (RGB 順: R=123, G=117, B=104)
MEAN = np.array([123, 117, 104], dtype=np.float32).reshape(1, 3, 1, 1)
STD = np.array([1, 1, 1], dtype=np.float32).reshape(1, 3, 1, 1)

OUTPUT_NAMES = [
    "cls_40x40", "cls_20x20", "cls_10x10",
    "bbox_40x40", "bbox_20x20", "bbox_10x10",
    "lmk_40x40", "lmk_20x20", "lmk_10x10",
]


def get_cosine(vec1, vec2):
    return cosine_similarity(vec1.reshape(1, -1), vec2.reshape(1, -1))[0][0]


def load_image_uint8(image_path, input_h, input_w):
    img = Image.open(image_path).convert("RGB")
    img = img.resize((input_w, input_h), Image.BILINEAR)
    arr = np.array(img, dtype=np.uint8)
    arr = arr.transpose(2, 0, 1)[np.newaxis, ...]  # (1, 3, H, W)
    return arr


def find_images(image_dir):
    extensions = ("*.jpg", "*.jpeg", "*.png", "*.bmp")
    paths = []
    for ext in extensions:
        paths.extend(glob.glob(os.path.join(image_dir, ext)))
    paths.sort()
    return paths


def evaluate_image(image_path, simulator, input_dtype, onnx_sess, onnx_input_name):
    """1枚の画像に対して kmodel と ONNX の出力を比較し、各出力のコサイン類似度を返す。"""
    input_data = load_image_uint8(image_path, INPUT_H, INPUT_W)
    input_data = input_data.astype(input_dtype)

    # kmodel シミュレーション
    simulator.set_input_tensor(0, nncase.RuntimeTensor.from_numpy(input_data))
    simulator.run()
    kmodel_results = []
    for i in range(simulator.outputs_size):
        kmodel_results.append(simulator.get_output_tensor(i).to_numpy())

    # ONNX Runtime (mean 減算を手動適用)
    input_float = (input_data.astype(np.float32) - MEAN) / STD
    onnx_results = onnx_sess.run(None, {onnx_input_name: input_float})

    # コサイン類似度
    cosines = []
    for i in range(len(onnx_results)):
        cosines.append(get_cosine(onnx_results[i], kmodel_results[i]))
    return cosines


def main():
    parser = argparse.ArgumentParser(description="kmodel 精度バッチ評価")
    parser.add_argument("image_dir", type=str,
                        help="評価用画像ディレクトリ")
    args = parser.parse_args()

    print("=" * 60)
    print("kmodel 精度評価 (バッチ)")
    print("=" * 60)

    # 画像一覧
    image_paths = find_images(args.image_dir)
    if not image_paths:
        print(f"ERROR: 画像が見つかりません: {args.image_dir}")
        return
    print(f"\n画像ディレクトリ: {args.image_dir}")
    print(f"画像数: {len(image_paths)}")

    # kmodel 読み込み
    if not os.path.exists(KMODEL_PATH):
        print(f"ERROR: {KMODEL_PATH} が見つかりません。")
        print("先に step3_compile_kmodel.py を実行してください。")
        return
    print(f"\nkmodel: {KMODEL_PATH}")
    simulator = nncase.Simulator()
    with open(KMODEL_PATH, "rb") as f:
        simulator.load_model(f.read())
    input_dtype = simulator.get_input_desc(0).dtype

    # ONNX モデル読み込み
    onnx_path = SIMPLIFIED_PATH if os.path.exists(SIMPLIFIED_PATH) else MODEL_PATH
    print(f"ONNX:   {os.path.basename(onnx_path)}")
    onnx_sess = rt.InferenceSession(onnx_path)
    onnx_input_name = onnx_sess.get_inputs()[0].name

    # 各画像を評価
    print(f"\n{'画像':<30s}  {'平均cosine':>10s}")
    print("-" * 50)

    all_image_cosines = []
    for path in image_paths:
        cosines = evaluate_image(path, simulator, input_dtype, onnx_sess, onnx_input_name)
        avg = np.mean(cosines)
        all_image_cosines.append(cosines)
        print(f"  {os.path.basename(path):<28s}  {avg:.6f}")

    # 全体サマリー
    all_cosines = np.array(all_image_cosines)  # (num_images, num_outputs)
    print()
    print("=" * 50)
    print(f"{'出力':<14s}  {'平均':>8s}  {'最小':>8s}  {'最大':>8s}")
    print("-" * 50)
    for i, name in enumerate(OUTPUT_NAMES[:all_cosines.shape[1]]):
        col = all_cosines[:, i]
        print(f"  {name:<12s}  {col.mean():.6f}  {col.min():.6f}  {col.max():.6f}")

    overall = all_cosines.mean()
    print("-" * 50)
    print(f"  {'全体平均':<12s}  {overall:.6f}")
    print()
    if overall >= 0.99:
        print("  -> 量子化は良好です。")
    elif overall >= 0.95:
        print("  -> 量子化による精度劣化が見られます。")
    else:
        print("  -> 量子化による精度劣化が大きいです。")

    print("\nDone.")


if __name__ == "__main__":
    main()
