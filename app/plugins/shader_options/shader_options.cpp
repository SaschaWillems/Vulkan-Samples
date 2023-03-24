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

#include "shader_options.h"

#include <algorithm>

#include "platform/platform.h"
#include "platform/window.h"

namespace plugins
{
ShaderOptions::ShaderOptions() :
    ShaderOptionsTags("Shader Options",
                      "Options related to shader settings",
                      {}, {&shader_options_group})
{
}

bool ShaderOptions::is_active(const vkb::CommandParser &parser)
{
	return true;
}

void ShaderOptions::init(const vkb::CommandParser &parser)
{
	if (parser.contains(&shader_type_flag))
	{
		auto shader_type = parser.as<std::string>(&shader_type_flag);
		if (shader_type == "hlsl")
		{
			platform->set_shader_type(vkb::ShaderType::HLSL);
		}
	}
}
}        // namespace plugins