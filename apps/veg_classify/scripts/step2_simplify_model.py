"""Step 2: ONNXモデルの簡略化
=============================
onnxsim を使って ONNX モデルを最適化する。
"""

import os
import onnx
import onnxsim

SCRIPT_DIR = os.path.dirname(__file__)
OUTPUT_DIR = os.path.join(SCRIPT_DIR, "..", "output")
MODEL_PATH = os.path.join(OUTPUT_DIR, "best.onnx")
SIMPLIFIED_PATH = os.path.join(OUTPUT_DIR, "simplified.onnx")


def parse_model_input_output(model_file):
    onnx_model = onnx.load(model_file)
    input_all = [node.name for node in onnx_model.graph.input]
    input_initializer = [node.name for node in onnx_model.graph.initializer]
    input_names = list(set(input_all) - set(input_initializer))
    input_tensors = [
        node for node in onnx_model.graph.input if node.name in input_names
    ]
    inputs = []
    for e in input_tensors:
        onnx_type = e.type.tensor_type
        inputs.append({
            "name": e.name,
            "dtype": onnx.helper.tensor_dtype_to_np_dtype(onnx_type.elem_type),
            "shape": [i.dim_value for i in onnx_type.shape.dim],
        })
    return onnx_model, inputs


def main():
    print("=" * 60)
    print("Step 2: ONNXモデルの簡略化 (onnxsim)")
    print("=" * 60)

    if not os.path.exists(MODEL_PATH):
        print(f"ERROR: {MODEL_PATH} が見つかりません。")
        print("先に train.py を実行してください。")
        return

    os.makedirs(OUTPUT_DIR, exist_ok=True)

    onnx_model, inputs = parse_model_input_output(MODEL_PATH)
    print(f"\n元モデル: {MODEL_PATH}")
    print(f"元ノード数: {len(onnx_model.graph.node)}")

    onnx_model = onnx.shape_inference.infer_shapes(onnx_model)
    input_shapes = {inp["name"]: inp["shape"] for inp in inputs}
    onnx_model, check = onnxsim.simplify(onnx_model, overwrite_input_shapes=input_shapes)
    assert check, "簡略化後のモデル検証に失敗"

    print(f"簡略化後ノード数: {len(onnx_model.graph.node)}")

    onnx.save_model(onnx_model, SIMPLIFIED_PATH)
    orig_size = os.path.getsize(MODEL_PATH)
    simp_size = os.path.getsize(SIMPLIFIED_PATH)
    print(f"\n保存先: {SIMPLIFIED_PATH}")
    print(f"サイズ: {orig_size:,} bytes -> {simp_size:,} bytes")
    print("Done.")


if __name__ == "__main__":
    main()
