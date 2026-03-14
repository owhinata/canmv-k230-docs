# veg_classify — Vegetable Classification Application

`veg_classify` is a vegetable image classification tutorial for the K230. It provides a **step-by-step end-to-end workflow** from training a ResNet-18 model on PC to kmodel conversion and deployment on the K230 device.

## Prerequisites

- K230 SDK must be built (toolchain extracted, MPP libraries compiled)
- SDK placed at `k230_sdk/` in the repository root
- Python 3.8 or later (see `requirements.txt`)
- Host OS: x86_64 Linux
- CMake 3.16 or later

!!! note "Building the SDK"
    For K230 SDK build instructions, see [SDK Build](../development/sdk_build.md).

## Overall Workflow

```
Dataset (image folders)
  |
  +-- split_data.py: train/val/test split
  |
  +-- train.py: ResNet-18 training (CPU)
  |     +-- best.pth  (PyTorch weights)
  |     +-- best.onnx (ONNX export)
  |     +-- best.kmodel (nncase compile)
  |
  +-- step1: ONNX model analysis
  +-- step2: ONNX simplification (onnxsim)
  +-- step3: kmodel compilation (PTQ quantization)
  +-- step4: kmodel simulation
  +-- step5: Accuracy comparison with ONNX Runtime
                  |
              kmodel --> K230 on-device app
                          |
                    +-----+-----+
                    | Camera     |
                    | AI2D -> KPU|
                    | Result     |
                    +-----------+
```

---

## Part 1: Model Training (PC)

### Environment Setup

Use the `.venv` at the repository root (shared with MkDocs):

```bash
source .venv/bin/activate
pip install -r requirements.txt
```

!!! tip "No GPU required"
    `config.yaml` is set to `device: cpu`. With the sample dataset (5 categories x 50 images), training completes in a few minutes on CPU.

### Dataset Structure

