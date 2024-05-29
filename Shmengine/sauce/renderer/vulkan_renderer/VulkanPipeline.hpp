#pragma once

#include "VulkanTypes.hpp"

bool32 vulkan_pipeline_create
(
	VulkanContext* context,
	VulkanRenderpass* renderpass,
	uint32 attribute_count,
	VkVertexInputAttributeDescription* attributes,
	uint32 descriptor_set_layout_count,
	VkDescriptorSetLayout* descpriptor_set_layouts,
	uint32 stage_count,
	VkPipelineShaderStageCreateInfo* stages,
	VkViewport viewport,
	VkRect2D scissor,
	bool32 is_wireframe,
	VulkanPipeline* out_pipeline
);
void vulkan_pipeline_destroy(VulkanContext* context, VulkanPipeline* pipeline);

void vulkan_pipeline_bind(VulkanCommandBuffer* command_buffer, VkPipelineBindPoint bind_point, VulkanPipeline* pipeline);
