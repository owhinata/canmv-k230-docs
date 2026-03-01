#include "face_ae_roi.h"

#include <algorithm>
#include <cstring>

FaceAeRoi::FaceAeRoi(k_isp_dev dev, k_u32 model_w, k_u32 model_h,
                     k_u32 sensor_w, k_u32 sensor_h)
    : dev_(dev),
      model_w_(model_w),
      model_h_(model_h),
      sensor_w_(sensor_w),
      sensor_h_(sensor_h) {}

void FaceAeRoi::SetEnable(bool enable) {
  kd_mpi_isp_ae_roi_set_enable(dev_, enable ? K_TRUE : K_FALSE);
}

void FaceAeRoi::Update(const std::vector<face_coordinate>& boxes) {
  k_isp_ae_roi ae_roi;
  memset(&ae_roi, 0, sizeof(ae_roi));

  auto box_count = std::min<int>(boxes.size(), 8);
  if (box_count == 0) {
    ae_roi.roiNum = 0;
    kd_mpi_isp_ae_set_roi(dev_, ae_roi);
    return;
  }

  ae_roi.roiNum = box_count;
  k_u32 area[8] = {0};

  for (int i = 0; i < box_count; i++) {
    k_u32 x1 = std::max(boxes[i].x1, 0);
    k_u32 y1 = std::max(boxes[i].y1, 0);

    k_u32 h_offset = x1 * sensor_w_ / model_w_;
    k_u32 v_offset = y1 * sensor_h_ / model_h_;
    k_u32 w = (static_cast<k_u32>(boxes[i].x2) - x1) * sensor_w_ / model_w_;
    k_u32 h = (static_cast<k_u32>(boxes[i].y2) - y1) * sensor_h_ / model_h_;

    w = std::min(w, sensor_w_ - h_offset);
    h = std::min(h, sensor_h_ - v_offset);

    ae_roi.roiWindow[i].window.hOffset = h_offset;
    ae_roi.roiWindow[i].window.vOffset = v_offset;
    ae_roi.roiWindow[i].window.width = w;
    ae_roi.roiWindow[i].window.height = h;

    area[i] = w * h;
  }

  k_u32 sum = 0;
  for (int i = 0; i < box_count; i++) {
    sum += area[i];
  }
  for (int i = 0; i < box_count; i++) {
    ae_roi.roiWindow[i].weight = static_cast<float>(area[i]) / sum;
  }

  kd_mpi_isp_ae_set_roi(dev_, ae_roi);
}
