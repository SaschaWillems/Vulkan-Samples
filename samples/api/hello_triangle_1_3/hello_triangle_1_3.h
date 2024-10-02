/* Copyright (c) 2024, Sascha Willems
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

#include "common/vk_common.h"
#include "core/instance.h"
#include "platform/application.h"

/**
 * @brief A self-contained (minimal use of framework) sample that illustrates
 * the rendering of a triangle
 */
class HelloTriangleVulkan13 : public vkb::Application
{
	/**
	 * @brief Swapchain state
	 */
	struct SwapchainProperties
	{
		uint32_t width  = 0;
		uint32_t height = 0;
		VkFormat format = VK_FORMAT_UNDEFINED;
	};

	/**
	 * @brief Per-frame data
	 */
	// @todo: rename
	struct PerFrame
	{
		VkFence         queue_submit_fence          = VK_NULL_HANDLE;
		VkCommandBuffer primary_command_buffer      = VK_NULL_HANDLE;
		VkSemaphore     swapchain_acquire_semaphore = VK_NULL_HANDLE;
		VkSemaphore     swapchain_release_semaphore = VK_NULL_HANDLE;
	};

	/**
	 * @brief Vulkan objects and global state
	 */
	struct Context
	{
		VkInstance               instance              = VK_NULL_HANDLE;
		VkPhysicalDevice         gpu                   = VK_NULL_HANDLE;
		VkDevice                 device                = VK_NULL_HANDLE;
		VkQueue                  queue                 = VK_NULL_HANDLE;
		VkSwapchainKHR           swapchain             = VK_NULL_HANDLE;
		VkSurfaceKHR             surface               = VK_NULL_HANDLE;
		int32_t                  graphics_queue_index  = -1;
		std::vector<VkImageView> swapchain_image_views = {};
		VkPipeline               pipeline              = VK_NULL_HANDLE;
		VkCommandPool            command_pool          = VK_NULL_HANDLE;
		VkPipelineLayout         pipeline_layout       = VK_NULL_HANDLE;
		VkDebugReportCallbackEXT debug_callback        = VK_NULL_HANDLE;
		std::vector<VkSemaphore> recycled_semaphores   = {};
		std::vector<PerFrame>    per_frame             = {};
		SwapchainProperties      swapchain_properties  = {};
	};

  public:
	HelloTriangleVulkan13();

	virtual ~HelloTriangleVulkan13();

	virtual bool prepare(const vkb::ApplicationOptions &options) override;

	virtual void update(float delta_time) override;

	virtual bool resize(const uint32_t width, const uint32_t height) override;

	bool validate_extensions(const std::vector<const char *>          &required,
	                         const std::vector<VkExtensionProperties> &available);

	bool validate_layers(const std::vector<const char *>      &required,
	                     const std::vector<VkLayerProperties> &available);

	void init_instance(Context                         &context,
	                   const std::vector<const char *> &required_instance_extensions,
	                   const std::vector<const char *> &required_validation_layers);

	void init_device(Context                         &context,
	                 const std::vector<const char *> &required_device_extensions);

	void teardown_per_frame(Context &context, PerFrame &per_frame);

	void init_swapchain(Context &context);

	VkShaderModule load_shader_module(Context &context, const char *path, VkShaderStageFlagBits shader_stage);

	void init_pipeline(Context &context);

	VkResult acquire_next_image(Context &context, uint32_t *image);

	void render(Context &context, uint32_t swapchain_index);

  private:
	Context context;

	std::unique_ptr<vkb::Instance> vk_instance;
};

std::unique_ptr<vkb::Application> create_hello_triangle_1_3();
