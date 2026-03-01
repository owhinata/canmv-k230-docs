# face_detect — Face Detection Application

`face_detect` is a face detection application for the K230. It consists of two workflows: Python scripts for kmodel compilation and accuracy evaluation, and a C++ on-device application.

## Prerequisites

- K230 SDK must be built (toolchain extracted, MPP libraries compiled)
- SDK placed at `k230_sdk/` in the repository root
- Python 3.8 or later (see `requirements.txt`)
- Host OS: x86_64 Linux
- CMake 3.16 or later

!!! note "Building the SDK"
    For K230 SDK build instructions, see [SDK Build](../development/sdk_build.md).

## Overview

`face_detect` is based on [`sample_face_ae`](../development/sample_face_ae.md), with the following additions:

- **Python scripts**: Compile ONNX models to K230 kmodel format and evaluate accuracy
- **Capture feature**: Press 'c' to save the current frame as PNG on the device
- **Input thread**: 'c' to capture, 'q' to quit
- **OpenCV linking**: Uses OpenCV for PNG encoding and saving

### Differences from sample_face_ae

| Feature | sample_face_ae | face_detect |
|---------|---------------|-------------|
| Face detection + AE ROI | Yes | Yes |
| Capture feature | No | Yes (OpenCV) |
| Input thread | No | Yes ('c'/'q') |
| OpenCV linking | No | Yes |
| Python scripts | No | Yes |

### Overall Workflow

```
ONNX Model
  |
  +-- step1: Model analysis (inspect input/output info)
  +-- step2: Model simplification (onnxsim)
  +-- step3: kmodel compilation (PTQ quantization)
  +-- step4: Simulation execution
  +-- step5: Accuracy comparison with ONNX Runtime
                    |
                kmodel --> On-device app --> Captured images
                                              |
                                    Recompile with step3 --calib-dir
                                              |
                                    Verify with evaluate_kmodel.py
```

---

## Part 1: kmodel Compilation (PC)

### What is nncase?

nncase is a neural network compiler for the K230 KPU (Knowledge Process Unit). It converts ONNX models to kmodel format using PTQ (Post-Training Quantization).

References:

