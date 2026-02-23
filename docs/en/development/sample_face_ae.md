# sample_face_ae

This guide explains how to build the K230 `sample_face_ae` application using CMake out-of-tree build. This sample uses a face detection model (MobileRetinaface) to detect faces and maps the detected face regions to the ISP AE (Auto Exposure) ROI, achieving face-optimized exposure control.

## Prerequisites

- K230 SDK must be built (toolchain extracted, MPP libraries compiled)
- SDK placed at `k230_sdk/` in the repository root
- Face detection kmodel file (`mobile_retinaface.kmodel`)
- Host OS: x86_64 Linux
- CMake 3.16 or later

!!! note "Building the SDK"
    For K230 SDK build instructions, see [SDK Build](sdk_build.md).

## Overview

`sample_face_ae` is based on the K230 SDK face detection sample (`sample_face_ae`), combining AI inference with ISP AE ROI control. It demonstrates:

- Configuring camera sensors via the VICAP API (2 channels: YUV420 for display + RGB888P for AI inference)
- Loading kmodel and AI2D preprocessing with the NNCASE runtime
- Face detection using the MobileRetinaface model (bounding boxes + landmarks)
- Mapping detected face regions to ISP AE ROI (area-weighted)
- Real-time preview on VO display with face bounding box overlay

### Source Files

| File | Description |
|------|-------------|
| [`main.cc`][main] | Main application — VB/VICAP/VO initialization, AI inference loop, cleanup |
| [`model.h`][model-h] / [`model.cc`][model-cc] | `Model` abstract base class — kmodel loading and inference pipeline |
| [`mobile_retinaface.h`][mr-h] / [`mobile_retinaface.cc`][mr-cc] | `MobileRetinaface` class — face detection model (AI2D preprocessing, anchor decoding, NMS) |
| [`face_ae_roi.h`][far-h] / [`face_ae_roi.cc`][far-cc] | `FaceAeRoi` class — maps face coordinates to ISP AE ROI |
| [`util.h`][util-h] / [`util.cc`][util-cc] | Utility types (`box_t`, `face_coordinate`) and helpers |
| [`anchors_320.cc`][anchors] | Pre-computed anchor boxes for 320x320 input |
| [`vo_test_case.h`][vo-h] | VO layer helper type (`layer_info`) declarations |

[main]: https://github.com/owhinata/canmv-k230/blob/4f9b08c/apps/sample_face_ae/src/main.cc
[model-h]: https://github.com/owhinata/canmv-k230/blob/4f9b08c/apps/sample_face_ae/src/model.h
[model-cc]: https://github.com/owhinata/canmv-k230/blob/4f9b08c/apps/sample_face_ae/src/model.cc
[mr-h]: https://github.com/owhinata/canmv-k230/blob/4f9b08c/apps/sample_face_ae/src/mobile_retinaface.h
[mr-cc]: https://github.com/owhinata/canmv-k230/blob/4f9b08c/apps/sample_face_ae/src/mobile_retinaface.cc
[far-h]: https://github.com/owhinata/canmv-k230/blob/4f9b08c/apps/sample_face_ae/src/face_ae_roi.h
[far-cc]: https://github.com/owhinata/canmv-k230/blob/4f9b08c/apps/sample_face_ae/src/face_ae_roi.cc
[util-h]: https://github.com/owhinata/canmv-k230/blob/4f9b08c/apps/sample_face_ae/src/util.h
[util-cc]: https://github.com/owhinata/canmv-k230/blob/4f9b08c/apps/sample_face_ae/src/util.cc
[anchors]: https://github.com/owhinata/canmv-k230/blob/4f9b08c/apps/sample_face_ae/src/anchors_320.cc
[vo-h]: https://github.com/owhinata/canmv-k230/blob/4f9b08c/apps/sample_face_ae/src/vo_test_case.h

## Processing Flow

The application follows this data flow:

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
                              +-----+-----+
                              | AE ROI    |
                              | Update    |
                              | (FaceAeRoi) |
                              +-----+-----+
                                    |
                                    v
                              ISP AE Engine
