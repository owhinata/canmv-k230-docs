"""Step 4: 実機用 kmodel のシミュレーション実行
=============================================
preprocess=True, uint8 入力の kmodel をシミュレーション実行する。

チュートリアル版との違い:
  - テスト入力を uint8 で生成 (preprocess=True に合わせる)
  - 入力は [0, 255] の uint8 値をそのまま渡す
"""

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
PROJECT_ROOT = os.path.join(SCRIPT_DIR, "..", "..", "..")
OUTPUT_DIR = os.path.join(SCRIPT_DIR, "output")
DUMP_PATH = os.path.join(OUTPUT_DIR, "dump")
KMODEL_PATH = os.path.join(DUMP_PATH, "mobile_retinaface.kmodel")

INPUT_H, INPUT_W = 320, 320

# テスト用顔画像
TEST_IMAGE_PATH = os.path.join(
    PROJECT_ROOT,
    "k230_sdk", "src", "big", "nncase", "examples",
    "image_face_detect", "data", "face_500x500.jpg",
)


def load_test_image_uint8(image_path, input_h, input_w):
    """テスト画像を uint8 で読み込む (preprocess=True の kmodel 用)。

    前処理: RGB読込 → 320x320 リサイズ → uint8 のまま NCHW 変換
    """
    img = Image.open(image_path).convert("RGB")
    img = img.resize((input_w, input_h), Image.BILINEAR)
    arr = np.array(img, dtype=np.uint8)
    arr = arr.transpose(2, 0, 1)[np.newaxis, ...]  # (1, 3, H, W)
    return arr


def main():
    print("=" * 60)
    print("Step 4: 実機用 kmodel シミュレーション実行")
    print("=" * 60)

    if not os.path.exists(KMODEL_PATH):
        print(f"ERROR: {KMODEL_PATH} が見つかりません。")
        print("先に step3_compile_kmodel.py を実行してください。")
        return

    # ==========================================
    # 1. kmodel の読み込み
    # ==========================================
    print("\n[1/4] kmodel 読み込み中...")
    simulator = nncase.Simulator()
    with open(KMODEL_PATH, "rb") as f:
        simulator.load_model(f.read())
    print(f"  入力数: {simulator.inputs_size}")
    print(f"  出力数: {simulator.outputs_size}")

    # ==========================================
    # 2. 入力データの準備
    # ==========================================
    # kmodel が期待する入力 dtype を動的に取得 (公式スクリプトと同じ方法)
    input_dtype = simulator.get_input_desc(0).dtype
    print(f"\n[2/4] 入力データ準備 (kmodel 期待型: {input_dtype})")
    print(f"  画像: {os.path.basename(TEST_IMAGE_PATH)}")
    input_data = load_test_image_uint8(TEST_IMAGE_PATH, INPUT_H, INPUT_W)
    input_data = input_data.astype(input_dtype)
    print(f"  shape = {input_data.shape}")
    print(f"  dtype = {input_data.dtype}")
    print(f"  range = [{input_data.min()}, {input_data.max()}]")

    # 入力を保存 (step5 で同一入力を使うため)
    input_npy_path = os.path.join(DUMP_PATH, "input_data.npy")
    np.save(input_npy_path, input_data)
    print(f"  保存: {input_npy_path}")

    # ==========================================
    # 3. シミュレーション実行
    # ==========================================
    print("\n[3/4] シミュレーション実行中...")
    simulator.set_input_tensor(0, nncase.RuntimeTensor.from_numpy(input_data))
    simulator.run()
    print("  完了")

    # ==========================================
    # 4. 出力テンソルの取得と保存
    # ==========================================
    print("\n[4/4] 出力テンソル取得")
    output_names = [
        "cls_40x40", "cls_20x20", "cls_10x10",
        "bbox_40x40", "bbox_20x20", "bbox_10x10",
        "lmk_40x40", "lmk_20x20", "lmk_10x10",
    ]

    for idx in range(simulator.outputs_size):
        result = simulator.get_output_tensor(idx).to_numpy()
        name = output_names[idx] if idx < len(output_names) else f"output_{idx}"

        npy_path = os.path.join(DUMP_PATH, f"kmodel_result_{idx}_{name}.npy")
        np.save(npy_path, result)

        print(f"  [{idx}] {name}: shape={result.shape}, "
              f"range=[{result.min():.6f}, {result.max():.6f}]")

    print("\nDone. 出力は dump/ ディレクトリに保存されました。")


if __name__ == "__main__":
    main()
