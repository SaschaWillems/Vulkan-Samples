/* Copyright (c) 2023, ASascha Willems
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

#include "hdr_display.h"

hdr_display::hdr_display()
{
	// See framework\core\swapchain.cpp

	// Since we want to use the HDR capabilities of the display, we use different swapchain formats and color spaces than the other samples
	// We try to select the first format from one of the two available HDR color spaces in Vulkan
	swapchain_surface_priority_list = {
	    // HDR10 (BT2020 color) space SMPTE ST2084 Perceptual Quantizer (PQ) EOTF
	    {VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_COLOR_SPACE_HDR10_ST2084_EXT},
	    {VK_FORMAT_R16G16B16A16_SFLOAT, VK_COLOR_SPACE_HDR10_ST2084_EXT},
	    // HDR10 (BT2020 color) space Hybrid Log Gamma (HLG) EOTF
	    {VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_COLOR_SPACE_HDR10_HLG_EXT},
	    {VK_FORMAT_R16G16B16A16_SFLOAT, VK_COLOR_SPACE_HDR10_HLG_EXT},
	};

	// @todo: check for support for thisT
	add_instance_extension(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME);
	add_instance_extension(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
	add_device_extension(VK_EXT_HDR_METADATA_EXTENSION_NAME);
}

hdr_display::~hdr_display()
{
	if (device)
	{
		vkDestroyPipeline(get_device().get_handle(), triangles_pipeline, nullptr);
		vkDestroyPipeline(get_device().get_handle(), gradients_pipeline, nullptr);
		vkDestroyPipelineLayout(get_device().get_handle(), sample_pipeline_layout, nullptr);
	}
}

bool hdr_display::prepare(const vkb::ApplicationOptions &options)
{
	if (!ApiVulkanSample::prepare(options))
	{
		return false;
	}

	prepare_pipelines();
	generate_triangle();
	build_command_buffers();
	prepared = true;
	return true;
}

// @todo: rename (scene display)?
void hdr_display::generate_triangle()
{
	// Setup vertices for a single color coded triangle
	std::vector<Vertex> vertices =
	    {
	        {{0.5, -0.5}, {1.0, 0.0, 0.0}},
	        {{0.5, 0.5}, {0.0, 1.0, 0.0}},
	        {{-0.5, 0.5}, {0.0, 0.0, 1.0}}};

	// Rec 709
	vertices = {
	    {{0.503125012f, -0.0599999987f}, {0.150000006f, 0.0599999987f}},
	    {{0.587499976f, -0.600000024f}, {0.300000012f, 0.600000024f}},
	    {{0.594643712f, -0.328999996f}, {0.312700003f, 0.328999996f}},
	    {{0.587499976f, -0.600000024f}, {0.300000012f, 0.600000024f}},
	    {{0.778749943f, -0.330000013f}, {0.639999986f, 0.330000013f}},
	    {{0.594643712f, -0.328999996f}, {0.312700003f, 0.328999996f}},
	    {{0.778749943f, -0.330000013f}, {0.639999986f, 0.330000013f}},
	    {{0.503125012f, -0.0599999987f}, {0.150000006f, 0.0599999987f}},
	    {{0.594643712f, -0.328999996f}, {0.312700003f, 0.328999996f}},
	    //};

	    // Rec 2020
	    // vertices = {
	    {{0.492437482f, 0.953999996f}, {0.130999997f, 0.0460000001f}},
	    {{0.514374971f, 0.203000009f}, {0.170000002f, 0.796999991f}},
	    {{0.594643712f, 0.671000004f}, {0.312700003f, 0.328999996f}},
	    {{0.514374971f, 0.203000009f}, {0.170000002f, 0.796999991f}},
	    {{0.817000031f, 0.708000004f}, {0.708000004f, 0.291999996f}},
	    {{0.594643712f, 0.671000004f}, {0.312700003f, 0.328999996f}},
	    {{0.817000031f, 0.708000004f}, {0.708000004f, 0.291999996f}},
	    {{0.492437482f, 0.953999996f}, {0.130999997f, 0.0460000001f}},
	    {{0.594643712f, 0.671000004f}, {0.312700003f, 0.328999996f}},
	};

	auto vertex_buffer_size = vkb::to_u32(vertices.size() * sizeof(Vertex));
	triangles_vertex_count  = static_cast<uint32_t>(vertices.size());

	// For the sake of simplicity we won't stage the vertex data to the gpu memory
	triangles_vertex_buffer = std::make_unique<vkb::core::Buffer>(get_device(),
	                                                              vertex_buffer_size,
	                                                              VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
	                                                              VMA_MEMORY_USAGE_CPU_TO_GPU);
	triangles_vertex_buffer->update(vertices.data(), vertex_buffer_size);

	std::vector<Vertex> gradient_vertices = {
	    {{-0.5f, -0.45f}, {0.0f, 0.0f, 0.0f}},
	    {{-0.5f, -0.55f}, {0.0f, 0.0f, 0.0f}},
	    {{0.5f, -0.45f}, {1.0f, 1.0f, 1.0f}},
	    {{0.5f, -0.55f}, {1.0f, 1.0f, 1.0f}},
	};

	std::vector<glm::vec3> colors = {
	    {1.0f, 0.0f, 0.0f},
	    {0.0f, 1.0f, 0.0f},
	    {0.0f, 0.0f, 1.0f},
	    {1.0f, 1.0f, 1.0f},
	};

	vertices.clear();

	float top    = -0.60f;
	float height = -0.1f;
	float offset = 0.15f;

	// SDR gradients (Colors = 1.0)
	for (auto c = 0; c < colors.size(); c++)
	{
		vertices.push_back({{-0.5f, top}, {0.0f, 0.0f, 0.0f}});
		vertices.push_back({{-0.5f, top + height}, {0.0f, 0.0f, 0.0f}});
		vertices.push_back({{0.5f, top}, {colors[c].r, colors[c].g, colors[c].b}});
		vertices.push_back({{0.5f, top + height}, {colors[c].r, colors[c].g, colors[c].b}});
		top += offset;
	}

	// HDR gradients (Colors > 1.0)
	top = 0.20f;
	float color_range = 3.0f;
	for (auto c = 0; c < colors.size(); c++)
	{
		vertices.push_back({{-0.5f, top}, {0.0f, 0.0f, 0.0f}});
		vertices.push_back({{-0.5f, top + height}, {0.0f, 0.0f, 0.0f}});
		vertices.push_back({{0.5f, top}, {colors[c].r * color_range, colors[c].g * color_range, colors[c].b * color_range}});
		vertices.push_back({{0.5f, top + height}, {colors[c].r * color_range, colors[c].g * color_range, colors[c].b * color_range}});
		top += offset;
	}

	vertex_buffer_size = vkb::to_u32(vertices.size() * sizeof(Vertex));
	gradient_count     = static_cast<uint32_t>(vertices.size()) / 4;

	// For the sake of simplicity we won't stage the vertex data to the gpu memory
	gradients_vertex_buffer = std::make_unique<vkb::core::Buffer>(get_device(),
	                                                              vertex_buffer_size,
	                                                              VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
	                                                              VMA_MEMORY_USAGE_CPU_TO_GPU);
	gradients_vertex_buffer->update(vertices.data(), vertex_buffer_size);
}

void hdr_display::prepare_pipelines()
{
	// Create a blank pipeline layout.
	// We are not binding any resources to the pipeline in this sample.
	VkPipelineLayoutCreateInfo layout_info = vkb::initializers::pipeline_layout_create_info(nullptr, 0);
	VK_CHECK(vkCreatePipelineLayout(get_device().get_handle(), &layout_info, nullptr, &sample_pipeline_layout));

	// Specify we will use triangle lists to draw geometry.
	VkPipelineInputAssemblyStateCreateInfo input_assembly = vkb::initializers::pipeline_input_assembly_state_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0, VK_FALSE);

	// Specify rasterization state.
	VkPipelineRasterizationStateCreateInfo raster = vkb::initializers::pipeline_rasterization_state_create_info(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE /* @todo */, VK_FRONT_FACE_CLOCKWISE);

	// Our attachment will write to all color channels, but no blending is enabled.
	VkPipelineColorBlendAttachmentState blend_attachment = vkb::initializers::pipeline_color_blend_attachment_state(VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT, VK_FALSE);

	VkPipelineColorBlendStateCreateInfo blend = vkb::initializers::pipeline_color_blend_state_create_info(1, &blend_attachment);

	// We will have one viewport and scissor box.
	VkPipelineViewportStateCreateInfo viewport = vkb::initializers::pipeline_viewport_state_create_info(1, 1);

	// Enable depth testing (using reversed depth-buffer for increased precision).
	VkPipelineDepthStencilStateCreateInfo depth_stencil = vkb::initializers::pipeline_depth_stencil_state_create_info(VK_TRUE, VK_TRUE, VK_COMPARE_OP_GREATER);

	// No multisampling.
	VkPipelineMultisampleStateCreateInfo multisample = vkb::initializers::pipeline_multisample_state_create_info(VK_SAMPLE_COUNT_1_BIT);

	// Specify that these states will be dynamic, i.e. not part of pipeline state object.
	std::array<VkDynamicState, 2>    dynamics{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	VkPipelineDynamicStateCreateInfo dynamic = vkb::initializers::pipeline_dynamic_state_create_info(dynamics.data(), vkb::to_u32(dynamics.size()));

	// Load our SPIR-V shaders.
	std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages{};

	// Vertex stage of the pipeline
	shader_stages[0] = load_shader("hdr_display/triangle.vert", VK_SHADER_STAGE_VERTEX_BIT);

	// Vertex bindings and attributes
	const std::vector<VkVertexInputBindingDescription> vertex_input_bindings = {
	    vkb::initializers::vertex_input_binding_description(0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX),
	};
	const std::vector<VkVertexInputAttributeDescription> vertex_input_attributes = {
	    vkb::initializers::vertex_input_attribute_description(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos)),
	    vkb::initializers::vertex_input_attribute_description(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)),
	};
	VkPipelineVertexInputStateCreateInfo vertex_input_state = vkb::initializers::pipeline_vertex_input_state_create_info();
	vertex_input_state.vertexBindingDescriptionCount        = static_cast<uint32_t>(vertex_input_bindings.size());
	vertex_input_state.pVertexBindingDescriptions           = vertex_input_bindings.data();
	vertex_input_state.vertexAttributeDescriptionCount      = static_cast<uint32_t>(vertex_input_attributes.size());
	vertex_input_state.pVertexAttributeDescriptions         = vertex_input_attributes.data();

	// We need to specify the pipeline layout and the render pass description up front as well.
	VkGraphicsPipelineCreateInfo pipeline_create_info = vkb::initializers::pipeline_create_info(sample_pipeline_layout, render_pass);
	pipeline_create_info.stageCount                   = vkb::to_u32(shader_stages.size());
	pipeline_create_info.pVertexInputState            = &vertex_input_state;
	pipeline_create_info.pStages                      = shader_stages.data();
	pipeline_create_info.pInputAssemblyState          = &input_assembly;
	pipeline_create_info.pRasterizationState          = &raster;
	pipeline_create_info.pColorBlendState             = &blend;
	pipeline_create_info.pMultisampleState            = &multisample;
	pipeline_create_info.pViewportState               = &viewport;
	pipeline_create_info.pDepthStencilState           = &depth_stencil;
	pipeline_create_info.pDynamicState                = &dynamic;

	input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	shader_stages[1]        = load_shader("hdr_display/triangle.frag", VK_SHADER_STAGE_FRAGMENT_BIT);
	VK_CHECK(vkCreateGraphicsPipelines(get_device().get_handle(), pipeline_cache, 1, &pipeline_create_info, nullptr, &triangles_pipeline));

	input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	shader_stages[1]        = load_shader("hdr_display/gradient.frag", VK_SHADER_STAGE_FRAGMENT_BIT);
	VK_CHECK(vkCreateGraphicsPipelines(get_device().get_handle(), pipeline_cache, 1, &pipeline_create_info, nullptr, &gradients_pipeline));
}

