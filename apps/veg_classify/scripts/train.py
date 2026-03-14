"""野菜分類モデルの学習スクリプト

ResNet-18 ベースの分類モデルを学習し、ONNX + kmodel に変換する。

使い方:
  cd apps/veg_classify/scripts
  python train.py

  # CMake 経由 (出力は build/ 以下)
  cmake --build build/veg_classify --target train
"""

import argparse
import os
import sys

import yaml
import torch
import torch.nn as nn
import torch.optim as optim
import torchvision.transforms as transforms
from torch.utils.data import DataLoader
from torchvision.models import resnet18

from dataset import CustomDataset
from split_data import split_dataset

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))


def parse_args():
    parser = argparse.ArgumentParser(description="Train a vegetable classifier")
    parser.add_argument("--output-dir", default=None,
                        help="Output directory for model artifacts")
    parser.add_argument("--data-dir", default=None,
                        help="Root directory of the dataset")
    return parser.parse_args()


def load_config(output_dir=None, data_dir=None):
    config_path = os.path.join(SCRIPT_DIR, "config.yaml")
    with open(config_path) as f:
        config = yaml.safe_load(f)
    if output_dir is not None:
        config["train"]["save_path"] = output_dir
    if data_dir is not None:
        config["dataset"]["root_folder"] = data_dir
    return config


def export_onnx(model, input_shape, pth_path, onnx_path, device):
    model.load_state_dict(torch.load(pth_path, map_location=device))
    model.to(device)
    model.eval()
    dummy = torch.randn(input_shape).to(device)
    torch.onnx.export(model, dummy, onnx_path, opset_version=11,
                       dynamo=False)
    print(f"ONNX exported: {onnx_path}")


def export_kmodel(onnx_path, kmodel_path, image_size, mean, std,
                  samples_txt, ptq_option, target):
    """ONNX → 簡略化 → kmodel 変換 (nncase)"""
    try:
        import nncase
        import onnx
        import onnxsim
        import numpy as np
        from PIL import Image
    except ImportError as e:
        print(f"WARNING: kmodel export skipped ({e})")
        print("  nncase / onnxsim が必要です: pip install -r requirements.txt")
        return

    # simplify
    onnx_model = onnx.load(onnx_path)
    onnx_model = onnx.shape_inference.infer_shapes(onnx_model)
    onnx_model, ok = onnxsim.simplify(onnx_model)
    assert ok, "onnxsim simplification failed"
    simplified_path = onnx_path.replace(".onnx", "_simplified.onnx")
    onnx.save_model(onnx_model, simplified_path)

    # compile options
    co = nncase.CompileOptions()
    co.target = target
    co.preprocess = True
    co.input_shape = [1, 3, image_size[0], image_size[1]]
    co.input_type = "uint8"
    co.input_range = [0, 1]
    co.mean = mean
    co.std = std
    co.input_layout = "NCHW"
    co.dump_ir = True
    co.dump_asm = True
    co.dump_dir = os.path.dirname(kmodel_path)

    compiler = nncase.Compiler(co)
    with open(simplified_path, "rb") as f:
        compiler.import_onnx(f.read(), nncase.ImportOptions())

    # PTQ
    ptq = nncase.PTQTensorOptions()
    if ptq_option == 0:
        pass
    elif ptq_option == 1:
        ptq.calibrate_method = "NoClip"
        ptq.w_quant_type = "int16"

    # calibration data from samples.txt
    img_paths = []
    if os.path.exists(samples_txt):
        with open(samples_txt) as f:
            img_paths = [l.strip() for l in f if l.strip()]

    if not img_paths:
        img_paths = ["__random__"] * 5

    calib_data = []
    for p in img_paths:
        if p == "__random__":
            arr = np.random.randint(0, 256,
                                    (1, 3, image_size[0], image_size[1]),
                                    dtype=np.uint8)
        else:
            img = Image.open(p).convert("RGB")
            img = img.resize((image_size[1], image_size[0]), Image.BILINEAR)
            arr = np.array(img, dtype=np.uint8).transpose(2, 0, 1)[np.newaxis, ...]
        calib_data.append([arr])

    ptq.samples_count = len(calib_data)
    ptq.set_tensor_data(calib_data)
    compiler.use_ptq(ptq)

    compiler.compile()
    kmodel_bytes = compiler.gencode_tobytes()
    with open(kmodel_path, "wb") as f:
        f.write(kmodel_bytes)
    print(f"kmodel exported: {kmodel_path} ({len(kmodel_bytes):,} bytes)")


