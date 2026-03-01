"""Step 1: ONNXモデルの解析
=========================
mobile_retinaface.onnx の入出力情報を確認する。
"""

import os
import onnx
from onnx import TensorProto

SCRIPT_DIR = os.path.dirname(__file__)
PROJECT_ROOT = os.path.join(SCRIPT_DIR, "..", "..", "..")
MODEL_PATH = os.path.join(
    PROJECT_ROOT,
    "k230_sdk", "src", "big", "nncase", "examples", "models",
    "mobile_retinaface.onnx",
)


def main():
    print("=" * 60)
    print("Step 1: ONNXモデルの解析")
    print("=" * 60)

    model = onnx.load(MODEL_PATH)
    graph = model.graph

    print(f"\nModel: {os.path.basename(MODEL_PATH)}")
    print(f"IR Version: {model.ir_version}")
    print(f"Opset: {model.opset_import[0].version}")
    print(f"Producer: {model.producer_name} {model.producer_version}")
    print(f"Nodes: {len(graph.node)}")

    # --- 入力 ---
    initializer_names = {init.name for init in graph.initializer}
    real_inputs = [inp for inp in graph.input if inp.name not in initializer_names]

    print(f"\n--- 入力 ({len(real_inputs)}) ---")
    for inp in real_inputs:
        t = inp.type.tensor_type
        dtype = TensorProto.DataType.Name(t.elem_type)
        shape = [d.dim_value if d.dim_value else "?" for d in t.shape.dim]
        print(f"  name : {inp.name}")
        print(f"  dtype: {dtype}")
        print(f"  shape: {shape}")
        print()

    # --- 出力 ---
    print(f"--- 出力 ({len(graph.output)}) ---")
    for out in graph.output:
        t = out.type.tensor_type
        dtype = TensorProto.DataType.Name(t.elem_type)
        shape = [d.dim_value if d.dim_value else "?" for d in t.shape.dim]
        print(f"  name={out.name}  dtype={dtype}  shape={shape}")

    # --- 実機用ポイント ---
    print("\n--- 実機用コンパイルのポイント ---")
    print("1. 入力は1つ: float32 [1, 3, 320, 320] (NCHW)")
    print("2. 出力は9つ: 3スケール x (classification, bbox, landmark)")
    print("3. 実機用は preprocess=True, input_type=uint8")
    print("4. mean=[123, 117, 104], std=[1, 1, 1] (RGB順)")


if __name__ == "__main__":
    main()
