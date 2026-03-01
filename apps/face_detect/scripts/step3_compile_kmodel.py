"""Step 3: 実機用 kmodel へのコンパイル (PTQ量子化)
=================================================
simplified.onnx を nncase で K230 向け kmodel にコンパイルする。

チュートリアル版との違い:
  - preprocess=True: kmodel が uint8->float32 変換と mean/std 正規化を行う
  - input_type=uint8: カメラからの生データをそのまま入力可能
  - mean=[104, 117, 123], std=[1, 1, 1]: BGR mean subtraction

キャリブレーション:
  - デフォルト: ランダムデータ (初回コンパイル用)
  - --calib-dir: キャプチャした実画像を使用 (精度改善用)
"""

import argparse
import glob
import os
import numpy as np
from PIL import Image
import nncase

SCRIPT_DIR = os.path.dirname(__file__)
PROJECT_ROOT = os.path.join(SCRIPT_DIR, "..", "..", "..")
OUTPUT_DIR = os.path.join(SCRIPT_DIR, "output")
SIMPLIFIED_PATH = os.path.join(OUTPUT_DIR, "simplified.onnx")
DUMP_PATH = os.path.join(OUTPUT_DIR, "dump")
KMODEL_PATH = os.path.join(DUMP_PATH, "mobile_retinaface.kmodel")

# mobile_retinaface の入力仕様
INPUT_H, INPUT_W = 320, 320


def load_calib_images_uint8(image_dir, input_h, input_w):
    """キャプチャ画像を uint8 で読み込みキャリブレーションデータを作成する。

    preprocess=True の kmodel 用: uint8 NCHW [0, 255]
    """
    extensions = ("*.jpg", "*.jpeg", "*.png", "*.bmp")
    image_paths = []
    for ext in extensions:
        image_paths.extend(glob.glob(os.path.join(image_dir, ext)))
    image_paths.sort()

    if not image_paths:
        return []

    samples = []
    for path in image_paths:
        img = Image.open(path).convert("RGB")
        img = img.resize((input_w, input_h), Image.BILINEAR)
        # HWC uint8 -> NCHW uint8
        arr = np.array(img, dtype=np.uint8)
        arr = arr.transpose(2, 0, 1)[np.newaxis, ...]  # (1, 3, H, W)
        samples.append(arr.astype(np.float32))
        print(f"    {os.path.basename(path):25s} {img.size} -> {arr.shape}")

    return samples


def main():
    parser = argparse.ArgumentParser(description="実機用 kmodel コンパイル")
    parser.add_argument("--calib-dir", type=str, default=None,
                        help="キャリブレーション用画像ディレクトリ (キャプチャ PNG)")
    args = parser.parse_args()

    print("=" * 60)
    print("Step 3: 実機用 kmodel コンパイル (PTQ量子化)")
    print("=" * 60)

    os.makedirs(DUMP_PATH, exist_ok=True)

    if not os.path.exists(SIMPLIFIED_PATH):
        print(f"ERROR: {SIMPLIFIED_PATH} が見つかりません。")
        print("先に step2_simplify_model.py を実行してください。")
        return

    # ==========================================
    # 1. CompileOptions の設定 (実機用)
    # ==========================================
    print("\n[1/5] CompileOptions 設定 (実機用)")
    compile_options = nncase.CompileOptions()
    compile_options.target = "k230"
    compile_options.dump_ir = True
    compile_options.dump_asm = True
    compile_options.dump_dir = DUMP_PATH
    compile_options.input_file = ""

    # 実機用: preprocess=True で kmodel 内部で前処理を行う
    compile_options.preprocess = True
    compile_options.input_type = "uint8"
    compile_options.input_range = [0, 255]
    compile_options.mean = [104, 117, 123]
    compile_options.std = [1, 1, 1]
    compile_options.input_shape = [1, 3, INPUT_H, INPUT_W]
    compile_options.input_layout = "NCHW"
    compile_options.output_layout = "NCHW"

    print(f"  target      = {compile_options.target}")
    print(f"  preprocess  = {compile_options.preprocess}")
    print(f"  input_type  = {compile_options.input_type}")
    print(f"  input_range = {compile_options.input_range}")
    print(f"  mean        = {compile_options.mean}")
    print(f"  std         = {compile_options.std}")

    # ==========================================
    # 2. PTQTensorOptions の設定
    # ==========================================
    print("\n[2/5] PTQTensorOptions 設定")
    ptq_options = nncase.PTQTensorOptions()
    ptq_options.quant_type = "uint8"
    ptq_options.w_quant_type = "uint8"
    ptq_options.calibrate_method = "Kld"
    ptq_options.finetune_weights_method = "NoFineTuneWeights"
    ptq_options.dump_quant_error = False
    ptq_options.dump_quant_error_symmetric_for_signed = False

    ptq_options.quant_scheme = ""
    ptq_options.quant_scheme_strict_mode = False
    ptq_options.export_quant_scheme = False
    ptq_options.export_weight_range_by_channel = False

    print(f"  quant_type       = {ptq_options.quant_type}")
    print(f"  w_quant_type     = {ptq_options.w_quant_type}")
    print(f"  calibrate_method = {ptq_options.calibrate_method}")

    # ==========================================
    # 3. キャリブレーションデータの生成
    # ==========================================
    if args.calib_dir:
        print(f"\n[3/5] キャリブレーションデータ生成 (実画像)")
        print(f"  画像ディレクトリ: {args.calib_dir}")
        samples = load_calib_images_uint8(args.calib_dir, INPUT_H, INPUT_W)
        if not samples:
            print("  WARNING: 画像が見つかりません。ランダムデータで代用します。")
            samples = [np.random.randint(0, 256, (1, 3, INPUT_H, INPUT_W)).astype(np.float32)
                       for _ in range(3)]
    else:
        print(f"\n[3/5] キャリブレーションデータ生成 (ランダムデータ)")
        print("  --calib-dir 未指定のためランダムデータを使用")
        samples = [np.random.randint(0, 256, (1, 3, INPUT_H, INPUT_W)).astype(np.float32)
                   for _ in range(3)]

    calib_data = [samples]
    ptq_options.samples_count = len(samples)
    ptq_options.set_tensor_data(calib_data)
    print(f"  サンプル数: {len(samples)}")

    # ==========================================
    # 4. コンパイル実行
    # ==========================================
    print("\n[4/5] コンパイル実行中...")
    compiler = nncase.Compiler(compile_options)

    import_options = nncase.ImportOptions()
    with open(SIMPLIFIED_PATH, "rb") as f:
        model_content = f.read()
    compiler.import_onnx(model_content, import_options)

    compiler.use_ptq(ptq_options)
    compiler.compile()
    kmodel = compiler.gencode_tobytes()

    # ==========================================
    # 5. kmodel 保存
    # ==========================================
    with open(KMODEL_PATH, "wb") as f:
        f.write(kmodel)

    print(f"\n[5/5] kmodel 保存完了")
    print(f"  パス: {KMODEL_PATH}")
    print(f"  サイズ: {len(kmodel):,} bytes ({len(kmodel)/1024:.1f} KB)")
    print(f"  ダンプ: {DUMP_PATH}")
    print("Done.")


if __name__ == "__main__":
    main()
