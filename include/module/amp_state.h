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

#include "module/state.h"
#include "tensor.h"

namespace cs::module {
struct CS_API AmpState : virtual State {
  [[nodiscard]] virtual OrderedDict<std::string, Tensor>
  parameters_high_precision() const;
};
}  // namespace cs::module
