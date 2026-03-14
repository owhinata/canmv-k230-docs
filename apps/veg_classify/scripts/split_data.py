"""データセットを train/val/test に分割する

カテゴリごとのディレクトリ構成から分割テキストファイルを生成:
  train.txt, val.txt, test.txt — 各行 "relative/path.jpg label_id"
  labels.txt                   — 各行にカテゴリ名 (ソート順 = label_id)
  samples.txt                  — キャリブレーション用サンプル画像パス (各カテゴリ 1 枚)
"""

import os
import random


def split_dataset(root_folder, train_ratio, val_ratio, test_ratio, output_dir):
    assert abs(train_ratio + val_ratio + test_ratio - 1.0) < 1e-6

    os.makedirs(output_dir, exist_ok=True)
    class_names = sorted(
        d for d in os.listdir(root_folder)
        if os.path.isdir(os.path.join(root_folder, d))
    )

    train_f = open(os.path.join(output_dir, "train.txt"), "w")
    val_f = open(os.path.join(output_dir, "val.txt"), "w")
    test_f = open(os.path.join(output_dir, "test.txt"), "w")
    labels_f = open(os.path.join(output_dir, "labels.txt"), "w")
    samples_f = open(os.path.join(output_dir, "samples.txt"), "w")

    for class_idx, class_name in enumerate(class_names):
        class_path = os.path.join(root_folder, class_name)
        images = sorted(f for f in os.listdir(class_path)
                        if f.lower().endswith((".jpg", ".jpeg", ".png", ".bmp")))
        random.shuffle(images)

        n = len(images)
        n_train = int(n * train_ratio)
        n_val = int(n * val_ratio)

        splits = {
            train_f: images[:n_train],
            val_f: images[n_train:n_train + n_val],
            test_f: images[n_train + n_val:],
        }

        for fobj, img_list in splits.items():
            for img_name in img_list:
                rel = os.path.join(class_name, img_name).replace("\\", "/")
                fobj.write(f"{rel} {class_idx}\n")

        labels_f.write(f"{class_name}\n")

        # calibration sample: first val image (absolute path)
        if splits[val_f]:
            sample_path = os.path.join(
                os.path.abspath(root_folder), class_name, splits[val_f][0]
            ).replace("\\", "/")
            samples_f.write(f"{sample_path}\n")

    for f in (train_f, val_f, test_f, labels_f, samples_f):
        f.close()

    print(f"Split complete: {len(class_names)} classes")
    print(f"  labels : {os.path.join(output_dir, 'labels.txt')}")
    print(f"  train  : {os.path.join(output_dir, 'train.txt')}")
    print(f"  val    : {os.path.join(output_dir, 'val.txt')}")
    print(f"  test   : {os.path.join(output_dir, 'test.txt')}")
    print(f"  samples: {os.path.join(output_dir, 'samples.txt')}")


if __name__ == "__main__":
    import yaml

    config_path = os.path.join(os.path.dirname(__file__), "config.yaml")
    with open(config_path) as f:
        config = yaml.safe_load(f)

    ds = config["dataset"]
    script_dir = os.path.dirname(os.path.abspath(__file__))
    root = os.path.join(script_dir, ds["root_folder"])
    output_dir = os.path.join(script_dir, config["train"]["save_path"])

    split_dataset(root, ds["train_ratio"], ds["val_ratio"], ds["test_ratio"],
                  output_dir)
