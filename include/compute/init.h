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
#include <torch/nn/init.h>

#include "tensor.h"
#include "threading/scheduler.h"

namespace cs::compute::Init {
CS_API void kaiming_normal_(
    const Scheduler &scheduler, const Tensor &tensor, double a = 0,
    const torch::nn::init::FanModeType &mode = torch::kFanIn,
    const torch::nn::init::NonlinearityType &nonlinearity = torch::kLeakyReLU);

CS_API void kaiming_uniform_(
    const Scheduler &scheduler, const Tensor &tensor, double a = 0,
    const torch::nn::init::FanModeType &mode = torch::kFanIn,
    const torch::nn::init::NonlinearityType &nonlinearity = torch::kLeakyReLU);
}  // namespace cs::compute::Init
