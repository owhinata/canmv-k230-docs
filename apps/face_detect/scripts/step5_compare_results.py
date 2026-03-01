"""Step 5: ONNX Runtime との結果比較
====================================
preprocess=True の kmodel 結果を ONNX Runtime と比較する。

ONNX Runtime 側は preprocess なし (float32 入力) なので、
手動で mean 減算を適用してから推論する:
  input_float = input_uint8.astype(float32) - mean
"""

import os
import numpy as np
import onnxruntime as rt
from sklearn.metrics.pairwise import cosine_similarity

SCRIPT_DIR = os.path.dirname(__file__)
PROJECT_ROOT = os.path.join(SCRIPT_DIR, "..", "..", "..")
OUTPUT_DIR = os.path.join(SCRIPT_DIR, "output")
DUMP_PATH = os.path.join(OUTPUT_DIR, "dump")
SIMPLIFIED_PATH = os.path.join(OUTPUT_DIR, "simplified.onnx")
MODEL_PATH = os.path.join(
    PROJECT_ROOT,
    "k230_sdk", "src", "big", "nncase", "examples", "models",
    "mobile_retinaface.onnx",
)

# kmodel の preprocess 設定と同じ mean/std
MEAN = np.array([104, 117, 123], dtype=np.float32).reshape(1, 3, 1, 1)
STD = np.array([1, 1, 1], dtype=np.float32).reshape(1, 3, 1, 1)


def get_cosine(vec1, vec2):
    """コサイン類似度を計算。"""
    return cosine_similarity(vec1.reshape(1, -1), vec2.reshape(1, -1))[0][0]


def main():
    print("=" * 60)
    print("Step 5: ONNX Runtime vs kmodel 結果比較")
    print("=" * 60)

    # ==========================================
    # 1. 同一入力データの読み込み (step4 で保存した uint8)
    # ==========================================
    print("\n[1/3] 入力データ読み込み")
    input_npy = os.path.join(DUMP_PATH, "input_data.npy")
    if not os.path.exists(input_npy):
        print(f"ERROR: {input_npy} が見つかりません。")
        print("先に step4_simulate_kmodel.py を実行してください。")
        return
    input_data = np.load(input_npy)
    print(f"  shape = {input_data.shape}, dtype = {input_data.dtype}")

    # ==========================================
    # 2. ONNX Runtime で推論 (mean 減算を手動適用)
    # ==========================================
    print("\n[2/3] ONNX Runtime で推論")
    onnx_path = SIMPLIFIED_PATH if os.path.exists(SIMPLIFIED_PATH) else MODEL_PATH
    print(f"  モデル: {os.path.basename(onnx_path)}")

    sess = rt.InferenceSession(onnx_path)
    input_name = sess.get_inputs()[0].name
    print(f"  入力名: {input_name}")

    # ONNX Runtime 用: uint8 → float32 に変換し mean 減算
    # kmodel の preprocess (input - mean) / std と同じ前処理を適用
    input_float = (input_data.astype(np.float32) - MEAN) / STD
    print(f"  前処理: (uint8 - mean) / std")
    print(f"  変換後 range = [{input_float.min():.1f}, {input_float.max():.1f}]")

    onnx_results = sess.run(None, {input_name: input_float})
    print(f"  出力数: {len(onnx_results)}")

    for idx, res in enumerate(onnx_results):
        print(f"  [{idx}] shape={res.shape}, "
              f"range=[{res.min():.6f}, {res.max():.6f}]")

    # ==========================================
    # 3. kmodel 結果との比較
    # ==========================================
    print("\n[3/3] コサイン類似度比較")
    print("-" * 50)

    output_names = [
        "cls_40x40", "cls_20x20", "cls_10x10",
        "bbox_40x40", "bbox_20x20", "bbox_10x10",
        "lmk_40x40", "lmk_20x20", "lmk_10x10",
    ]

    all_cosines = []
    for idx in range(len(onnx_results)):
        name = output_names[idx] if idx < len(output_names) else f"output_{idx}"
        npy_path = os.path.join(DUMP_PATH, f"kmodel_result_{idx}_{name}.npy")

        if not os.path.exists(npy_path):
            print(f"  [{idx}] {name}: kmodel 結果ファイルが見つかりません")
            continue

        kmodel_result = np.load(npy_path)
        onnx_result = onnx_results[idx]

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

        print(f"  [{idx}] {name:12s}  cosine = {cosine:.6f}  ({status})")

    if all_cosines:
        avg = np.mean(all_cosines)
        print("-" * 50)
        print(f"  平均コサイン類似度: {avg:.6f}")
        print()
        if avg >= 0.99:
            print("  -> 量子化は良好です。kmodel は元モデルとほぼ同等の精度です。")
        elif avg >= 0.95:
            print("  -> 量子化による精度劣化が見られます。")
            print("     --calib-dir で実画像キャリブレーションを試してください。")
        else:
            print("  -> 量子化による精度劣化が大きいです。")
            print("     実画像でのキャリブレーション、または")
            print("     calibrate_method, quant_type の調整を検討してください。")

    print("\nDone.")


if __name__ == "__main__":
    main()
