#ifndef _MODEL_H
#define _MODEL_H

#include <nncase/functional/ai2d/ai2d_builder.h>
#include <nncase/runtime/interpreter.h>
#include <nncase/runtime/runtime_op_utility.h>

#include "util.h"

namespace nr = nncase::runtime;
namespace nfk = nncase::F::k230;

class Model {
 public:
  Model(const char *model_name, const char *kmodel_file);
  ~Model();
  void Run(uintptr_t vaddr, uintptr_t paddr);
  std::string ModelName() const;

 protected:
  virtual void Preprocess(uintptr_t vaddr, uintptr_t paddr) = 0;
  void KpuRun();
  virtual void Postprocess() = 0;
  nr::runtime_tensor InputTensor(size_t idx);
  void InputTensor(size_t idx, nr::runtime_tensor &tensor);
  nr::runtime_tensor OutputTensor(size_t idx);
  nncase::dims_t InputShape(size_t idx);
  nncase::dims_t OutputShape(size_t idx);

 protected:
  std::unique_ptr<nfk::ai2d_builder> ai2d_builder_;
  nr::runtime_tensor ai2d_in_tensor_;
  nr::runtime_tensor ai2d_out_tensor_;

 private:
  nr::interpreter interp_;
  std::string model_name_;
  std::vector<uint8_t> kmodel_;
};
#endif