void hdr_display::build_command_buffers()
{
	VkCommandBufferBeginInfo command_buffer_begin_info = vkb::initializers::command_buffer_begin_info();

	// Clear color and depth values.
	VkClearValue clear_values[2];
	clear_values[0].color        = {{0.0f, 0.0f, 0.0f, 0.0f}};
	clear_values[1].depthStencil = {0.0f, 0};

	// Begin the render pass.
	VkRenderPassBeginInfo render_pass_begin_info    = vkb::initializers::render_pass_begin_info();
	render_pass_begin_info.renderPass               = render_pass;
	render_pass_begin_info.renderArea.offset.x      = 0;
	render_pass_begin_info.renderArea.offset.y      = 0;
	render_pass_begin_info.renderArea.extent.width  = width;
	render_pass_begin_info.renderArea.extent.height = height;
	render_pass_begin_info.clearValueCount          = 2;
	render_pass_begin_info.pClearValues             = clear_values;

	for (int32_t i = 0; i < draw_cmd_buffers.size(); ++i)
	{
		auto cmd = draw_cmd_buffers[i];

		// Begin command buffer.
		vkBeginCommandBuffer(cmd, &command_buffer_begin_info);

		// Set framebuffer for this command buffer.
		render_pass_begin_info.framebuffer = framebuffers[i];
		// We will add draw commands in the same command buffer.
		vkCmdBeginRenderPass(cmd, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

		// Set viewport dynamically
		VkViewport viewport = vkb::initializers::viewport(static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f);
		vkCmdSetViewport(cmd, 0, 1, &viewport);

		// Set scissor dynamically
		VkRect2D scissor = vkb::initializers::rect2D(width, height, 0, 0);
		vkCmdSetScissor(cmd, 0, 1, &scissor);

		VkDeviceSize offsets[1] = {0};

		// Triangles
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, triangles_pipeline);
		vkCmdBindVertexBuffers(cmd, 0, 1, triangles_vertex_buffer->get(), offsets);
		//vkCmdDraw(cmd, triangles_vertex_count, 1, 0, 0);

		// Gradients
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gradients_pipeline);
		vkCmdBindVertexBuffers(cmd, 0, 1, gradients_vertex_buffer->get(), offsets);
		for (auto i = 0; i < gradient_count; i++)
		{
			vkCmdDraw(cmd, 4, 1, i * 4, 0);
		}

		// Draw user interface.
		draw_ui(draw_cmd_buffers[i]);

		// Complete render pass.
		vkCmdEndRenderPass(cmd);

		// Complete the command buffer.
		VK_CHECK(vkEndCommandBuffer(cmd));
	}
}

