#pragma once

#include "VulkanTypes.hpp"

namespace Renderer::Vulkan
{

	bool32 pipeline_create
	(
		VulkanContext* context,
		VulkanRenderpass* renderpass,
		uint32 stride,
		uint32 attribute_count,
		VkVertexInputAttributeDescription* attributes,
		uint32 descriptor_set_layout_count,
		VkDescriptorSetLayout* descriptor_set_layouts,
		uint32 stage_count,
		VkPipelineShaderStageCreateInfo* stages,
		VkViewport viewport,
		VkRect2D scissor,
		bool32 is_wireframe,
		bool32 depth_test_enabled,
		uint32 push_constant_range_count,
		Range* push_constant_ranges,
		VulkanPipeline* out_pipeline
	);
	void pipeline_destroy(VulkanContext* context, VulkanPipeline* pipeline);

	void pipeline_bind(VulkanCommandBuffer* command_buffer, VkPipelineBindPoint bind_point, VulkanPipeline* pipeline);

}