- [CanMV K230 Tutorial — Model Compilation](https://www.kendryte.com/k230/en/dev/CanMV_K230_Tutorial.html#model-compilation-and-simulator-inference)
- [nncase examples — K230 Simulate (GitHub)](https://github.com/kendryte/nncase/blob/master/examples/user_guide/k230_simulate-EN.ipynb)
- Official scripts bundled with the SDK: `k230_sdk/src/big/nncase/examples/scripts/`

### Python Environment Setup

Use the `.venv` at the repository root (shared with MkDocs):

```bash
source .venv/bin/activate
pip install -r requirements.txt
```

### Script List

| Script | Description |
|--------|-------------|
| `step1_parse_model.py` | Analyze ONNX model input/output |
| `step2_simplify_model.py` | Simplify ONNX with onnxsim |
| `step3_compile_kmodel.py` | Compile to kmodel (PTQ quantization) |
| `step4_simulate_kmodel.py` | Run kmodel simulation |
| `step5_compare_results.py` | Compare accuracy with ONNX Runtime |
| `evaluate_kmodel.py` | Batch accuracy evaluation |

### Step 1: Model Analysis

Inspect the ONNX model's input/output information.

```bash
python apps/face_detect/scripts/step1_parse_model.py
```

Key information:

- Input: float32 `[1, 3, 320, 320]` (NCHW)
- Output: 9 tensors — 3 scales x (classification, bbox, landmark)

### Step 2: Model Simplification

Optimize the ONNX model using onnxsim. This removes redundant nodes and improves compilation stability.

```bash
python apps/face_detect/scripts/step2_simplify_model.py
```

Output: `apps/face_detect/scripts/output/simplified.onnx`

### Step 3: kmodel Compilation

Compile the simplified ONNX to a K230-targeted kmodel.

#### Compilation Settings

| Option | Value |
|--------|-------|
| preprocess | True |
| input_type | uint8 |
| input_range | [0, 255] |
| mean | [123, 117, 104] (RGB order) |
| std | [1, 1, 1] |
| quant_type | uint8 |
| calibrate_method | Kld |

!!! tip "What preprocess=True means"
    The kmodel internally handles uint8-to-float32 conversion and mean/std normalization.
    On the device, raw camera data (uint8) can be fed directly as input.

#### Compile with Random Data (Initial)

```bash
python apps/face_detect/scripts/step3_compile_kmodel.py
```

#### Compile with Captured Images (Improved Accuracy)

```bash
python apps/face_detect/scripts/step3_compile_kmodel.py --calib-dir /path/to/captures/
```

Output: `apps/face_detect/scripts/output/dump/mobile_retinaface.kmodel`

### Step 4: Simulation

Run the compiled kmodel on the PC simulator.

```bash
# Use the SDK-bundled test image
python apps/face_detect/scripts/step4_simulate_kmodel.py

# Use a custom image
python apps/face_detect/scripts/step4_simulate_kmodel.py --image photo.png
```

Output: `.npy` files for each output tensor are saved in `apps/face_detect/scripts/output/dump/`.

### Step 5: Accuracy Comparison

Compare the kmodel simulation results with ONNX Runtime inference using cosine similarity.

```bash
python apps/face_detect/scripts/step5_compare_results.py
```

Accuracy guidelines:

| Cosine Similarity | Rating |
|-------------------|--------|
| 0.999 or above | excellent |
| 0.99 or above | good |
| 0.95 or above | acceptable |
| Below 0.95 | poor — needs improvement |

### Batch Accuracy Evaluation

Compare kmodel and ONNX Runtime outputs in batch for all images in a directory.

```bash
python apps/face_detect/scripts/evaluate_kmodel.py /path/to/images/
```

This displays min/mean/max cosine similarity for each output.

### Calibration Improvement Cycle

!!! note "How to improve accuracy"
    1. Compile with random data (step3) and verify on the device
    2. Use 'c' key on the device app to capture images from the real environment
    3. Recompile with `step3 --calib-dir` using captured images
    4. Verify accuracy with `evaluate_kmodel.py`
    5. Repeat steps 2–4 as needed

---

## Part 2: On-Device Application (C++)

### Source Files

| File | Description |
|------|-------------|
| [`main.cc`][main] | Main application — VICAP/VO initialization, inference loop, capture feature |
| [`model.h`][model-h] / [`model.cc`][model-cc] | `Model` abstract base class — kmodel loading and inference pipeline |
| [`mobile_retinaface.h`][mr-h] / [`mobile_retinaface.cc`][mr-cc] | `MobileRetinaface` class — face detection model (AI2D preprocessing, anchor decoding, NMS) |
| [`face_ae_roi.h`][far-h] / [`face_ae_roi.cc`][far-cc] | `FaceAeRoi` class — maps face coordinates to ISP AE ROI |
| [`util.h`][util-h] / [`util.cc`][util-cc] | Utility types (`box_t`, `face_coordinate`) and helpers |
| [`anchors_320.cc`][anchors] | Pre-computed anchor boxes for 320x320 input |
| [`vo_test_case.h`][vo-h] | VO layer helper type (`layer_info`) declarations |

[main]: https://github.com/owhinata/canmv-k230/blob/2dd0691/apps/face_detect/src/main.cc
[model-h]: https://github.com/owhinata/canmv-k230/blob/2dd0691/apps/face_detect/src/model.h
[model-cc]: https://github.com/owhinata/canmv-k230/blob/2dd0691/apps/face_detect/src/model.cc
[mr-h]: https://github.com/owhinata/canmv-k230/blob/2dd0691/apps/face_detect/src/mobile_retinaface.h
[mr-cc]: https://github.com/owhinata/canmv-k230/blob/2dd0691/apps/face_detect/src/mobile_retinaface.cc
[far-h]: https://github.com/owhinata/canmv-k230/blob/2dd0691/apps/face_detect/src/face_ae_roi.h
[far-cc]: https://github.com/owhinata/canmv-k230/blob/2dd0691/apps/face_detect/src/face_ae_roi.cc
[util-h]: https://github.com/owhinata/canmv-k230/blob/2dd0691/apps/face_detect/src/util.h
[util-cc]: https://github.com/owhinata/canmv-k230/blob/2dd0691/apps/face_detect/src/util.cc
[anchors]: https://github.com/owhinata/canmv-k230/blob/2dd0691/apps/face_detect/src/anchors_320.cc
[vo-h]: https://github.com/owhinata/canmv-k230/blob/2dd0691/apps/face_detect/src/vo_test_case.h

### Processing Flow

```
Sensor (OV5647)
  |
  +-- CHN0 (1920x1080 YUV420) --> VO Layer --> HDMI Display
  |                                    ^
  |                              Face box drawing (kd_mpi_vo_draw_frame)
  |
  +-- CHN1 (1280x720 RGB888P) --> AI Inference
                                    |
                              +-----+-----+
                              | AI2D      |
                              | Preprocess|
                              | (resize+pad) |
                              +-----+-----+
                                    |
                              +-----+-----+
                              | KPU       |
                              | Inference |
                              | (MobileRetinaface) |
                              +-----+-----+
                                    |
                              +-----+-----+
                              | Postprocess|
                              | (decode+NMS) |
                              +-----+-----+
                                    |
                          +---------+---------+
                          |         |         |
                    AE ROI      Face box   Capture
                    Update      Drawing    ('c' key)
                    (FaceAeRoi)             |
                          |                 v
                          v           PNG Save (OpenCV)
                    ISP AE Engine
```

### Build Steps

#### 1. Configure

```bash
cmake -B build/face_detect -S apps/face_detect \
  -DCMAKE_TOOLCHAIN_FILE="$(pwd)/cmake/toolchain-k230-rtsmart.cmake"
```

#### 2. Build

```bash
cmake --build build/face_detect
```

#### 3. Verify

```bash
file build/face_detect/face_detect
```

Expected output:

```
face_detect: ELF 64-bit LSB executable, UCB RISC-V, RVC, double-float ABI, version 1 (SYSV), statically linked, ...
```

### CMakeLists.txt Details

The `apps/face_detect/CMakeLists.txt` handles:

- **MPP include paths**: Headers from `mpp/include/`, `mpp/include/comm/`, `mpp/include/ioctl/`, and `mpp/userapps/api/`
- **NNCASE include paths**: Headers from `nncase/include/`, `nncase/include/nncase/runtime/`, and `rvvlib/include/`
- **OpenCV include paths**: Headers from `opencv_thead/include/opencv4/`
- **MPP static libraries**: All MPP libraries linked with `--start-group` / `--end-group` to resolve circular dependencies
- **NNCASE libraries**: `Nncase.Runtime.Native`, `nncase.rt_modules.k230`, `functional_k230`, `rvv`
- **OpenCV libraries**: `opencv_imgcodecs`, `opencv_imgproc`, `opencv_core` and 3rdparty libraries
- **C++20**: Requires C++20 via `target_compile_features`

### Command-Line Arguments

```
./face_detect <kmodel> <ae_roi> [capture_dir]
```

| Argument | Description |
|----------|-------------|
| `<kmodel>` | Path to the face detection kmodel file (e.g., `/sharefs/mobile_retinaface.kmodel`) |
| `<ae_roi>` | Enable AE ROI: `1` = enabled, `0` = disabled |
| `[capture_dir]` | Directory to save captured images (optional) |

### Key Controls

| Key | Action |
|-----|--------|
| c + Enter | Save current frame as PNG (only when `capture_dir` is specified) |
| q + Enter | Quit the application |

### Transferring and Running on K230

#### Transfer via SCP

```bash
scp build/face_detect/face_detect root@<K230_IP_ADDRESS>:/sharefs/face_detect
scp apps/face_detect/scripts/output/dump/mobile_retinaface.kmodel root@<K230_IP_ADDRESS>:/sharefs/
```

#### Run on the K230 bigcore (msh)

On the K230 serial console (ACM1), run:

```
msh /> /sharefs/face_detect /sharefs/mobile_retinaface.kmodel 1
```

To run with AE ROI disabled:

```
msh /> /sharefs/face_detect /sharefs/mobile_retinaface.kmodel 0
```

#### Run in Capture Mode

To capture images for calibration, specify `capture_dir`:

```
msh /> mkdir /sharefs/calib
msh /> /sharefs/face_detect /sharefs/mobile_retinaface.kmodel 1 /sharefs/calib
```

!!! tip "Calibration capture"
    Run with `capture_dir` specified and press 'c' + Enter several times to capture images from the real environment.
    Transfer the captured images to your PC and use them as calibration data for `step3 --calib-dir`.

    ```bash
    # Transfer captured images from K230 to PC
    scp root@<K230_IP_ADDRESS>:/sharefs/calib/*.png ./calib/

    # Recompile with captured images for calibration
    python apps/face_detect/scripts/step3_compile_kmodel.py --calib-dir ./calib/
    ```

!!! tip "Serial connection"
    - **Bigcore (RT-Smart msh)**: `/dev/ttyACM1` at 115200 bps

    ```bash
    minicom -D /dev/ttyACM1 -b 115200
    ```
