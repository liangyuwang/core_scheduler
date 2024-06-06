#include "threading/thread_pool_cudart.h"

#include <cuda_runtime.h>
#include <pthread.h>
#include <sched.h>

#include "logger.h"

namespace dllm {
namespace {
void setThreadAffinity(std::jthread &th, const int coreId) {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(coreId, &cpuset);

  int rc =
      pthread_setaffinity_np(th.native_handle(), sizeof(cpu_set_t), &cpuset);
  DLLM_ASSERT_TRUE(rc == 0, "core binding error with code {}", rc);
}

void threadTask(const int localRank, std::queue<TaskCudart> *taskQueue,
                std::mutex *queueMutex, std::condition_variable *cv,
                std::mutex *cvMutex, const std::atomic<bool> *shutDown,
                std::latch *barrier) {
  ContextCudart context{.deviceRank = localRank};
  barrier->arrive_and_wait();
  CHECK_CUDART(cudaSetDevice(localRank));
  CHECK_CUDART(
      cudaStreamCreateWithFlags(&context.cudaStream, cudaStreamNonBlocking));
  while (!shutDown->load()) {
    TaskCudart task;
    std::unique_lock lock{*queueMutex};
    if (!taskQueue->empty()) {
      task = std::move(taskQueue->front());
      taskQueue->pop();
    }
    lock.unlock();
    if (task.valid()) {
      try {
        task(&context);
      } catch (const std::exception &e) {
        DLLM_ASSERT_TRUE(false, "Task failed with error: {}", e.what());
      }
      task = {};
    } else {
      std::unique_lock<std::mutex> uniqueLock{*cvMutex};
      cv->wait(uniqueLock,
               [&] { return shutDown->load() || !taskQueue->empty(); });
    }
  }
  std::unique_lock lock{*queueMutex};
  *taskQueue = {};
  lock.unlock();
  CHECK_CUDART(cudaStreamDestroy(context.cudaStream));
}
}  // namespace

ThreadPoolCudart::~ThreadPoolCudart() {
  shutDown = true;
  cv.notify_all();
  for (auto &t : threadVector) {
    while (!t.joinable()) {
      cv.notify_all();
    }
    t.join();
  }
}

void ThreadPoolCudart::submit(TaskCudart &&task) {
  std::unique_lock lock{queueMutex};
  taskQueue.push(std::move(task));
  lock.unlock();
  cv.notify_one();
}

ThreadPoolCudart::ThreadPoolCudart(const int localRank, const int threadNum,
                                   const std::vector<int> &bindingMap)
    : latch_{threadNum + 1} {
  DLLM_ASSERT_TRUE(threadNum > 0, "Wrong thread num");
  DLLM_ASSERT_TRUE(bindingMap.empty() ||
                       bindingMap.size() == static_cast<std::size_t>(threadNum),
                   "bindingMap size mismathches with threadNum");
  threadVector.reserve(threadNum);
  for (int i = 0; i < threadNum; ++i) {
    threadVector.emplace_back(threadTask, localRank, &taskQueue, &queueMutex,
                              &cv, &cvMutex, &shutDown, &latch_);
  }
  if (!bindingMap.empty()) {
    for (int i = 0; i < threadNum; ++i) {
      setThreadAffinity(threadVector[i], bindingMap[i]);
    }
  }
  latch_.arrive_and_wait();
}
}  // namespace dllm