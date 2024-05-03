#include "compute/init.h"

#include "logger.h"

namespace dllm::compute::Init {
void kaimingNormKernel(cudaStream_t cudaStream, Tensor2D &y, double stddev);

Task kaimingNorm(const std::shared_ptr<Tensor2D> &y) {
  double stddev = std::sqrt(2.0f / cute::shape<1>(y->layout));
  return Task{[=](const Context *context) {
    kaimingNormKernel(context->cudaStream, *y, stddev);
    CHECK_CUDART(cudaStreamSynchronize(context->cudaStream));
  }};
}
}  // namespace dllm::compute::Init
