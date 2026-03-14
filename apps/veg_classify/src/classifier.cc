#include "classifier.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <numeric>

using namespace nncase;
using namespace nncase::runtime;
using namespace nncase::runtime::k230;
using namespace nncase::F::k230;

Classifier::Classifier(const char *kmodel_file, const char *labels_file,
                       size_t channel, size_t height, size_t width)
    : Model("Classifier", kmodel_file),
      ai2d_input_c_(channel),
      ai2d_input_h_(height),
      ai2d_input_w_(width) {
  // Load labels
  std::ifstream ifs(labels_file);
  std::string line;
  while (std::getline(ifs, line)) {
    // Trim trailing whitespace
    while (!line.empty() &&
           (line.back() == '\n' || line.back() == '\r' || line.back() == ' ')) {
      line.pop_back();
    }
    if (!line.empty()) {
      labels_.push_back(line);
    }
  }
  printf("Loaded %zu labels\n", labels_.size());

  // AI2D output tensor = kmodel input tensor
  ai2d_out_tensor_ = InputTensor(0);

  // AI2D config: stretch resize (no padding, no aspect ratio preservation)
  dims_t in_shape{1, ai2d_input_c_, ai2d_input_h_, ai2d_input_w_};
  auto out_shape = InputShape(0);

  ai2d_datatype_t ai2d_dtype{ai2d_format::NCHW_FMT, ai2d_format::NCHW_FMT,
                             typecode_t::dt_uint8, typecode_t::dt_uint8};
  ai2d_crop_param_t crop_param{false, 0, 0, 0, 0};
  ai2d_shift_param_t shift_param{false, 0};
  ai2d_pad_param_t pad_param{false, {{0, 0}, {0, 0}, {0, 0}, {0, 0}},
                             ai2d_pad_mode::constant, {0, 0, 0}};
  ai2d_resize_param_t resize_param{true, ai2d_interp_method::tf_bilinear,
                                   ai2d_interp_mode::half_pixel};
  ai2d_affine_param_t affine_param{false};

  ai2d_builder_.reset(new ai2d_builder(in_shape, out_shape, ai2d_dtype,
                                       crop_param, shift_param, pad_param,
                                       resize_param, affine_param));
  ai2d_builder_->build_schedule();
}

Classifier::~Classifier() {}

void Classifier::Preprocess(uintptr_t vaddr, uintptr_t paddr) {
#if ENABLE_PROFILING
  ScopedTiming st(ModelName() + " " + __FUNCTION__);
#endif

  dims_t in_shape{1, ai2d_input_c_, ai2d_input_h_, ai2d_input_w_};
  auto ai2d_in_tensor =
      host_runtime_tensor::create(
          typecode_t::dt_uint8, in_shape,
          {reinterpret_cast<gsl::byte *>(vaddr), compute_size(in_shape)}, false,
          hrt::pool_shared, paddr)
          .expect("cannot create input tensor");
  hrt::sync(ai2d_in_tensor, sync_op_t::sync_write_back, true)
      .expect("sync write_back failed");

  ai2d_builder_->invoke(ai2d_in_tensor, ai2d_out_tensor_)
      .expect("error occurred in ai2d running");
}

void Classifier::Postprocess() {
#if ENABLE_PROFILING
  ScopedTiming st(ModelName() + " " + __FUNCTION__);
#endif

  auto tensor = OutputTensor(0);
  auto buf = tensor.impl()
                 ->to_host()
                 .unwrap()
                 ->buffer()
                 .as_host()
                 .unwrap()
                 .map(map_access_::map_read)
                 .unwrap()
                 .buffer();
  float *output = reinterpret_cast<float *>(buf.data());

  auto out_shape = OutputShape(0);
  int num_classes = static_cast<int>(out_shape[1]);

  // Softmax
  float max_val = *std::max_element(output, output + num_classes);
  float sum = 0.0f;
  for (int i = 0; i < num_classes; i++) {
    output[i] = std::exp(output[i] - max_val);
    sum += output[i];
  }
  for (int i = 0; i < num_classes; i++) {
    output[i] /= sum;
  }

  // Argmax
  int max_idx = static_cast<int>(
      std::max_element(output, output + num_classes) - output);

  result_.class_id = max_idx;
  result_.confidence = output[max_idx];
  result_.label =
      (max_idx < static_cast<int>(labels_.size())) ? labels_[max_idx] : "???";
}
