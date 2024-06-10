#include "threading/dynamic_scheduler.h"

#include <c10/cuda/CUDAGuard.h>
#include <tensor_impl.h>
#include <threading/event.h>
#include <threading/event_impl.h>
#include <threading/task_impl.h>

#include <barrier>
#include <latch>
#include <queue>
#include <unordered_set>

#include "logger.h"
#include "nvtx_helper.h"
#include "threading/scheduler_impl.h"

namespace dllm {
namespace {
struct Impl_ final : Scheduler::Impl {
  explicit Impl_(int localRank);

  ~Impl_() override;

  void submit(Task &&task) override;

  void submit(const Task &task) = delete;

 private:
  int8_t threadCommIdx_;
  std::vector<std::shared_ptr<std::barrier<>>> startBarrier_{};
  std::vector<std::shared_ptr<std::barrier<>>> endBarrier_{};
  std::vector<std::jthread> threadVector_{};
  std::vector<std::shared_ptr<std::queue<Task>>> taskQueue_{};
  std::vector<std::vector<void *>> lastOutput_;
  std::shared_ptr<std::atomic<bool>> shutDown_{
      std::make_shared<std::atomic<bool>>(false)};
};

void threadTask(const int localRank, const int8_t streamIdx,
                std::shared_ptr<std::queue<Task>> taskQueue,
                const std::shared_ptr<std::atomic<bool>> shutDown,
                const std::shared_ptr<std::barrier<>> startBarrier,
                const std::shared_ptr<std::barrier<>> endBarrier) {
  struct ContextCompute {
    int deviceRank{0};
    cudaStream_t cudaStream{nullptr};
  };
  ContextCompute context{.deviceRank = localRank};
  startBarrier->arrive_and_wait();
  const auto stream = c10::cuda::getStreamFromPool(
      false, static_cast<c10::DeviceIndex>(context.deviceRank));
  context.cudaStream = stream.stream();
  c10::cuda::CUDAStreamGuard streamGuard{stream};
  c10::cuda::CUDAGuard deviceGuard{
      static_cast<c10::DeviceIndex>(context.deviceRank)};

  while (true) {
    startBarrier->arrive_and_wait();
    if (shutDown->load()) {
      break;
    }
    auto task = std::move(taskQueue->front());
    taskQueue->pop();
    try {
      for (auto &input = task.input(); auto &t : input) {
        if (t.impl()->streamIdx() != streamIdx) {
          if (!t.impl()->event().query()) {
            t.impl()->event().block();
          }
        }
      }
      DLLM_NVTX_RANGE_FN(task.name());
      task();
      for (auto &output = task.output(); auto &t : output) {
        t.impl()->streamIdx() = streamIdx;
        t.impl()->event().record();
      }
    } catch (const std::exception &e) {
      DLLM_ASSERT_TRUE(false, fmt::format("Task {} failed with error: {}",
                                          task.name(), e.what()));
    }
    endBarrier->arrive_and_wait();
    for (auto &output = task.output(); auto &t : output) {
      t.impl()->stream() = context.cudaStream;
    }
    task.reset();
  }
}

void threadCommTask(const int localRank, const int8_t streamIdx,
                    std::shared_ptr<std::queue<Task>> taskQueue,
                    const std::shared_ptr<std::atomic<bool>> shutDown,
                    const std::shared_ptr<std::barrier<>> startBarrier,
                    const std::shared_ptr<std::barrier<>> endBarrier) {
  struct ContextCompute {
    int deviceRank{0};
    cudaStream_t cudaStream{nullptr};
  };
  ContextCompute context{.deviceRank = localRank};
  startBarrier->arrive_and_wait();
  const auto stream = c10::cuda::getStreamFromPool(
      true, static_cast<c10::DeviceIndex>(context.deviceRank));
  context.cudaStream = stream.stream();
  c10::cuda::CUDAStreamGuard streamGuard{stream};
  c10::cuda::CUDAGuard deviceGuard{
      static_cast<c10::DeviceIndex>(context.deviceRank)};

  while (true) {
    startBarrier->arrive_and_wait();
    if (shutDown->load()) {
      break;
    }
    auto task = std::move(taskQueue->front());
    taskQueue->pop();
    try {
      for (auto &input = task.input(); auto &t : input) {
        if (t.impl()->streamIdx() != streamIdx) {
          if (!t.impl()->event().query()) {
            t.impl()->event().block();
          }
        }
      }
      DLLM_NVTX_RANGE_FN(task.name());
      task();
      for (auto &output = task.output(); auto &t : output) {
        t.impl()->streamIdx() = streamIdx;
        t.impl()->event().record();
      }
    } catch (const std::exception &e) {
      DLLM_ASSERT_TRUE(false, fmt::format("Task {} failed with error: {}",
                                          task.name(), e.what()));
    }
    endBarrier->arrive_and_wait();
    for (auto &output = task.output(); auto &t : output) {
      t.impl()->stream() = context.cudaStream;
    }
    task.reset();
  }
}
}  // namespace

Impl_::Impl_(int localRank) {
  constexpr int threadNum = 3;
  taskQueue_.resize(threadNum);
  for (auto &queue : taskQueue_) {
    queue = std::make_shared<std::queue<Task>>();
  }
  lastOutput_.resize(threadNum);
  threadVector_.reserve(threadNum);
  startBarrier_.reserve(threadNum);
  endBarrier_.reserve(threadNum);
  for (int8_t i = 0; i < threadNum; ++i) {
    startBarrier_.emplace_back(std::make_shared<std::barrier<>>(2));
    endBarrier_.emplace_back(std::make_shared<std::barrier<>>(2));
    threadVector_.emplace_back(threadTask, localRank, i, taskQueue_[i],
                               shutDown_, startBarrier_[i], endBarrier_[i]);
    startBarrier_[i]->arrive_and_wait();
  }
  threadCommIdx_ = static_cast<int8_t>(threadVector_.size());
  taskQueue_.emplace_back(std::make_shared<std::queue<Task>>());
  startBarrier_.emplace_back(std::make_shared<std::barrier<>>(2));
  endBarrier_.emplace_back(std::make_shared<std::barrier<>>(2));
  lastOutput_.emplace_back();
  threadVector_.emplace_back(
      threadCommTask, localRank, threadCommIdx_, taskQueue_[threadCommIdx_],
      shutDown_, startBarrier_[threadCommIdx_], endBarrier_[threadCommIdx_]);
  startBarrier_[threadCommIdx_]->arrive_and_wait();
}

Impl_::~Impl_() {
  shutDown_->store(true);
  for (const auto &b : startBarrier_) {
    (void)b->arrive();
  }
  for (const auto &b : endBarrier_) {
    (void)b->arrive();
  }
}

void Impl_::submit(Task &&task) {
  int8_t streamIdx = -1;
  if (task.impl()->type() == Task::Impl::nccl) {
    streamIdx = threadCommIdx_;
  } else {
    [&]() mutable {
      auto &input = task.input();
      std::unordered_set<void *> elements;
      for (const auto &ptr : input) {
        elements.insert(ptr.impl().get());
      }
      for (std::size_t i = 0; i < lastOutput_.size(); ++i) {
        for (auto e : lastOutput_[i]) {
          if (elements.contains(e)) {
            streamIdx = static_cast<int8_t>(i);
            return;
          }
        }
      }
    }();
    if (streamIdx == -1 /* not found */) {
      std::vector<int> count;
      count.resize(taskQueue_.size());
      std::ranges::fill(count, 0);
      for (auto &input = task.input(); auto &t : input) {
        ++count[t.impl()->streamIdx()];
      }
      int smallestCount = task.input().size();
      streamIdx = 0;
      for (std::size_t i = 0; i < count.size(); ++i) {
        if (count[i] < smallestCount) {
          smallestCount = count[i];
          streamIdx = i;
        }
      }
    }
  }

  taskQueue_[streamIdx]->push(task);
  (void)startBarrier_[streamIdx]->arrive();
  lastOutput_[streamIdx].clear();
  for (auto &output = task.output(); auto &t : output) {
    lastOutput_[streamIdx].push_back(t.impl().get());
  }
  endBarrier_[streamIdx]->arrive_and_wait();
}

DynamicScheduler::DynamicScheduler(int localRank) {
  impl_ = std::make_shared<Impl_>(localRank);
}
}  // namespace dllm