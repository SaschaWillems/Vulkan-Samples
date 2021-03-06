/* Copyright (c) 2021, Sascha Willems
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

/*
 * Using callable shaders to execute different ray tracing shaders based on dynamic conditions
 */

#include "raytracing_callable.h"

RaytracingCallable::RaytracingCallable()
{
	title = "Hardware accelerated ray tracing";

	// SPIRV 1.4 requires Vulkan 1.1
	set_api_version(VK_API_VERSION_1_1);

	// Ray tracing related extensions required by this sample
	add_device_extension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
	add_device_extension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);

	// Required by VK_KHR_acceleration_structure
	add_device_extension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
	add_device_extension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
	add_device_extension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);

	// Required for VK_KHR_ray_tracing_pipeline
	add_device_extension(VK_KHR_SPIRV_1_4_EXTENSION_NAME);

	// Required by VK_KHR_spirv_1_4
	add_device_extension(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);
}

RaytracingCallable::~RaytracingCallable()
{
	if (device)
	{
		vkDestroyPipeline(get_device().get_handle(), pipeline, nullptr);
		vkDestroyPipelineLayout(get_device().get_handle(), pipeline_layout, nullptr);
		vkDestroyDescriptorSetLayout(get_device().get_handle(), descriptor_set_layout, nullptr);
		vkDestroyImageView(get_device().get_handle(), storage_image.view, nullptr);
		vkDestroyImage(get_device().get_handle(), storage_image.image, nullptr);
		vkFreeMemory(get_device().get_handle(), storage_image.memory, nullptr);
		vertex_buffer.reset();
		index_buffer.reset();
		ubo.reset();
	}
}

void RaytracingCallable::request_gpu_features(vkb::PhysicalDevice &gpu)
{
	// Enable extension features required by this sample
	// These are passed to device creation via a pNext structure chain
	auto &requested_buffer_device_address_features                  = gpu.request_extension_features<VkPhysicalDeviceBufferDeviceAddressFeatures>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES);
	requested_buffer_device_address_features.bufferDeviceAddress    = VK_TRUE;
	auto &requested_ray_tracing_features                            = gpu.request_extension_features<VkPhysicalDeviceRayTracingPipelineFeaturesKHR>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR);
	requested_ray_tracing_features.rayTracingPipeline               = VK_TRUE;
	auto &requested_acceleration_structure_features                 = gpu.request_extension_features<VkPhysicalDeviceAccelerationStructureFeaturesKHR>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR);
	requested_acceleration_structure_features.accelerationStructure = VK_TRUE;
}