void hdr_display::render(float delta_time)
{
	if (!prepared)
	{
		return;
	}
	if (update_hdr_metadata)
	{
		// @todo: comment, application dependant, only example values
		VkHdrMetadataEXT hdr_meta_data{};
		hdr_meta_data.sType                     = VK_STRUCTURE_TYPE_HDR_METADATA_EXT;
		hdr_meta_data.maxLuminance              = max_luminance;
		hdr_meta_data.minLuminance              = min_luminance;
		hdr_meta_data.maxContentLightLevel      = max_content_light_level;
		hdr_meta_data.maxFrameAverageLightLevel = max_frame_average_light_level;
		// @todo: toogle based on HDR format (Rec2020 for testing)
		// See https://en.wikipedia.org/wiki/Rec._2020#System_colorimetry
		hdr_meta_data.displayPrimaryRed   = {0.70800f, 0.29200f};
		hdr_meta_data.displayPrimaryGreen = {0.17000f, 0.79700f};
		hdr_meta_data.displayPrimaryBlue  = {0.13100f, 0.04600f};
		hdr_meta_data.whitePoint          = {0.31270f, 0.32900f};
		const VkSwapchainKHR swap_chain   = get_render_context().get_swapchain().get_handle();
		vkSetHdrMetadataEXT(get_device().get_handle(), 1, &swap_chain, &hdr_meta_data);
		update_hdr_metadata = false;
	}
	ApiVulkanSample::prepare_frame();
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers    = &draw_cmd_buffers[current_buffer];
	VK_CHECK(vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE));
	ApiVulkanSample::submit_frame();
}

