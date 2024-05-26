#pragma once
#include "communication/all_gather.h"
#include "tensor.h"
#include "threading/task_nccl.h"

namespace dllm::communication {
template <>
struct AllGather<NCCL> {
  static TaskNccl run(const std::shared_ptr<const Tensor1D> &tensorSend,
                      const std::shared_ptr<Tensor1D> &tensorReceive);

  static TaskNccl runInplace(const std::shared_ptr<Tensor1D> &tensor);
};
}  // namespace dllm::communication