#include "mobile_retinaface.h"

#include <chrono>
#include <fstream>
#include <iostream>

#include "util.h"

using namespace nncase;
using namespace nncase::runtime;
using namespace nncase::runtime::k230;

#define MIN_SIZE 200
#define LOC_SIZE 4
#define CONF_SIZE 2
#define LAND_SIZE 10

extern float anchors320[4200][4];
typedef int (*__compar_fn_t)(__const void*, __const void*);
static float umeyama_args[] = {76.5892,  103.3926, 147.0636, 103.0028,
                               112.0504, 143.4732, 83.0986,  184.731,
                               141.4598, 184.4082};

MobileRetinaface::MobileRetinaface(const char* kmodel_file, size_t channel,
                                   size_t height, size_t width)
    : Model("MobileRetinaface", kmodel_file),
      ai2d_input_c_(channel),
      ai2d_input_h_(height),
      ai2d_input_w_(width) {
  // ai2d output tensor
  ai2d_out_tensor_ = input_tensor(0);

  // ai2d config
  dims_t in_shape{1, ai2d_input_c_, ai2d_input_h_, ai2d_input_w_};
  auto out_shape = input_shape(0);

  ai2d_datatype_t ai2d_dtype{ai2d_format::NCHW_FMT, ai2d_format::NCHW_FMT,
                             typecode_t::dt_uint8, typecode_t::dt_uint8};
  ai2d_crop_param_t crop_param{false, 0, 0, 0, 0};
  ai2d_shift_param_t shift_param{false, 0};
  float h_ratio = static_cast<float>(height) / out_shape[2];
  float w_ratio = static_cast<float>(width) / out_shape[3];
  float ratio = h_ratio > w_ratio ? h_ratio : w_ratio;
  int h_pad = out_shape[2] - height / ratio;
  int h_pad_before = h_pad / 2;
  int h_pad_after = h_pad - h_pad_before;
  int w_pad = out_shape[3] - width / ratio;
  int w_pad_before = w_pad / 2;
  int w_pad_after = w_pad - w_pad_before;

  ai2d_pad_param_t pad_param{true,
                             {{0, 0}, {0, 0}, {70, 70}, {0, 0}},
                             ai2d_pad_mode::constant,
                             {0, 0, 0}};
  ai2d_resize_param_t resize_param{true, ai2d_interp_method::tf_bilinear,
                                   ai2d_interp_mode::half_pixel};
  ai2d_affine_param_t affine_param{false};
  ai2d_builder_.reset(new ai2d_builder(in_shape, out_shape, ai2d_dtype,
                                       crop_param, shift_param, pad_param,
                                       resize_param, affine_param));
  ai2d_builder_->build_schedule();
}

MobileRetinaface::~MobileRetinaface() {}

void MobileRetinaface::preprocess(uintptr_t vaddr, uintptr_t paddr) {
#if ENABLE_PROFILING
  ScopedTiming st(model_name() + " " + __FUNCTION__);
#endif

  // ai2d input tensor
  dims_t in_shape{1, ai2d_input_c_, ai2d_input_h_, ai2d_input_w_};
  auto ai2d_in_tensor =
      host_runtime_tensor::create(typecode_t::dt_uint8, in_shape,
                                  {(gsl::byte*)vaddr, compute_size(in_shape)},
                                  false, hrt::pool_shared, paddr)
          .expect("cannot create input tensor");
  hrt::sync(ai2d_in_tensor, sync_op_t::sync_write_back, true)
      .expect("sync write_back failed");

  // run ai2d
  ai2d_builder_->invoke(ai2d_in_tensor, ai2d_out_tensor_)
      .expect("error occurred in ai2d running");
}