void hdr_display::on_update_ui_overlay(vkb::Drawer &drawer)
{
	if (drawer.header("HDR meta data"))
	{
		update_hdr_metadata = drawer.input_float("Max. luminance", &max_luminance, 10.0f, 1);
		update_hdr_metadata |= drawer.input_float("Min. luminance", &min_luminance, 0.05f, 3);
		update_hdr_metadata |= drawer.input_float("Max. content light level", &max_content_light_level, 10.0f, 1);
		update_hdr_metadata |= drawer.input_float("Max. frame average light level", &max_frame_average_light_level, 10.0f, 1);
		if (drawer.button("Preset 1"))
		{
			max_luminance                 = 1000.0f;
			min_luminance                 = 0.001f;
			max_content_light_level       = 2000.0f;
			max_frame_average_light_level = 500.0f;
			update_hdr_metadata           = true;
		}
		if (drawer.button("Preset 2"))
		{
			max_luminance                 = 500.0f;
			min_luminance                 = 0.001f;
			max_content_light_level       = 2000.0f;
			max_frame_average_light_level = 500.0f;
			update_hdr_metadata           = true;
		}
		if (drawer.button("Preset 3"))
		{
			max_luminance                 = 500.0f;
			min_luminance                 = 0.100f;
			max_content_light_level       = 500.0f;
			max_frame_average_light_level = 100.0f;
			update_hdr_metadata           = true;
		}
		if (drawer.button("Preset 4"))
		{
			max_luminance                 = 2000.0f;
			min_luminance                 = 1.0f;
			max_content_light_level       = 2000.0f;
			max_frame_average_light_level = 1000.0f;
			update_hdr_metadata           = true;
		}
	}
}

std::unique_ptr<vkb::VulkanSample> create_hdr_display()
{
	return std::make_unique<hdr_display>();
}
