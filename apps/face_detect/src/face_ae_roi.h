#pragma once

#include <vector>

#include "k_isp_comm.h"
#include "mpi_isp_api.h"
#include "util.h"

class FaceAeRoi {
 public:
  FaceAeRoi(k_isp_dev dev, k_u32 model_w, k_u32 model_h, k_u32 sensor_w,
            k_u32 sensor_h);

  void SetEnable(bool enable);
  void Update(const std::vector<face_coordinate>& boxes);

 private:
  k_isp_dev dev_;
  k_u32 model_w_, model_h_;
  k_u32 sensor_w_, sensor_h_;
};