/*
	Set up a storage image that the ray generation shader will be writing to
*/
void RaytracingCallable::create_storage_image()
{
	storage_image.width  = width;
	storage_image.height = height;

	VkImageCreateInfo image = vkb::initializers::image_create_info();
	image.imageType         = VK_IMAGE_TYPE_2D;
	image.format            = VK_FORMAT_B8G8R8A8_UNORM;
	image.extent.width      = storage_image.width;
	image.extent.height     = storage_image.height;
	image.extent.depth      = 1;
	image.mipLevels         = 1;
	image.arrayLayers       = 1;
	image.samples           = VK_SAMPLE_COUNT_1_BIT;
	image.tiling            = VK_IMAGE_TILING_OPTIMAL;
	image.usage             = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
	image.initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
	VK_CHECK(vkCreateImage(get_device().get_handle(), &image, nullptr, &storage_image.image));

	VkMemoryRequirements memory_requirements;
	vkGetImageMemoryRequirements(get_device().get_handle(), storage_image.image, &memory_requirements);
	VkMemoryAllocateInfo memory_allocate_info = vkb::initializers::memory_allocate_info();
	memory_allocate_info.allocationSize       = memory_requirements.size;
	memory_allocate_info.memoryTypeIndex      = get_device().get_memory_type(memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK(vkAllocateMemory(get_device().get_handle(), &memory_allocate_info, nullptr, &storage_image.memory));
	VK_CHECK(vkBindImageMemory(get_device().get_handle(), storage_image.image, storage_image.memory, 0));

	VkImageViewCreateInfo color_image_view           = vkb::initializers::image_view_create_info();
	color_image_view.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
	color_image_view.format                          = VK_FORMAT_B8G8R8A8_UNORM;
	color_image_view.subresourceRange                = {};
	color_image_view.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
	color_image_view.subresourceRange.baseMipLevel   = 0;
	color_image_view.subresourceRange.levelCount     = 1;
	color_image_view.subresourceRange.baseArrayLayer = 0;
	color_image_view.subresourceRange.layerCount     = 1;
	color_image_view.image                           = storage_image.image;
	VK_CHECK(vkCreateImageView(get_device().get_handle(), &color_image_view, nullptr, &storage_image.view));

	VkCommandBuffer command_buffer = get_device().create_command_buffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	vkb::set_image_layout(command_buffer, storage_image.image,
	                      VK_IMAGE_LAYOUT_UNDEFINED,
	                      VK_IMAGE_LAYOUT_GENERAL,
	                      {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
	get_device().flush_command_buffer(command_buffer, queue);
}

/*
	Create the bottom level acceleration structure that contains the scene's geometry (triangles)
*/
void RaytracingCallable::create_bottom_level_acceleration_structure()
{
	// Setup vertices and indices for a single triangle
	struct Vertex
	{
		float pos[3];
	};
	std::vector<Vertex> vertices = {
	    {{1.0f, 1.0f, 0.0f}},
	    {{-1.0f, 1.0f, 0.0f}},
	    {{0.0f, -1.0f, 0.0f}}};
	std::vector<uint32_t> indices = {0, 1, 2};

	// Setup transform matrices for the geometries in the bottom level AS
	std::vector<VkTransformMatrixKHR> transform_matrices(3);
	for (uint32_t i = 0; i < 3; i++)
	{
		transform_matrices[i] = {
		    1.0f, 0.0f, 0.0f, (float) i * 3.0f - 3.0f,
		    0.0f, 1.0f, 0.0f, 0.0f,
		    0.0f, 0.0f, 1.0f, 0.0f};
	}

	auto vertex_buffer_size = vertices.size() * sizeof(Vertex);
	auto index_buffer_size  = indices.size() * sizeof(uint32_t);

	// Create buffers for the bottom level geometry
	// For the sake of simplicity we won't stage the vertex data to the GPU memory

	// Note that the buffer usage flags for buffers consumed by the bottom level acceleration structure require special flags
	const VkBufferUsageFlags buffer_usage_flags = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

	vertex_buffer = std::make_unique<vkb::core::Buffer>(get_device(), vertex_buffer_size, buffer_usage_flags, VMA_MEMORY_USAGE_CPU_TO_GPU);
	vertex_buffer->update(vertices.data(), vertex_buffer_size);

	index_buffer = std::make_unique<vkb::core::Buffer>(get_device(), index_buffer_size, buffer_usage_flags, VMA_MEMORY_USAGE_CPU_TO_GPU);
	index_buffer->update(indices.data(), index_buffer_size);

	// Setup a single transformation matrix that can be used to transform the whole geometry for a single bottom level acceleration structure
	std::unique_ptr<vkb::core::Buffer> transform_data_buffer = std::make_unique<vkb::core::Buffer>(get_device(), sizeof(VkTransformMatrixKHR) * 3, buffer_usage_flags, VMA_MEMORY_USAGE_CPU_TO_GPU);
	transform_data_buffer->update(transform_matrices.data(), sizeof(VkTransformMatrixKHR) * 3);

	// The bottom level acceleration structure for this sample contains three separate triangle geometries, so we can use gl_GeometryIndexEXT in the closest hit shader to select different callable shaders
	bottom_level_acceleration_structure = std::make_unique<vkb::core::AccelerationStructure>(get_device(), VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR);
	bottom_level_acceleration_structure->add_triangle_geometry(vertex_buffer, index_buffer, transform_data_buffer, 1, 2, sizeof(Vertex), 0);
	bottom_level_acceleration_structure->add_triangle_geometry(vertex_buffer, index_buffer, transform_data_buffer, 1, 2, sizeof(Vertex), sizeof(VkTransformMatrixKHR));
	bottom_level_acceleration_structure->add_triangle_geometry(vertex_buffer, index_buffer, transform_data_buffer, 1, 2, sizeof(Vertex), sizeof(VkTransformMatrixKHR) * 2);
	bottom_level_acceleration_structure->build(queue);
}

/*
	Create the top level acceleration structure containing geometry instances of the bottom level acceleration structure(s)
*/
void RaytracingCallable::create_top_level_acceleration_structure()
{
	VkTransformMatrixKHR transform_matrix = {
	    1.0f, 0.0f, 0.0f, 0.0f,
	    0.0f, 1.0f, 0.0f, 0.0f,
	    0.0f, 0.0f, 1.0f, 0.0f};

	VkAccelerationStructureInstanceKHR acceleration_structure_instance{};
	acceleration_structure_instance.transform                              = transform_matrix;
	acceleration_structure_instance.instanceCustomIndex                    = 0;
	acceleration_structure_instance.mask                                   = 0xFF;
	acceleration_structure_instance.instanceShaderBindingTableRecordOffset = 0;
	acceleration_structure_instance.flags                                  = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
	acceleration_structure_instance.accelerationStructureReference         = bottom_level_acceleration_structure->get_device_address();

	std::unique_ptr<vkb::core::Buffer> instances_buffer = std::make_unique<vkb::core::Buffer>(get_device(),
	                                                                                          sizeof(VkAccelerationStructureInstanceKHR),
	                                                                                          VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
	                                                                                          VMA_MEMORY_USAGE_CPU_TO_GPU);
	instances_buffer->update(&acceleration_structure_instance, sizeof(VkAccelerationStructureInstanceKHR));

	top_level_acceleration_structure = std::make_unique<vkb::core::AccelerationStructure>(get_device(), VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR);
	top_level_acceleration_structure->add_instance_geometry(instances_buffer, 1);
	top_level_acceleration_structure->build(queue);
}

inline uint32_t aligned_size(uint32_t value, uint32_t alignment)
{
	return (value + alignment - 1) & ~(alignment - 1);
}

/*
	Create scene geometry and ray tracing acceleration structures
*/
void RaytracingCallable::create_scene()
{
	create_bottom_level_acceleration_structure();
	create_top_level_acceleration_structure();
}

/*
	Create the Shader Binding Tables that connects the ray tracing pipelines' programs and the  top-level acceleration structure

	SBT Layout used in this sample:

		/-----------\
		| raygen    |
		|-----------|
		| miss      |
		|-----------|
		| hit       |
		|-----------|
		| callable0 |
		| callable1 |
		| callabel2 |
		\-----------/
*/

void RaytracingCallable::create_shader_binding_tables()
{
	const uint32_t handle_size         = ray_tracing_pipeline_properties.shaderGroupHandleSize;
	const uint32_t handle_size_aligned = aligned_size(ray_tracing_pipeline_properties.shaderGroupHandleSize, ray_tracing_pipeline_properties.shaderGroupHandleAlignment);
	const uint32_t group_count         = static_cast<uint32_t>(shader_groups.size());
	const uint32_t sbt_size            = group_count * handle_size_aligned;

	// Create binding table buffers for each shader type
	raygen_shader_binding_table   = std::make_unique<vkb::core::ShaderBindingTable>(get_device(), 1, handle_size_aligned);
	miss_shader_binding_table     = std::make_unique<vkb::core::ShaderBindingTable>(get_device(), 1, handle_size_aligned);
	hit_shader_binding_table      = std::make_unique<vkb::core::ShaderBindingTable>(get_device(), 1, handle_size_aligned);
	callable_shader_binding_table = std::make_unique<vkb::core::ShaderBindingTable>(get_device(), 3, handle_size_aligned);

	// Copy the pipeline's shader handles into a host buffer
	std::vector<uint8_t> shader_handle_storage(sbt_size);
	VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(get_device().get_handle(), pipeline, 0, group_count, sbt_size, shader_handle_storage.data()));

	// Copy the shader handles from the host buffer to the binding tables
	memcpy(raygen_shader_binding_table->get_data(), shader_handle_storage.data(), handle_size);
	memcpy(miss_shader_binding_table->get_data(), shader_handle_storage.data() + handle_size_aligned, handle_size);
	memcpy(hit_shader_binding_table->get_data(), shader_handle_storage.data() + handle_size_aligned * 2, handle_size);
	memcpy(callable_shader_binding_table->get_data(), shader_handle_storage.data() + handle_size_aligned * 3, handle_size * 3);
}

/*
	Create the descriptor sets used for the ray tracing dispatch
*/
void RaytracingCallable::create_descriptor_sets()
{
	std::vector<VkDescriptorPoolSize> pool_sizes = {
	    {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1},
	    {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
	    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}};
	VkDescriptorPoolCreateInfo descriptor_pool_create_info = vkb::initializers::descriptor_pool_create_info(pool_sizes, 1);
	VK_CHECK(vkCreateDescriptorPool(get_device().get_handle(), &descriptor_pool_create_info, nullptr, &descriptor_pool));

	VkDescriptorSetAllocateInfo descriptor_set_allocate_info = vkb::initializers::descriptor_set_allocate_info(descriptor_pool, &descriptor_set_layout, 1);
	VK_CHECK(vkAllocateDescriptorSets(get_device().get_handle(), &descriptor_set_allocate_info, &descriptor_set));

	// Setup the descriptor for binding our top level acceleration structure to the ray tracing shaders
	VkWriteDescriptorSetAccelerationStructureKHR descriptor_acceleration_structure_info{};
	descriptor_acceleration_structure_info.sType                      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
	descriptor_acceleration_structure_info.accelerationStructureCount = 1;
	descriptor_acceleration_structure_info.pAccelerationStructures    = top_level_acceleration_structure->get();

	VkWriteDescriptorSet acceleration_structure_write{};
	acceleration_structure_write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	acceleration_structure_write.dstSet          = descriptor_set;
	acceleration_structure_write.dstBinding      = 0;
	acceleration_structure_write.descriptorCount = 1;
	acceleration_structure_write.descriptorType  = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	// The acceleration structure descriptor has to be chained via pNext
	acceleration_structure_write.pNext = &descriptor_acceleration_structure_info;

	VkDescriptorImageInfo image_descriptor{};
	image_descriptor.imageView   = storage_image.view;
	image_descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	VkDescriptorBufferInfo buffer_descriptor = create_descriptor(*ubo);

	VkWriteDescriptorSet result_image_write   = vkb::initializers::write_descriptor_set(descriptor_set, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, &image_descriptor);
	VkWriteDescriptorSet uniform_buffer_write = vkb::initializers::write_descriptor_set(descriptor_set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2, &buffer_descriptor);

	std::vector<VkWriteDescriptorSet> write_descriptor_sets = {
	    acceleration_structure_write,
	    result_image_write,
	    uniform_buffer_write};
	vkUpdateDescriptorSets(get_device().get_handle(), static_cast<uint32_t>(write_descriptor_sets.size()), write_descriptor_sets.data(), 0, VK_NULL_HANDLE);
}

/*
	Create our ray tracing pipeline
*/
void RaytracingCallable::create_ray_tracing_pipeline()
{
	// Slot for binding top level acceleration structures to the ray generation shader
	VkDescriptorSetLayoutBinding acceleration_structure_layout_binding{};
	acceleration_structure_layout_binding.binding         = 0;
	acceleration_structure_layout_binding.descriptorType  = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	acceleration_structure_layout_binding.descriptorCount = 1;
	acceleration_structure_layout_binding.stageFlags      = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

	VkDescriptorSetLayoutBinding result_image_layout_binding{};
	result_image_layout_binding.binding         = 1;
	result_image_layout_binding.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	result_image_layout_binding.descriptorCount = 1;
	result_image_layout_binding.stageFlags      = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

	VkDescriptorSetLayoutBinding uniform_buffer_binding{};
	uniform_buffer_binding.binding         = 2;
	uniform_buffer_binding.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uniform_buffer_binding.descriptorCount = 1;
	uniform_buffer_binding.stageFlags      = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

	std::vector<VkDescriptorSetLayoutBinding> bindings = {
	    acceleration_structure_layout_binding,
	    result_image_layout_binding,
	    uniform_buffer_binding};

	VkDescriptorSetLayoutCreateInfo layout_info{};
	layout_info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
	layout_info.pBindings    = bindings.data();
	VK_CHECK(vkCreateDescriptorSetLayout(get_device().get_handle(), &layout_info, nullptr, &descriptor_set_layout));

	VkPipelineLayoutCreateInfo pipeline_layout_create_info{};
	pipeline_layout_create_info.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout_create_info.setLayoutCount = 1;
	pipeline_layout_create_info.pSetLayouts    = &descriptor_set_layout;

	VK_CHECK(vkCreatePipelineLayout(get_device().get_handle(), &pipeline_layout_create_info, nullptr, &pipeline_layout));

	// Ray tracing shaders require SPIR-V 1.4, so we need to set the appropriate target envinroment for the glslang compiler
	vkb::GLSLCompiler::set_target_environment(glslang::EShTargetSpv, glslang::EShTargetSpv_1_4);

	/*
		Setup ray tracing shader groups
		Each shader group points at the corresponding shader in the pipeline
	*/
	std::vector<VkPipelineShaderStageCreateInfo> shader_stages;

	// Ray generation group
	shader_stages.push_back(load_shader("khr_ray_tracing_callable/raygen.rgen", VK_SHADER_STAGE_RAYGEN_BIT_KHR));
	VkRayTracingShaderGroupCreateInfoKHR raygen_group_ci{};
	raygen_group_ci.sType              = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	raygen_group_ci.type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	raygen_group_ci.generalShader      = static_cast<uint32_t>(shader_stages.size()) - 1;
	raygen_group_ci.closestHitShader   = VK_SHADER_UNUSED_KHR;
	raygen_group_ci.anyHitShader       = VK_SHADER_UNUSED_KHR;
	raygen_group_ci.intersectionShader = VK_SHADER_UNUSED_KHR;
	shader_groups.push_back(raygen_group_ci);

	// Ray miss group
	shader_stages.push_back(load_shader("khr_ray_tracing_callable/miss.rmiss", VK_SHADER_STAGE_MISS_BIT_KHR));
	VkRayTracingShaderGroupCreateInfoKHR miss_group_ci{};
	miss_group_ci.sType              = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	miss_group_ci.type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	miss_group_ci.generalShader      = static_cast<uint32_t>(shader_stages.size()) - 1;
	miss_group_ci.closestHitShader   = VK_SHADER_UNUSED_KHR;
	miss_group_ci.anyHitShader       = VK_SHADER_UNUSED_KHR;
	miss_group_ci.intersectionShader = VK_SHADER_UNUSED_KHR;
	shader_groups.push_back(miss_group_ci);

	// Ray closest hit group
	shader_stages.push_back(load_shader("khr_ray_tracing_callable/closesthit.rchit", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR));
	VkRayTracingShaderGroupCreateInfoKHR closes_hit_group_ci{};
	closes_hit_group_ci.sType              = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	closes_hit_group_ci.type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
	closes_hit_group_ci.generalShader      = VK_SHADER_UNUSED_KHR;
	closes_hit_group_ci.closestHitShader   = static_cast<uint32_t>(shader_stages.size()) - 1;
	closes_hit_group_ci.anyHitShader       = VK_SHADER_UNUSED_KHR;
	closes_hit_group_ci.intersectionShader = VK_SHADER_UNUSED_KHR;
	shader_groups.push_back(closes_hit_group_ci);

	// Callable shader group
	// This sample's hit shader will call different callable shaders depending on the geometry index using executeCallableEXT, so as we render three geometries, we'll also use three callable shaders
	for (uint32_t i = 0; i < 3; i++)
	{
		shader_stages.push_back(load_shader("khr_ray_tracing_callable/callable" + std::to_string(i + 1) + ".rcall", VK_SHADER_STAGE_CALLABLE_BIT_KHR));
		VkRayTracingShaderGroupCreateInfoKHR callable_group_ci{};
		callable_group_ci.sType              = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
		callable_group_ci.type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
		callable_group_ci.generalShader      = static_cast<uint32_t>(shader_stages.size()) - 1;
		callable_group_ci.closestHitShader   = VK_SHADER_UNUSED_KHR;
		callable_group_ci.anyHitShader       = VK_SHADER_UNUSED_KHR;
		callable_group_ci.intersectionShader = VK_SHADER_UNUSED_KHR;
		shader_groups.push_back(callable_group_ci);
	}

	/*
		Create the ray tracing pipeline
	*/
	VkRayTracingPipelineCreateInfoKHR raytracing_pipeline_create_info{};
	raytracing_pipeline_create_info.sType                        = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
	raytracing_pipeline_create_info.stageCount                   = static_cast<uint32_t>(shader_stages.size());
	raytracing_pipeline_create_info.pStages                      = shader_stages.data();
	raytracing_pipeline_create_info.groupCount                   = static_cast<uint32_t>(shader_groups.size());
	raytracing_pipeline_create_info.pGroups                      = shader_groups.data();
	raytracing_pipeline_create_info.maxPipelineRayRecursionDepth = 1;
	raytracing_pipeline_create_info.layout                       = pipeline_layout;
	VK_CHECK(vkCreateRayTracingPipelinesKHR(get_device().get_handle(), VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &raytracing_pipeline_create_info, nullptr, &pipeline));
}

/*
	Create the uniform buffer used to pass matrices to the ray tracing ray generation shader
*/
void RaytracingCallable::create_uniform_buffer()
{
	ubo = std::make_unique<vkb::core::Buffer>(get_device(),
	                                          sizeof(uniform_data),
	                                          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
	                                          VMA_MEMORY_USAGE_CPU_TO_GPU);
	ubo->convert_and_update(uniform_data);

	update_uniform_buffers();
}

/*
	Command buffer generation
*/
void RaytracingCallable::build_command_buffers()
{
	if (width != storage_image.width || height != storage_image.height)
	{
		// If the view port size has changed, we need to recreate the storage image
		vkDestroyImageView(get_device().get_handle(), storage_image.view, nullptr);
		vkDestroyImage(get_device().get_handle(), storage_image.image, nullptr);
		vkFreeMemory(get_device().get_handle(), storage_image.memory, nullptr);
		create_storage_image();
		// The descriptor also needs to be updated to reference the new image
		VkDescriptorImageInfo image_descriptor{};
		image_descriptor.imageView              = storage_image.view;
		image_descriptor.imageLayout            = VK_IMAGE_LAYOUT_GENERAL;
		VkWriteDescriptorSet result_image_write = vkb::initializers::write_descriptor_set(descriptor_set, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, &image_descriptor);
		vkUpdateDescriptorSets(get_device().get_handle(), 1, &result_image_write, 0, VK_NULL_HANDLE);
		build_command_buffers();
	}

	VkCommandBufferBeginInfo command_buffer_begin_info = vkb::initializers::command_buffer_begin_info();

	VkImageSubresourceRange subresource_range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

	for (int32_t i = 0; i < draw_cmd_buffers.size(); ++i)
	{
		VK_CHECK(vkBeginCommandBuffer(draw_cmd_buffers[i], &command_buffer_begin_info));

		/*
			Setup the strided device address regions pointing at the shader identifiers in the shader binding table
		*/

		const uint32_t handle_size_aligned = aligned_size(ray_tracing_pipeline_properties.shaderGroupHandleSize, ray_tracing_pipeline_properties.shaderGroupHandleAlignment);

		VkStridedDeviceAddressRegionKHR callable_shader_sbt_entry{};

		/*
			Dispatch the ray tracing commands
		*/
		vkCmdBindPipeline(draw_cmd_buffers[i], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
		vkCmdBindDescriptorSets(draw_cmd_buffers[i], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline_layout, 0, 1, &descriptor_set, 0, 0);

		vkCmdTraceRaysKHR(
		    draw_cmd_buffers[i],
		    raygen_shader_binding_table->get_strided_device_address_region(),
		    miss_shader_binding_table->get_strided_device_address_region(),
		    hit_shader_binding_table->get_strided_device_address_region(),
		    callable_shader_binding_table->get_strided_device_address_region(),
		    width,
		    height,
		    1);

		/*
			Copy ray tracing output to swap chain image
		*/

		// Prepare current swap chain image as transfer destination
		vkb::set_image_layout(
		    draw_cmd_buffers[i],
		    get_render_context().get_swapchain().get_images()[i],
		    VK_IMAGE_LAYOUT_UNDEFINED,
		    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		    subresource_range);

		// Prepare ray tracing output image as transfer source
		vkb::set_image_layout(
		    draw_cmd_buffers[i],
		    storage_image.image,
		    VK_IMAGE_LAYOUT_GENERAL,
		    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		    subresource_range);

		VkImageCopy copy_region{};
		copy_region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
		copy_region.srcOffset      = {0, 0, 0};
		copy_region.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
		copy_region.dstOffset      = {0, 0, 0};
		copy_region.extent         = {width, height, 1};
		vkCmdCopyImage(draw_cmd_buffers[i], storage_image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		               get_render_context().get_swapchain().get_images()[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

		// Transition swap chain image back for presentation
		vkb::set_image_layout(
		    draw_cmd_buffers[i],
		    get_render_context().get_swapchain().get_images()[i],
		    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		    subresource_range);

		// Transition ray tracing output image back to general layout
		vkb::set_image_layout(
		    draw_cmd_buffers[i],
		    storage_image.image,
		    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		    VK_IMAGE_LAYOUT_GENERAL,
		    subresource_range);
		VK_CHECK(vkEndCommandBuffer(draw_cmd_buffers[i]));
	}
}

void RaytracingCallable::update_uniform_buffers()
{
	uniform_data.proj_inverse = glm::inverse(camera.matrices.perspective);
	uniform_data.view_inverse = glm::inverse(camera.matrices.view);
	ubo->convert_and_update(uniform_data);
}

bool RaytracingCallable::prepare(vkb::Platform &platform)
{
	if (!ApiVulkanSample::prepare(platform))
	{
		return false;
	}

	// This sample copies the ray traced output to the swap chain image, so we need to enable the required image usage flags
	std::set<VkImageUsageFlagBits> image_usage_flags = {VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_USAGE_TRANSFER_DST_BIT};
	get_render_context().update_swapchain(image_usage_flags);

	// Get the ray tracing pipeline properties, which we'll need later on in the sample
	ray_tracing_pipeline_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
	VkPhysicalDeviceProperties2 device_properties{};
	device_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	device_properties.pNext = &ray_tracing_pipeline_properties;
	vkGetPhysicalDeviceProperties2(get_device().get_gpu().get_handle(), &device_properties);

	// Get the acceleration structure features, which we'll need later on in the sample
	acceleration_structure_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
	VkPhysicalDeviceFeatures2 device_features{};
	device_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	device_features.pNext = &acceleration_structure_features;
	vkGetPhysicalDeviceFeatures2(get_device().get_gpu().get_handle(), &device_features);

	camera.type = vkb::CameraType::LookAt;
	camera.set_perspective(60.0f, (float) width / (float) height, 0.1f, 512.0f);
	camera.set_rotation(glm::vec3(0.0f, 0.0f, 0.0f));
	camera.set_translation(glm::vec3(0.0f, 0.0f, -2.5f));

	create_storage_image();
	create_scene();
	create_uniform_buffer();
	create_ray_tracing_pipeline();
	create_shader_binding_tables();
	create_descriptor_sets();
	build_command_buffers();
	prepared = true;
	return true;
}

void RaytracingCallable::draw()
{
	ApiVulkanSample::prepare_frame();
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers    = &draw_cmd_buffers[current_buffer];
	VK_CHECK(vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE));
	ApiVulkanSample::submit_frame();
}

void RaytracingCallable::render(float delta_time)
{
	if (!prepared)
		return;
	draw();
	if (camera.updated)
		update_uniform_buffers();
}

std::unique_ptr<vkb::VulkanSample> create_raytracing_callable()
{
	return std::make_unique<RaytracingCallable>();
}