void MobileRetinaface::postprocess() {
#if ENABLE_PROFILING
  ScopedTiming st(model_name() + " " + __FUNCTION__);
#endif

  std::vector<box_t> pred_box;
  pred_box.reserve(16);
  std::vector<landmarks_t> landmarks;

  decode(pred_box, landmarks);

  int long_side = ai2d_input_h_ > ai2d_input_w_ ? ai2d_input_h_ : ai2d_input_w_;
  int short_side =
      ai2d_input_h_ < ai2d_input_w_ ? ai2d_input_h_ : ai2d_input_w_;
  int pad = (long_side - short_side) / 2;
  bool width_pad = long_side == ai2d_input_h_ ? true : false;

  // boxes
  result_.boxes.clear();
  result_.boxes.reserve(pred_box.size());
  for (size_t i = 0; i < pred_box.size(); i++) {
    face_coordinate box;

    if (width_pad) {
      box.x1 =
          (int)(pred_box[i].x * long_side - pred_box[i].w * long_side / 2) -
          pad;
      box.y1 = (int)(pred_box[i].y * long_side - pred_box[i].h * long_side / 2);
      box.x2 =
          (int)(pred_box[i].x * long_side + pred_box[i].w * long_side / 2) -
          pad;
      box.y2 = (int)(pred_box[i].y * long_side + pred_box[i].h * long_side / 2);
    } else {
      box.x1 = (int)(pred_box[i].x * long_side - pred_box[i].w * long_side / 2);
      box.y1 =
          (int)(pred_box[i].y * long_side - pred_box[i].h * long_side / 2) -
          pad;
      box.x2 = (int)(pred_box[i].x * long_side + pred_box[i].w * long_side / 2);
      box.y2 =
          (int)(pred_box[i].y * long_side + pred_box[i].h * long_side / 2) -
          pad;
    }

    box.x1 = box.x1 < 0 ? 1 : box.x1;
    box.y1 = box.y1 < 0 ? 1 : box.y1;
    box.x2 = box.x2 > ai2d_input_w_ ? ai2d_input_w_ : box.x2;
    box.y2 = box.y2 > ai2d_input_h_ ? ai2d_input_h_ : box.y2;

    result_.boxes.push_back(box);
  }

  // landmarks
  result_.landmarks.clear();
  for (size_t i = 0; i < landmarks.size(); i++) {
    auto landmark = landmarks[i];
    int x = 0, y = 0;
    for (uint32_t j = 0; j < 5; j++) {
      if (width_pad) {
        x = (int)(landmark.points[2 * j + 0] * long_side) - pad;
        y = (int)(landmark.points[2 * j + 1] * long_side);
      } else {
        x = (int)(landmark.points[2 * j + 0] * long_side);
        y = (int)(landmark.points[2 * j + 1] * long_side) - pad;
      }

      landmark.points[2 * j + 0] = x;
      landmark.points[2 * j + 1] = y;
    }
    result_.landmarks.push_back(landmark);
  }
}

float MobileRetinaface::overlap(float x1, float w1, float x2, float w2) {
  float l1 = x1 - w1 / 2;
  float l2 = x2 - w2 / 2;
  float left = l1 > l2 ? l1 : l2;
  float r1 = x1 + w1 / 2;
  float r2 = x2 + w2 / 2;
  float right = r1 < r2 ? r1 : r2;

  return right - left;
}

float MobileRetinaface::box_intersection(box_t a, box_t b) {
  float w = overlap(a.x, a.w, b.x, b.w);
  float h = overlap(a.y, a.h, b.y, b.h);

  if (w < 0 || h < 0) return 0;
  return w * h;
}

float MobileRetinaface::box_union(box_t a, box_t b) {
  float i = box_intersection(a, b);
  float u = a.w * a.h + b.w * b.h - i;

  return u;
}

float MobileRetinaface::box_iou(box_t a, box_t b) {
  return box_intersection(a, b) / box_union(a, b);
}

