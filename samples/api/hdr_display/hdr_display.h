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

#include "api_vulkan_sample.h"

class hdr_display : public ApiVulkanSample
{
  public:
	hdr_display();
	virtual ~hdr_display();

	void prepare_pipelines();

	// Override basic framework functionality
	void build_command_buffers() override;
	void render(float delta_time) override;
	bool prepare(const vkb::ApplicationOptions &options) override;
	virtual void on_update_ui_overlay(vkb::Drawer &drawer) override;

  private:

	struct Vertex
	{
		float pos[2];
		float color[3];
	};

	// HDR display settings applied to the swapchain
	float min_luminance{0.01f};
	float max_luminance{1000.0f};
	float max_content_light_level{2000.0f};
	float max_frame_average_light_level{500.0f};

	bool update_hdr_metadata{false};
	bool hdr_preset{0};

	std::unique_ptr<vkb::core::Buffer> gradients_vertex_buffer;
	uint32_t                           gradient_count{0};

	std::unique_ptr<vkb::core::Buffer> triangles_vertex_buffer;
	uint32_t                           triangles_vertex_count{0};

	VkPipeline       gradients_pipeline{};
	VkPipeline       triangles_pipeline{};
	VkPipelineLayout sample_pipeline_layout{};

	void generate_triangle();
};

std::unique_ptr<vkb::VulkanSample> create_hdr_display();
