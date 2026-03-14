"""Step 4: 実機用 kmodel のシミュレーション実行
=============================================
preprocess=True, uint8 入力の kmodel をシミュレーション実行する。

使い方:
  python step4_simulate_kmodel.py                    # サンプルデータを使用
  python step4_simulate_kmodel.py --image photo.jpg  # 指定画像を使用
"""

import argparse
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

SCRIPT_DIR = os.path.dirname(__file__)
OUTPUT_DIR = os.path.join(SCRIPT_DIR, "..", "output")
DUMP_PATH = os.path.join(OUTPUT_DIR, "dump")
KMODEL_PATH = os.path.join(DUMP_PATH, "veg_classify.kmodel")
DATA_DIR = os.path.join(SCRIPT_DIR, "..", "data")

INPUT_H, INPUT_W = 224, 224


def find_default_image():
    """サンプルデータから最初の画像を返す。"""
    for cls_dir in sorted(os.listdir(DATA_DIR)):
        cls_path = os.path.join(DATA_DIR, cls_dir)
        if not os.path.isdir(cls_path):
            continue
        for f in sorted(os.listdir(cls_path)):
            if f.lower().endswith((".jpg", ".jpeg", ".png")):
                return os.path.join(cls_path, f)
    return None


def load_test_image_uint8(image_path, input_h, input_w):
    img = Image.open(image_path).convert("RGB")
    img = img.resize((input_w, input_h), Image.BILINEAR)
    arr = np.array(img, dtype=np.uint8)
    arr = arr.transpose(2, 0, 1)[np.newaxis, ...]
    return arr


def main():
    parser = argparse.ArgumentParser(description="kmodel シミュレーション")
    parser.add_argument("--image", type=str, default=None)
    args = parser.parse_args()

    image_path = args.image or find_default_image()

    print("=" * 60)
    print("Step 4: kmodel シミュレーション実行")
    print("=" * 60)

    if not os.path.exists(KMODEL_PATH):
        print(f"ERROR: {KMODEL_PATH} が見つかりません。")
        print("先に step3_compile_kmodel.py を実行してください。")
        return

    if not image_path or not os.path.exists(image_path):
        print(f"ERROR: テスト画像が見つかりません: {image_path}")
        return

    # Load kmodel
    print("\n[1/4] kmodel 読み込み中...")
    simulator = nncase.Simulator()
    with open(KMODEL_PATH, "rb") as f:
        simulator.load_model(f.read())
    print(f"  入力数: {simulator.inputs_size}")
    print(f"  出力数: {simulator.outputs_size}")

    # Prepare input
    input_dtype = simulator.get_input_desc(0).dtype
    print(f"\n[2/4] 入力データ準備 (kmodel 期待型: {input_dtype})")
    print(f"  画像: {image_path}")
    input_data = load_test_image_uint8(image_path, INPUT_H, INPUT_W)
    input_data = input_data.astype(input_dtype)
    print(f"  shape = {input_data.shape}, range = [{input_data.min()}, {input_data.max()}]")

    input_npy_path = os.path.join(DUMP_PATH, "input_data.npy")
    np.save(input_npy_path, input_data)

    # Simulate
    print("\n[3/4] シミュレーション実行中...")
    simulator.set_input_tensor(0, nncase.RuntimeTensor.from_numpy(input_data))
    simulator.run()
    print("  完了")

    # Get outputs
    print("\n[4/4] 出力テンソル取得")
    for idx in range(simulator.outputs_size):
        result = simulator.get_output_tensor(idx).to_numpy()
        npy_path = os.path.join(DUMP_PATH, f"kmodel_result_{idx}.npy")
        np.save(npy_path, result)
        print(f"  [{idx}] shape={result.shape}, "
              f"range=[{result.min():.6f}, {result.max():.6f}]")

        # Show top-3 predictions
        if result.ndim == 2 and result.shape[0] == 1:
            logits = result[0]
            exp_logits = np.exp(logits - logits.max())
            probs = exp_logits / exp_logits.sum()
            top3 = np.argsort(probs)[::-1][:3]

            labels_txt = os.path.join(OUTPUT_DIR, "labels.txt")
            labels = []
            if os.path.exists(labels_txt):
                with open(labels_txt) as f:
                    labels = [l.strip() for l in f if l.strip()]

            print("  Top-3 predictions:")
            for rank, i in enumerate(top3):
                label = labels[i] if i < len(labels) else f"class_{i}"
                print(f"    {rank+1}. {label}: {probs[i]:.4f}")

    print("\nDone.")


if __name__ == "__main__":
    main()
