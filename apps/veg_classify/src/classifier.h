#ifndef APPS_VEG_CLASSIFY_SRC_CLASSIFIER_H_
#define APPS_VEG_CLASSIFY_SRC_CLASSIFIER_H_

#include <string>
#include <vector>

#include "model.h"

struct ClassifyResult {
  int class_id;
  float confidence;
  std::string label;
};

class Classifier : public Model {
 public:
  Classifier(const char *kmodel_file, const char *labels_file, size_t channel,
             size_t height, size_t width);
  ~Classifier();
  ClassifyResult GetResult() const { return result_; }

 protected:
  void Preprocess(uintptr_t vaddr, uintptr_t paddr) override;
  void Postprocess() override;

 private:
  size_t ai2d_input_c_;
  size_t ai2d_input_h_;
  size_t ai2d_input_w_;
  std::vector<std::string> labels_;
  ClassifyResult result_;
};

#endif  // APPS_VEG_CLASSIFY_SRC_CLASSIFIER_H_
