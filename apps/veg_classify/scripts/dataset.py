"""カスタムデータセットクラス

train/val/test の分割テキストファイルと画像ルートディレクトリから
PyTorch Dataset を構築する。
"""

import os
from PIL import Image
from torch.utils.data import Dataset


class CustomDataset(Dataset):
    def __init__(self, txt_file, root_folder, transform=None):
        self.root_folder = root_folder
        self.transform = transform
        self.data = []
        with open(txt_file, "r") as f:
            for line in f:
                img_name, label = line.strip().split()
                self.data.append((img_name, int(label)))

    def __len__(self):
        return len(self.data)

    def __getitem__(self, idx):
        img_name, label = self.data[idx]
        img_path = os.path.join(self.root_folder, img_name)
        image = Image.open(img_path).convert("RGB")
        if self.transform:
            image = self.transform(image)
        return image, label