```

## Class Reference

### Model — Abstract Base Class

**Source:** [`model.h` L13–L39][model-class] / [`model.cc`][model-cc]

Abstract base class that manages kmodel loading and the inference pipeline. Subclasses implement `Preprocess()` and `Postprocess()`.

| Method | Description |
|--------|-------------|
| `Model(model_name, kmodel_file)` | Loads the kmodel file and creates input tensors |
| `Run(vaddr, paddr)` | Executes the inference pipeline: `Preprocess` → `KpuRun` → `Postprocess` |
| `Preprocess(vaddr, paddr)` | Pure virtual — preprocesses input data |
| `KpuRun()` | Runs the kmodel on the KPU |
| `Postprocess()` | Pure virtual — postprocesses inference results |
| `InputTensor(idx)` / `OutputTensor(idx)` | Access input/output tensors |
| `InputShape(idx)` / `OutputShape(idx)` | Get input/output shapes |

**Member variables:**

| Variable | Description |
|----------|-------------|
| `ai2d_builder_` | AI2D preprocessing pipeline builder |
| `ai2d_in_tensor_` / `ai2d_out_tensor_` | AI2D input/output tensors |
| `interp_` | NNCASE runtime interpreter |

[model-class]: https://github.com/owhinata/canmv-k230/blob/4f9b08c/apps/sample_face_ae/src/model.h#L13-L39

### MobileRetinaface — Face Detection Model

**Source:** [`mobile_retinaface.h` L13–L48][mr-class] / [`mobile_retinaface.cc`][mr-cc]

Inherits from `Model` and implements preprocessing and postprocessing for the MobileRetinaface face detection model.

| Method | Description |
|--------|-------------|
| `MobileRetinaface(kmodel_file, channel, height, width)` | Builds the AI2D preprocessing pipeline (resize + padding) |
| `GetResult()` | Returns detection results (bounding boxes + landmarks) |
| `Preprocess(vaddr, paddr)` | Creates AI2D tensor from VICAP frame and runs preprocessing |
| `Postprocess()` | Decodes 9 output tensors and filters with NMS |

**Postprocessing flow:**

1. **Confidence decoding** (`DealConfOpt`) — Processes confidence scores from 3 scales with softmax, selects objects above threshold (`obj_threshold_` = 0.6)
2. **Location decoding** (`DealLocOpt`) — Decodes bounding box positions using anchor boxes
3. **Landmark decoding** (`DealLandmsOpt`) — Decodes 5-point facial landmarks
4. **NMS** — Removes overlapping boxes using IoU threshold (`nms_threshold_` = 0.5)
5. **Coordinate conversion** — Converts from model coordinates to camera input coordinates

[mr-class]: https://github.com/owhinata/canmv-k230/blob/4f9b08c/apps/sample_face_ae/src/mobile_retinaface.h#L13-L48

### FaceAeRoi — AE ROI Control

**Source:** [`face_ae_roi.h` L9–L21][far-class] / [`face_ae_roi.cc`][far-cc]

Converts detected face coordinates into ISP AE ROI windows, enabling face-optimized auto exposure control.

| Method | Description |
|--------|-------------|
| `FaceAeRoi(dev, model_w, model_h, sensor_w, sensor_h)` | Sets the ISP device and model/sensor resolutions |
| `SetEnable(enable)` | Enables/disables the ISP AE ROI feature (`kd_mpi_isp_ae_roi_set_enable`) |
| `Update(boxes)` | Converts face bounding boxes to AE ROI windows and applies them |

**`Update()` processing:**

1. Scales face coordinates from model resolution to sensor resolution
2. Sets up to 8 ROI windows
3. Calculates each ROI weight by area ratio (larger face = higher weight)
4. Applies to ISP via `kd_mpi_isp_ae_set_roi()`

[far-class]: https://github.com/owhinata/canmv-k230/blob/4f9b08c/apps/sample_face_ae/src/face_ae_roi.h#L9-L21

## Build Steps

### 1. Configure

```bash
cmake -B build/sample_face_ae -S apps/sample_face_ae \
  -DCMAKE_TOOLCHAIN_FILE="$(pwd)/cmake/toolchain-k230-rtsmart.cmake"
```

### 2. Build

```bash
cmake --build build/sample_face_ae
```

### 3. Verify

```bash
file build/sample_face_ae/sample_face_ae
```

Expected output:

```
sample_face_ae: ELF 64-bit LSB executable, UCB RISC-V, RVC, double-float ABI, version 1 (SYSV), statically linked, ...
```

## CMakeLists.txt Details

The `apps/sample_face_ae/CMakeLists.txt` handles:

- **MPP include paths**: Headers from `mpp/include/`, `mpp/include/comm/`, `mpp/include/ioctl/`, and `mpp/userapps/api/`
- **NNCASE include paths**: Headers from `nncase/include/`, `nncase/include/nncase/runtime/`, and `rvvlib/include/`
- **MPP static libraries**: All MPP libraries linked with `--start-group` / `--end-group` to resolve circular dependencies
- **NNCASE libraries**: `Nncase.Runtime.Native`, `nncase.rt_modules.k230`, `functional_k230`, `rvv`
- **C++20**: Requires C++20 via `target_compile_features`

## Command-Line Arguments

```
./sample_face_ae <kmodel> <roi_enable>
```

| Argument | Description |
|----------|-------------|
| `<kmodel>` | Path to the face detection kmodel file (e.g., `/sharefs/mobile_retinaface.kmodel`) |
| `<roi_enable>` | Enable AE ROI: `1` = enabled, `0` = disabled |

## Transferring and Running on K230

### Transfer via SCP

```bash
scp build/sample_face_ae/sample_face_ae root@<K230_IP_ADDRESS>:/sharefs/sample_face_ae
scp mobile_retinaface.kmodel root@<K230_IP_ADDRESS>:/sharefs/
```

### Run on the K230 bigcore (msh)

On the K230 serial console (ACM1), run:

```
msh /> /sharefs/sample_face_ae /sharefs/mobile_retinaface.kmodel 1
```

To run with AE ROI disabled:

```
msh /> /sharefs/sample_face_ae /sharefs/mobile_retinaface.kmodel 0
```

!!! tip "Serial connection"
    - **Bigcore (RT-Smart msh)**: `/dev/ttyACM1` at 115200 bps

    ```bash
    minicom -D /dev/ttyACM1 -b 115200
    ```