def main():
    args = parse_args()
    config = load_config(output_dir=args.output_dir, data_dir=args.data_dir)
    ds_cfg = config["dataset"]
    tr_cfg = config["train"]
    dp_cfg = config["deploy"]

    root_folder = os.path.join(SCRIPT_DIR, ds_cfg["root_folder"])
    save_path = os.path.join(SCRIPT_DIR, tr_cfg["save_path"])
    os.makedirs(save_path, exist_ok=True)

    device = tr_cfg.get("device", "cpu")
    image_size = tr_cfg["image_size"]
    mean = tr_cfg["mean"]
    std = tr_cfg["std"]
    epochs = tr_cfg["epochs"]
    batch_size = tr_cfg["batch_size"]
    lr = tr_cfg["learning_rate"]

    # --- split dataset ---
    if ds_cfg.get("split", True):
        split_dataset(root_folder,
                      ds_cfg["train_ratio"], ds_cfg["val_ratio"],
                      ds_cfg["test_ratio"], save_path)

    train_txt = os.path.join(save_path, "train.txt")
    val_txt = os.path.join(save_path, "val.txt")
    test_txt = os.path.join(save_path, "test.txt")
    labels_txt = os.path.join(save_path, "labels.txt")
    samples_txt = os.path.join(save_path, "samples.txt")

    # --- load labels ---
    with open(labels_txt) as f:
        class_names = [l.strip() for l in f if l.strip()]
    num_classes = len(class_names)
    print(f"\nClasses ({num_classes}): {class_names}")

    # --- transforms ---
    transform = transforms.Compose([
        transforms.Resize(image_size),
        transforms.ToTensor(),
        transforms.Normalize(mean=mean, std=std),
    ])

    # --- dataloaders ---
    train_loader = DataLoader(
        CustomDataset(train_txt, root_folder, transform),
        batch_size=batch_size, shuffle=True, num_workers=0)
    val_loader = DataLoader(
        CustomDataset(val_txt, root_folder, transform),
        batch_size=batch_size, shuffle=False, num_workers=0)
    test_loader = DataLoader(
        CustomDataset(test_txt, root_folder, transform),
        batch_size=batch_size, shuffle=False, num_workers=0)

    # --- model ---
    model = resnet18(num_classes=num_classes)
    model.to(device)
    criterion = nn.CrossEntropyLoss()
    optimizer = optim.Adam(model.parameters(), lr=lr)

    best_val_acc = 0.0
    best_pth = os.path.join(save_path, "best.pth")

    # --- training loop ---
    for epoch in range(epochs):
        model.train()
        running_loss = 0.0
        for i, (inputs, labels) in enumerate(train_loader):
            inputs, labels = inputs.to(device), labels.to(device)
            optimizer.zero_grad()
            outputs = model(inputs)
            loss = criterion(outputs, labels)
            loss.backward()
            optimizer.step()
            running_loss += loss.item()
            if (i + 1) % 10 == 0:
                print(f"  Epoch {epoch+1}/{epochs}, Batch {i+1}, "
                      f"Loss: {running_loss/10:.4f}")
                running_loss = 0.0

        # --- validation ---
        model.eval()
        correct = total = 0
        with torch.no_grad():
            for inputs, labels in val_loader:
                inputs, labels = inputs.to(device), labels.to(device)
                outputs = model(inputs)
                _, predicted = torch.max(outputs, 1)
                correct += (predicted == labels).sum().item()
                total += labels.size(0)
        val_acc = correct / total if total > 0 else 0.0
        print(f"Epoch {epoch+1}/{epochs}, Val Accuracy: {val_acc:.3f}")

        if val_acc > best_val_acc:
            best_val_acc = val_acc
            torch.save(model.state_dict(), best_pth)

    # --- save last ---
    last_pth = os.path.join(save_path, "last.pth")
    torch.save(model.state_dict(), last_pth)
    print(f"\nTraining complete. Best val acc: {best_val_acc:.3f}")

    # --- test ---
    model.load_state_dict(torch.load(best_pth, map_location=device))
    model.eval()
    correct = total = 0
    with torch.no_grad():
        for inputs, labels in test_loader:
            inputs, labels = inputs.to(device), labels.to(device)
            outputs = model(inputs)
            _, predicted = torch.max(outputs, 1)
            correct += (predicted == labels).sum().item()
            total += labels.size(0)
    print(f"Test Accuracy: {correct/total:.3f}")

    # --- ONNX export ---
    onnx_path = os.path.join(save_path, "best.onnx")
    export_onnx(model, [1, 3, image_size[0], image_size[1]],
                best_pth, onnx_path, device)

    # --- kmodel export ---
    kmodel_path = os.path.join(save_path, "best.kmodel")
    export_kmodel(onnx_path, kmodel_path, image_size, mean, std,
                  samples_txt, dp_cfg.get("ptq_option", 0),
                  dp_cfg.get("target", "k230"))


if __name__ == "__main__":
    main()