void MobileRetinaface::deal_conf_opt(float* conf, float* s_probs, int* s,
                                     int size, int* obj_cnt, int* real_count,
                                     float* tmp) {
  softmax_2group_vec(size, conf, conf + size, tmp, tmp + size);  //
  softmax_2group_vec(size, conf + 2 * size, conf + 3 * size, tmp + 2 * size,
                     tmp + 3 * size);
  register int cnt = *obj_cnt;
  register int index_s = *real_count;

  for (int i = 0; i < size; ++i) {
    register float soft_vlaue = tmp[size + i];
    if (soft_vlaue >= obj_threshold_) {
      s[index_s] = cnt;
      s_probs[index_s] = soft_vlaue;
      ++index_s;
    }
    cnt += 1;
    soft_vlaue = tmp[size * 3 + i];
    if (soft_vlaue >= obj_threshold_) {
      s[index_s] = cnt;
      s_probs[index_s] = soft_vlaue;
      ++index_s;
    }
    cnt += 1;
  }

  *obj_cnt = cnt;
  *real_count = index_s;
}

void MobileRetinaface::deal_loc_opt(float* loc, float* boxes, int size,
                                    int* obj_cnt, int* s, int* real_count) {
  register int cnt = *obj_cnt;
  register int index_s = *real_count;
  for (uint32_t ww = 0; ww < size; ww++) {
    for (uint32_t hh = 0; hh < 2; hh++) {
      if (cnt == s[index_s]) {
        for (uint32_t cc = 0; cc < LOC_SIZE; cc++) {
          boxes[index_s * LOC_SIZE + cc] =
              loc[(hh * LOC_SIZE + cc) * size + ww];
        }
        index_s += 1;
      }
      cnt += 1;
    }
  }
  *obj_cnt = cnt;
  *real_count = index_s;
}

void MobileRetinaface::deal_landms_opt(float* landms, float* landmarks,
                                       int size, int* obj_cnt, int* s,
                                       int* real_count) {
  register int cnt = *obj_cnt;
  register int index_s = *real_count;
  for (uint32_t ww = 0; ww < size; ww++) {
    for (uint32_t hh = 0; hh < 2; hh++) {
      if (cnt == s[index_s]) {
        for (uint32_t cc = 0; cc < LAND_SIZE; cc++) {
          landmarks[index_s * LAND_SIZE + cc] =
              landms[(hh * LAND_SIZE + cc) * size + ww];
        }
        index_s += 1;
      }
      cnt += 1;
    }
  }
  *obj_cnt = cnt;
  *real_count = index_s;
}

box_t MobileRetinaface::get_box_opt(float* boxes, int obj_index,
                                    int index_anchors) {
  float x, y, w, h;
  x = boxes[obj_index * LOC_SIZE + 0];
  y = boxes[obj_index * LOC_SIZE + 1];
  w = boxes[obj_index * LOC_SIZE + 2];
  h = boxes[obj_index * LOC_SIZE + 3];
  x = anchors320[index_anchors][0] + x * 0.1 * anchors320[index_anchors][2];
  y = anchors320[index_anchors][1] + y * 0.1 * anchors320[index_anchors][3];
  w = anchors320[index_anchors][2] * k230_expf(w * 0.2);
  h = anchors320[index_anchors][3] * k230_expf(h * 0.2);
  box_t box;
  box.x = x;
  box.y = y;
  box.w = w;
  box.h = h;
  return box;
}

landmarks_t MobileRetinaface::get_landmark_opt(float* landmarks, int obj_index,
                                               int index_anchors) {
  landmarks_t landmark;
  for (uint32_t ll = 0; ll < 5; ll++) {
    landmark.points[2 * ll + 0] =
        anchors320[index_anchors][0] +
        landmarks[obj_index * LAND_SIZE + 2 * ll + 0] * 0.1 *
            anchors320[index_anchors][2];
    landmark.points[2 * ll + 1] =
        anchors320[index_anchors][1] +
        landmarks[obj_index * LAND_SIZE + 2 * ll + 1] * 0.1 *
            anchors320[index_anchors][3];
  }
  return landmark;
}

static float* cmp_prob;
int nms_comparator2(void* pa, void* pb) {
  int a = *(int*)pa;
  int b = *(int*)pb;

  if (cmp_prob[a] < cmp_prob[b])
    return 1;
  else if (cmp_prob[a] > cmp_prob[b])
    return -1;
  else
    return 0;
}

