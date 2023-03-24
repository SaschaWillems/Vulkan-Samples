/* Copyright (c) 2023, Sascha Willems
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 the "License";
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "platform/plugins/plugin_base.h"

namespace plugins
{
class ShaderOptions;

using ShaderOptionsTags = vkb::PluginBase<ShaderOptions, vkb::tags::Passive>;

/**
 * @brief Shader options
 * 
 * Configure the shader type to use (GLSL or HLSL)
 * 
 * Usage: vulkan_samples sample instancing --shadertype hlsl
 * 
 */
class ShaderOptions : public ShaderOptionsTags
{
  public:
	ShaderOptions();

	virtual ~ShaderOptions() = default;

	virtual bool is_active(const vkb::CommandParser &parser) override;

	virtual void init(const vkb::CommandParser &options) override;

	vkb::FlagCommand shader_type_flag      = {vkb::FlagType::OneValue, "shadertype", "", "Shader type to use (hlsl or glsl)"};

	vkb::CommandGroup shader_options_group = {"Shader Options", {&shader_type_flag}};
};
}        // namespace plugins