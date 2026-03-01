#ifndef _MOBILE_RETINAFACE_H
#define _MOBILE_RETINAFACE_H
#include "k230_math.h"
#include "model.h"
#include "rvv_math.h"
#include "util.h"

typedef struct {
  std::vector<face_coordinate> boxes;
  std::vector<landmarks_t> landmarks;
} DetectResult;

class MobileRetinaface : public Model {
 public:
  MobileRetinaface(const char *kmodel_file, size_t channel, size_t height,
                   size_t width);
  ~MobileRetinaface();
  DetectResult GetResult() const { return result_; }

 protected:
  void Preprocess(uintptr_t vaddr, uintptr_t paddr);
  void Postprocess();

 private:
  void Decode(std::vector<box_t> &pred_box,
              std::vector<landmarks_t> &pred_landmarks);
  float Overlap(float x1, float w1, float x2, float w2);
  float BoxIntersection(box_t a, box_t b);
  float BoxUnion(box_t a, box_t b);
  float BoxIou(box_t a, box_t b);
  void DealConfOpt(float *conf, float *s_probs, int *s, int size, int *obj_cnt,
                   int *real_count, float *tmp);
  void DealLocOpt(float *loc, float *boxes, int size, int *obj_cnt, int *s,
                  int *real_count);
  void DealLandmsOpt(float *landms, float *landmarks, int size, int *obj_cnt,
                     int *s, int *real_count);
  box_t GetBoxOpt(float *boxes, int obj_index, int index_anchors);
  landmarks_t GetLandmarkOpt(float *landmarks, int obj_index,
                             int index_anchors);

 private:
  size_t ai2d_input_c_;
  size_t ai2d_input_h_;
  size_t ai2d_input_w_;
  float obj_threshold_ = 0.6f;
  float nms_threshold_ = 0.5f;
  DetectResult result_;
};

#endif