void MobileRetinaface::decode(std::vector<box_t>& pred_box,
                              std::vector<landmarks_t>& pred_landmarks) {
  const size_t size = 9;
  float* out[size];
  for (size_t i = 0; i < size; i++) {
    auto tensor = output_tensor(i);
    auto buf = tensor.impl()
                   ->to_host()
                   .unwrap()
                   ->buffer()
                   .as_host()
                   .unwrap()
                   .map(map_access_::map_read)
                   .unwrap()
                   .buffer();
    out[i] = reinterpret_cast<float*>(buf.data());
  }

  float* loc0 = out[0];
  float* loc1 = out[1];
  float* loc2 = out[2];
  float* conf0 = out[3];
  float* conf1 = out[4];
  float* conf2 = out[5];
  float* landms0 = out[6];
  float* landms1 = out[7];
  float* landms2 = out[8];

  int objs_num = MIN_SIZE * (1 + 4 + 16);
  int* s = (int*)malloc(objs_num * sizeof(int));
  float* s_probs = (float*)malloc(objs_num * sizeof(float));
  cmp_prob = s_probs;
  int obj_cnt = 0;
  int real_count = 0;

  float* tmp =
      (float*)malloc(16 * MIN_SIZE / 2 * sizeof(float) * CONF_SIZE * 2);

  deal_conf_opt(conf0, s_probs, s, 16 * MIN_SIZE / 2, &obj_cnt, &real_count,
                tmp);
  deal_conf_opt(conf1, s_probs, s, 4 * MIN_SIZE / 2, &obj_cnt, &real_count,
                tmp);
  deal_conf_opt(conf2, s_probs, s, 1 * MIN_SIZE / 2, &obj_cnt, &real_count,
                tmp);

  float* boxes = (float*)malloc(objs_num * LOC_SIZE * sizeof(float));
  obj_cnt = 0;
  int real_count_box = 0;

  deal_loc_opt(loc0, boxes, 16 * MIN_SIZE / 2, &obj_cnt, s, &real_count_box);
  deal_loc_opt(loc1, boxes, 4 * MIN_SIZE / 2, &obj_cnt, s, &real_count_box);
  deal_loc_opt(loc2, boxes, 1 * MIN_SIZE / 2, &obj_cnt, s, &real_count_box);

  float* landmarks = (float*)malloc(objs_num * LAND_SIZE * sizeof(float));
  obj_cnt = 0;
  int real_count_landms = 0;
  deal_landms_opt(landms0, landmarks, 16 * MIN_SIZE / 2, &obj_cnt, s,
                  &real_count_landms);
  deal_landms_opt(landms1, landmarks, 4 * MIN_SIZE / 2, &obj_cnt, s,
                  &real_count_landms);
  deal_landms_opt(landms2, landmarks, 1 * MIN_SIZE / 2, &obj_cnt, s,
                  &real_count_landms);

  objs_num = real_count;
  int* s_int = (int*)malloc(objs_num * sizeof(int));
  for (int i = 0; i < objs_num; ++i) {
    s_int[i] = i;
  }
  qsort(s_int, objs_num, sizeof(int), (__compar_fn_t)nms_comparator2);

  for (int i = 0; i < objs_num; ++i) {
    int obj_index = s_int[i];
    if (s_probs[obj_index] < obj_threshold_) continue;
    box_t a = get_box_opt(boxes, obj_index, s[obj_index]);
    pred_box.push_back(a);

    landmarks_t l = get_landmark_opt(landmarks, obj_index, s[obj_index]);
    pred_landmarks.push_back(l);
    for (int j = i + 1; j < objs_num; ++j) {
      obj_index = s_int[j];
      if (s_probs[obj_index] < obj_threshold_) continue;
      box_t b = get_box_opt(boxes, obj_index, s[obj_index]);
      if (box_iou(a, b) >= nms_threshold_) s_probs[obj_index] = 0;
    }
  }

  free(s_int);
  free(landmarks);
  free(boxes);
  free(tmp);
  free(s_probs);
  free(s);
}
