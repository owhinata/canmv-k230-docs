"""Step 5: ONNX Runtime との結果比較
====================================
preprocess=True の kmodel 結果を ONNX Runtime と比較する。

ONNX Runtime 側は preprocess なし (float32 入力) なので、
手動で ToTensor + Normalize を適用してから推論する。
"""

import os
import numpy as np
import onnxruntime as rt
from sklearn.metrics.pairwise import cosine_similarity

SCRIPT_DIR = os.path.dirname(__file__)
OUTPUT_DIR = os.path.join(SCRIPT_DIR, "..", "output")
DUMP_PATH = os.path.join(OUTPUT_DIR, "dump")
SIMPLIFIED_PATH = os.path.join(OUTPUT_DIR, "simplified.onnx")
ONNX_PATH = os.path.join(OUTPUT_DIR, "best.onnx")

MEAN = np.array([0.485, 0.456, 0.406], dtype=np.float32).reshape(1, 3, 1, 1)
STD = np.array([0.229, 0.224, 0.225], dtype=np.float32).reshape(1, 3, 1, 1)


def get_cosine(vec1, vec2):
    return cosine_similarity(vec1.reshape(1, -1), vec2.reshape(1, -1))[0][0]


def main():
    print("=" * 60)
    print("Step 5: ONNX Runtime vs kmodel 結果比較")
    print("=" * 60)

    input_npy = os.path.join(DUMP_PATH, "input_data.npy")
    if not os.path.exists(input_npy):
        print(f"ERROR: {input_npy} が見つかりません。")
        print("先に step4_simulate_kmodel.py を実行してください。")
        return

    # Load input
    print("\n[1/3] 入力データ読み込み")
    input_data = np.load(input_npy)
    print(f"  shape = {input_data.shape}, dtype = {input_data.dtype}")

    # ONNX Runtime inference
    print("\n[2/3] ONNX Runtime で推論")
    onnx_path = SIMPLIFIED_PATH if os.path.exists(SIMPLIFIED_PATH) else ONNX_PATH
    print(f"  モデル: {os.path.basename(onnx_path)}")

    sess = rt.InferenceSession(onnx_path)
    input_name = sess.get_inputs()[0].name

    # Apply same preprocessing as kmodel: uint8 -> [0,1] -> normalize
    input_float = (input_data.astype(np.float32) / 255.0 - MEAN) / STD
    print(f"  前処理: (uint8/255 - mean) / std")
    print(f"  変換後 range = [{input_float.min():.3f}, {input_float.max():.3f}]")

    onnx_results = sess.run(None, {input_name: input_float})
    print(f"  出力数: {len(onnx_results)}")

    # Compare
    print("\n[3/3] コサイン類似度比較")
    print("-" * 50)

    all_cosines = []
    for idx, onnx_result in enumerate(onnx_results):
        npy_path = os.path.join(DUMP_PATH, f"kmodel_result_{idx}.npy")
        if not os.path.exists(npy_path):
            print(f"  [{idx}] kmodel 結果ファイルが見つかりません")
            continue

        kmodel_result = np.load(npy_path)
        cosine = get_cosine(onnx_result, kmodel_result)
        all_cosines.append(cosine)

        if cosine >= 0.999:
            status = "excellent"
        elif cosine >= 0.99:
            status = "good"
        elif cosine >= 0.95:
            status = "acceptable"
        else:
            status = "poor"

        print(f"  [{idx}] cosine = {cosine:.6f}  ({status})")

    if all_cosines:
        avg = np.mean(all_cosines)
        print("-" * 50)
        print(f"  平均コサイン類似度: {avg:.6f}")
        if avg >= 0.99:
            print("  -> 量子化は良好です。")
        elif avg >= 0.95:
            print("  -> 精度劣化あり。--calib-dir で実画像キャリブレーションを推奨。")
        else:
            print("  -> 精度劣化大。キャリブレーション or quant_type 調整を検討。")

    print("\nDone.")


if __name__ == "__main__":
    main()
