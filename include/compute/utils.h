#pragma once
#include "tensor.h"
#include "threading/task_compute.h"

namespace dllm::compute::Utils {
TaskCompute sum(const std::shared_ptr<Tensor> &output,
                const std::shared_ptr<const ReadOnlyTensor> &input,
                const IntArray &dim, bool keep_dim = false,
                c10::optional<at::ScalarType> dtype = c10::nullopt);

inline TaskCompute sum(
    const std::shared_ptr<Tensor> &output,
    const std::shared_ptr<const ReadOnlyTensor> &input, const int64_t dim,
    const bool keep_dim = false,
    const c10::optional<at::ScalarType> dtype = c10::nullopt) {
  return sum(output, input, IntArray{dim}, keep_dim, dtype);
}

TaskCompute range(const std::shared_ptr<Tensor> &tensor,
                  const at::Scalar &start, const at::Scalar &end,
                  at::TensorOptions options = {});

TaskCompute arange(const std::shared_ptr<Tensor> &tensor,
                   const at::Scalar &start, const at::Scalar &end,
                   at::TensorOptions options = {});

TaskCompute arange(const std::shared_ptr<Tensor> &tensor,
                   const at::Scalar &start, const at::Scalar &end,
                   const at::Scalar &step, at::TensorOptions options = {});

TaskCompute randint(const std::shared_ptr<Tensor> &tensor, int64_t low,
                    int64_t high, const IntArrayRef &size,
                    at::TensorOptions options = at::kLong);

TaskCompute empty(const std::shared_ptr<Tensor> &tensor,
                  const IntArrayRef &size, at::TensorOptions options = {});

TaskCompute empty_like(const std::shared_ptr<Tensor> &dst,
                       const std::shared_ptr<const ReadOnlyTensor> &src);

TaskCompute ones(const std::shared_ptr<Tensor> &tensor, const IntArrayRef &size,
                 at::TensorOptions options = {});

TaskCompute ones_like(const std::shared_ptr<Tensor> &dst,
                      const std::shared_ptr<const ReadOnlyTensor> &src);

TaskCompute zeros(const std::shared_ptr<Tensor> &tensor,
                  const IntArrayRef &size, at::TensorOptions options = {});

TaskCompute zeros_like(const std::shared_ptr<Tensor> &dst,
                       const std::shared_ptr<const ReadOnlyTensor> &src);

TaskCompute rand(const std::shared_ptr<Tensor> &tensor, const IntArrayRef &size,
                 at::TensorOptions options = {});

TaskCompute rand_like(const std::shared_ptr<Tensor> &dst,
                      const std::shared_ptr<const ReadOnlyTensor> &src);

TaskCompute randn(const std::shared_ptr<Tensor> &tensor,
                  const IntArrayRef &size, at::TensorOptions options = {});

TaskCompute randn_like(const std::shared_ptr<Tensor> &dst,
                       const std::shared_ptr<const ReadOnlyTensor> &src);

// split output tensors should be read-only!
TaskCompute split(std::vector<std::shared_ptr<const ReadOnlyTensor>> &output,
                  const std::shared_ptr<const ReadOnlyTensor> &src,
                  const int64_t &split_size, const int64_t &dim);

TaskCompute view(const std::shared_ptr<Tensor> &output,
                 const std::shared_ptr<const ReadOnlyTensor> &input,
                 const IntArrayRef &size);

TaskCompute broadcast_to(const std::shared_ptr<Tensor> &output,
                         const std::shared_ptr<const ReadOnlyTensor> &input,
                         const IntArrayRef &size);

TaskCompute cat(const std::shared_ptr<Tensor> &output,
                const std::vector<std::shared_ptr<const ReadOnlyTensor>> &input,
                int64_t dim);
}  // namespace dllm::compute::Utils