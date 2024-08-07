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

#include "module/amp_embedding.h"

#include "threading/scheduler.h"

namespace cs::module {
AmpEmbeddingImpl::AmpEmbeddingImpl(const Scheduler& scheduler,
                                   const Options& options) {
  const auto state = compute::AmpEmbedding::init(scheduler, options);
  register_state("AmpEmbeddingState", state);
  state_ = state;
}
}  // namespace cs::module
