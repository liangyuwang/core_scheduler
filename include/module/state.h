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

#pragma once
#include <torch/ordered_dict.h>

#include "tensor.h"

namespace cs {
template <typename Key, typename Value>
using OrderedDict = torch::OrderedDict<Key, Value>;
}

namespace cs::module {
struct OptimizerState {
  virtual ~OptimizerState() = default;
};

struct CS_API State {
  struct Increment {
    Tensor &parameter;
    Tensor &gradient;
    std::shared_ptr<OptimizerState> &optimizerState;
  };

  virtual ~State() = default;

  [[nodiscard]] virtual OrderedDict<std::string, Tensor> parameters() const;

  [[nodiscard]] virtual OrderedDict<std::string, Increment> increments();
};
}  // namespace cs::module
