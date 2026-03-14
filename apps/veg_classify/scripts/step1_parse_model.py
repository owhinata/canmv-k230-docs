"""Step 1: ONNXモデルの解析
=========================
best.onnx の入出力情報を確認する。
"""

import os
import onnx
from onnx import TensorProto

SCRIPT_DIR = os.path.dirname(__file__)
OUTPUT_DIR = os.path.join(SCRIPT_DIR, "..", "output")
MODEL_PATH = os.path.join(OUTPUT_DIR, "best.onnx")


def main():
    print("=" * 60)
    print("Step 1: ONNXモデルの解析")
    print("=" * 60)

    if not os.path.exists(MODEL_PATH):
        print(f"ERROR: {MODEL_PATH} が見つかりません。")
        print("先に train.py を実行してください。")
        return

    model = onnx.load(MODEL_PATH)
    graph = model.graph

    print(f"\nModel: {os.path.basename(MODEL_PATH)}")
    print(f"IR Version: {model.ir_version}")
    print(f"Opset: {model.opset_import[0].version}")
    print(f"Nodes: {len(graph.node)}")

    # --- inputs ---
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

    # --- outputs ---
    print(f"\n--- 出力 ({len(graph.output)}) ---")
    for out in graph.output:
        t = out.type.tensor_type
        dtype = TensorProto.DataType.Name(t.elem_type)
        shape = [d.dim_value if d.dim_value else "?" for d in t.shape.dim]
        print(f"  name={out.name}  dtype={dtype}  shape={shape}")

    print("\n--- 分類モデルのポイント ---")
    print("1. 入力: float32 [1, 3, 224, 224] (NCHW)")
    print("2. 出力: 1テンソル [1, num_classes]")
    print("3. 実機用は preprocess=True, input_type=uint8")
    print("4. mean=[0.485, 0.456, 0.406], std=[0.229, 0.224, 0.225] (ImageNet)")


if __name__ == "__main__":
    main()