The sample dataset is included in `apps/veg_classify/data/` (from [kendryte/K230_training_scripts](https://github.com/kendryte/K230_training_scripts/tree/main/end2end_cls_doc/data/veg_cls)):

```
data/
+-- broccoli/    # Broccoli (50 images)
+-- carrot/      # Carrot (50 images)
+-- eggplant/    # Eggplant (50 images)
+-- spinach/     # Spinach (50 images)
+-- tomato/      # Tomato (50 images)
```

To use a custom dataset, organize it in the same folder structure (category-named directories containing image files).

### Training Configuration (config.yaml)

| Parameter | Default | Description |
|-----------|---------|-------------|
| `dataset.root_folder` | `../data` | Dataset root |
| `dataset.split` | `true` | Whether to split data |
| `dataset.train_ratio` | `0.7` | Training set ratio |
| `dataset.val_ratio` | `0.15` | Validation set ratio |
| `dataset.test_ratio` | `0.15` | Test set ratio |
| `train.device` | `cpu` | Training device |
| `train.image_size` | `[224, 224]` | Input image size |
| `train.mean` | `[0.485, 0.456, 0.406]` | ImageNet normalization mean |
| `train.std` | `[0.229, 0.224, 0.225]` | ImageNet normalization std |
| `train.epochs` | `10` | Number of epochs |
| `train.batch_size` | `8` | Batch size |
| `train.learning_rate` | `0.001` | Learning rate |
| `deploy.target` | `k230` | Target chip |
| `deploy.ptq_option` | `0` | Quantization type (0=uint8) |

### Running Training

#### Direct Execution

```bash
cd apps/veg_classify/scripts
python train.py
```

#### CMake train Target

The CMake `train` target automates venv creation, dependency installation, and dataset change detection:

```bash
cmake -B build/veg_classify -S apps/veg_classify \
  -DCMAKE_TOOLCHAIN_FILE="$(pwd)/cmake/toolchain-k230-rtsmart.cmake"
cmake --build build/veg_classify --target train
```

The `train` target:

1. Creates `.venv` if it doesn't exist and installs dependencies from `requirements.txt`
2. Hashes the dataset file structure and compares with the previous run
3. Skips training if unchanged (`Dataset unchanged. Skipping training.`)
4. Runs `train.py` if the dataset has changed

To use an external dataset, specify the `DATA_DIR` option:

```bash
cmake -B build/veg_classify -S apps/veg_classify \
  -DCMAKE_TOOLCHAIN_FILE="$(pwd)/cmake/toolchain-k230-rtsmart.cmake" \
  -DDATA_DIR=/path/to/custom/dataset
```

!!! tip "How change detection works"
    `check_data_hash.sh` computes an MD5 hash from the paths and sizes of all files in the dataset directory. It does not read file contents, so it runs fast even with large datasets. It detects file additions, deletions, and size changes.

`train.py` runs the following steps end-to-end:

1. Dataset splitting (`split_data.py`)
2. ResNet-18 training (PyTorch)
3. ONNX export
4. kmodel conversion (if nncase is installed)

Output files (`build/veg_classify/output/`):

| File | Description |
|------|-------------|
| `labels.txt` | Class name list |
| `train.txt` / `val.txt` / `test.txt` | Data split results |
| `samples.txt` | Calibration sample paths |
| `best.pth` | Best validation accuracy weights |
| `last.pth` | Last epoch weights |
| `best.onnx` | ONNX model |
| `best.kmodel` | K230 kmodel |

---

## Part 2: kmodel Conversion (PC)

While `train.py` automatically converts to kmodel, you can run each step individually to inspect and improve accuracy.

### Script List

| Script | Description |
|--------|-------------|
| `step1_parse_model.py` | Analyze ONNX model input/output |
| `step2_simplify_model.py` | Simplify ONNX with onnxsim |
| `step3_compile_kmodel.py` | Compile to kmodel (PTQ quantization) |
| `step4_simulate_kmodel.py` | Run kmodel simulation |
| `step5_compare_results.py` | Compare accuracy with ONNX Runtime |

### Step 1: Model Analysis

```bash
python apps/veg_classify/scripts/step1_parse_model.py
```

Key information:

- Input: float32 `[1, 3, 224, 224]` (NCHW)
- Output: 1 tensor `[1, num_classes]`

### Step 2: Model Simplification

```bash
python apps/veg_classify/scripts/step2_simplify_model.py
```

Output: `apps/veg_classify/output/simplified.onnx`

### Step 3: kmodel Compilation

#### Compilation Settings

| Option | Value |
|--------|-------|
| preprocess | True |
| input_type | uint8 |
| input_range | [0, 1] |
| mean | [0.485, 0.456, 0.406] (ImageNet) |
| std | [0.229, 0.224, 0.225] (ImageNet) |
| quant_type | uint8 |
| calibrate_method | Kld |

!!! tip "What preprocess=True means"
    The kmodel internally handles uint8-to-float32 conversion and mean/std normalization.
    On the device, raw camera data (uint8) can be fed directly as input.

#### Compile with Random Data (Initial)

```bash
python apps/veg_classify/scripts/step3_compile_kmodel.py
```

#### Compile with Captured Images (Improved Accuracy)

```bash
python apps/veg_classify/scripts/step3_compile_kmodel.py --calib-dir /path/to/captures/
```

Output: `apps/veg_classify/output/dump/veg_classify.kmodel`

### Step 4: Simulation

```bash
python apps/veg_classify/scripts/step4_simulate_kmodel.py
python apps/veg_classify/scripts/step4_simulate_kmodel.py --image photo.jpg
```

### Step 5: Accuracy Comparison

```bash
python apps/veg_classify/scripts/step5_compare_results.py
```

Accuracy guidelines:

| Cosine Similarity | Rating |
|-------------------|--------|
| 0.999 or above | excellent |
| 0.99 or above | good |
| 0.95 or above | acceptable |
| Below 0.95 | poor — needs improvement |

### Calibration Improvement Cycle

!!! note "How to improve accuracy"
    1. Compile with random data (step3) and verify on the device
    2. Use 'c' key on the device app to capture images from the real environment
    3. Recompile with `step3 --calib-dir` using captured images
    4. Verify accuracy with `step5`
    5. Repeat steps 2-4 as needed

---

## Part 3: On-Device Application (C++)

### Source Files

| File | Description |
|------|-------------|
| [`main.cc`][main] | Main application — VICAP/VO initialization, inference loop, capture feature |
| [`model.h`][model-h] / [`model.cc`][model-cc] | `Model` abstract base class — kmodel loading and inference pipeline |
| [`classifier.h`][cls-h] / [`classifier.cc`][cls-cc] | `Classifier` class — AI2D resize preprocessing, softmax postprocessing |
| [`util.h`][util-h] / [`util.cc`][util-cc] | Utilities (`ScopedTiming`, etc.) |
| [`vo_test_case.h`][vo-h] | VO layer helper type declarations |

[main]: https://github.com/owhinata/canmv-k230/blob/be53063/apps/veg_classify/src/main.cc
[model-h]: https://github.com/owhinata/canmv-k230/blob/be53063/apps/veg_classify/src/model.h
[model-cc]: https://github.com/owhinata/canmv-k230/blob/be53063/apps/veg_classify/src/model.cc
[cls-h]: https://github.com/owhinata/canmv-k230/blob/be53063/apps/veg_classify/src/classifier.h
[cls-cc]: https://github.com/owhinata/canmv-k230/blob/be53063/apps/veg_classify/src/classifier.cc
[util-h]: https://github.com/owhinata/canmv-k230/blob/be53063/apps/veg_classify/src/util.h
[util-cc]: https://github.com/owhinata/canmv-k230/blob/be53063/apps/veg_classify/src/util.cc
[vo-h]: https://github.com/owhinata/canmv-k230/blob/be53063/apps/veg_classify/src/vo_test_case.h

### Inference Pipeline

```
Sensor (OV5647)
  |
  +-- CHN0 (1920x1080 YUV420) --> VO Layer --> HDMI Display
  |
  +-- CHN1 (1280x720 RGB888P) --> AI Inference
                                    |
                              +-----+-----+
                              | AI2D      |
                              | Preprocess|
                              | (224x224  |
                              | stretch)  |
                              +-----+-----+
                                    |
                              +-----+-----+
                              | KPU       |
                              | Inference |
                              | (ResNet-18)|
                              +-----+-----+
                                    |
                              +-----+-----+
                              | Postprocess|
                              | (softmax   |
                              |  + argmax) |
                              +-----+-----+
                                    |
                          +---------+---------+
                          |                   |
                    Console Output       Capture
                    (classification)     ('c' key)
                          |                   |
                          v                   v
                    Class: bocai        PNG Save (OpenCV)
                    (95.3%)
```

### Build Steps

#### 1. Configure

```bash
cmake -B build/veg_classify -S apps/veg_classify \
  -DCMAKE_TOOLCHAIN_FILE="$(pwd)/cmake/toolchain-k230-rtsmart.cmake"
```

#### 2. Build

```bash
cmake --build build/veg_classify
```

#### 3. Verify

```bash
file build/veg_classify/veg_classify
```

Expected output:

```
veg_classify: ELF 64-bit LSB executable, UCB RISC-V, RVC, double-float ABI, version 1 (SYSV), statically linked, ...
```

### Command-Line Arguments

```
./veg_classify <kmodel> <labels.txt> [capture_dir]
```

| Argument | Description |
|----------|-------------|
| `<kmodel>` | Path to the classification kmodel file |
| `<labels.txt>` | Class label file (one label per line) |
| `[capture_dir]` | Directory to save captured images (optional) |

### Key Controls

| Key | Action |
|-----|--------|
| c + Enter | Save current frame as PNG (only when `capture_dir` is specified) |
| q + Enter | Quit the application |

### Transferring and Running on K230

#### Transfer via SCP

```bash
scp build/veg_classify/veg_classify root@<K230_IP_ADDRESS>:/sharefs/
scp apps/veg_classify/output/best.kmodel root@<K230_IP_ADDRESS>:/sharefs/veg_classify.kmodel
scp apps/veg_classify/output/labels.txt root@<K230_IP_ADDRESS>:/sharefs/
```

#### Run on the K230 bigcore (msh)

```
msh /> /sharefs/veg_classify /sharefs/veg_classify.kmodel /sharefs/labels.txt
```

#### Run in Capture Mode

```
msh /> mkdir /sharefs/calib
msh /> /sharefs/veg_classify /sharefs/veg_classify.kmodel /sharefs/labels.txt /sharefs/calib
```

!!! tip "Calibration capture"
    Run with `capture_dir` specified and press 'c' + Enter several times to capture images from the real environment.
    Transfer the captured images to your PC and use them as calibration data for `step3 --calib-dir`.

    ```bash
    scp root@<K230_IP_ADDRESS>:/sharefs/calib/*.png ./calib/
    python apps/veg_classify/scripts/step3_compile_kmodel.py --calib-dir ./calib/
    ```

!!! tip "Serial connection"
    - **Bigcore (RT-Smart msh)**: `/dev/ttyACM1` at 115200 bps

    ```bash
    minicom -D /dev/ttyACM1 -b 115200
    ```
