/*
 * Copyright (c) 2024 The Core team
 *
 * Licensed under the Apache License, Version 2.0;
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an 'AS IS' BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <arpa/inet.h>

#include <torch/csrc/distributed/c10d/ProcessGroupNCCL.hpp>
#include <torch/csrc/distributed/c10d/TCPStore.hpp>

#include "communication/communication_impl.h"
#include "logger.h"

namespace cs::communication {
namespace {
template <BackendType backend>
std::unordered_map<MPI_Comm, Comm> &getMap() {
  static std::unordered_map<MPI_Comm, Comm> map;
  return map;
}

Comm createNccl(const MPI_Comm group) {
  int rank, size;
  MPI_Comm_rank(group, &rank);
  MPI_Comm_size(group, &size);

  char processor_name[MPI_MAX_PROCESSOR_NAME];
  int name_len;
  MPI_Get_processor_name(processor_name, &name_len);

  char addr0[INET_ADDRSTRLEN];  // INET_ADDRSTRLEN is typically 16 for IPv4
  uint16_t port = 29500;        // default port

  if (rank == 0) {
    const hostent *he = gethostbyname(processor_name);
    CS_ASSERT_TRUE(he != nullptr, fmt::format("Error resolving hostname: {}",
                                              hstrerror(h_errno)));
    strcpy(addr0, inet_ntoa(*reinterpret_cast<in_addr *>(he->h_addr)));

    // Try to find a free port
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    CS_ASSERT_TRUE(sock != -1, "Socket creation failed");

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;

    while (port < 30000) {  // try ports from 29500 to 29999
      addr.sin_port = htons(port);
      if (bind(sock, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0) {
        // Successfully bound to the port
        break;
      }
      port++;
    }

    close(sock);
    CS_ASSERT_TRUE(port < 30000, "Could not find a free port");
  }

  // Broadcast the IP address and port from rank 0 to all other ranks
  MPI_Bcast(addr0, INET_ADDRSTRLEN, MPI_CHAR, 0, group);
  MPI_Bcast(&port, 1, MPI_UINT16_T, 0, group);

  auto store = c10::make_intrusive<c10d::TCPStore>(
      addr0, c10d::TCPStoreOptions{
                 .port = port, .isServer = rank == 0, .numWorkers = size});
  auto backend = c10::make_intrusive<c10d::ProcessGroupNCCL>(store, rank, size);
  return Comm{std::make_shared<Comm::Impl>(group, std::move(store),
                                           std::move(backend))};
}

Comm lookupMapOrCreate(const MPI_Comm group, const BackendType backendType) {
  static std::mutex mutex;
  std::lock_guard guard{mutex};
  switch (backendType) {
    case kNCCL: {
      auto &map = getMap<kNCCL>();
      if (const auto find = map.find(group); find == map.end()) {
        auto comm = createNccl(group);
        map.insert({group, comm});
        return comm;
      } else {
        return find->second;
      }
    }
    default: {
      CS_ASSERT_TRUE(false, "we only support NCCL now");
    }
  }
}
}  // namespace

Comm::Comm(std::shared_ptr<Impl> impl) : impl_{std::move(impl)} {}

const std::shared_ptr<Comm::Impl> &Comm::impl() const { return impl_; }

int64_t Comm::get_rank() const { return impl_->backend()->getRank(); }

int64_t Comm::get_size() const { return impl_->backend()->getSize(); }

void Bucket::apply(const Scheduler &scheduler, const Comm &comm) const {
  impl_->apply(scheduler, comm);
}
const std::shared_ptr<Bucket::Impl> &Bucket::impl() const { return impl_; }

Comm get_comm(const MPI_Comm group, const BackendType backendType) {
  return lookupMapOrCreate(group, backendType);
}

Comm get_comm_world(const BackendType backendType) {
  return get_comm(MPI_COMM_WORLD, backendType);
}

Comm get_comm_node(const BackendType backendType) {
  static struct MPICommGuard {
    MPI_Comm comm;
    MPICommGuard() {
      int world_rank;
      CS_CHECK_MPI(MPI_Comm_rank(MPI_COMM_WORLD, &world_rank));
      CS_CHECK_MPI(MPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, 0,
                                       MPI_INFO_NULL, &comm));
    }
    ~MPICommGuard() { CS_CHECK_MPI(MPI_Comm_free(&comm)); }
  } guard;
  return get_comm(guard.comm, backendType);
}

Comm::Impl::Impl(const MPI_Comm group, c10::intrusive_ptr<c10d::Store> store,
                 c10::intrusive_ptr<c10d::Backend> backend)
    : group_{group}, store_{std::move(store)}, backend_{std::move(backend)} {}

const c10::intrusive_ptr<c10d::Store> &Comm::Impl::store() const {
  return store_;
}

const c10::intrusive_ptr<c10d::Backend> &Comm::Impl::backend() const {
  return backend_;
}
}  // namespace cs::communication
