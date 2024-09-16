#pragma once

#include "VulkanTypes.hpp"

namespace Renderer::Vulkan
{

	bool32 pipeline_create(VulkanContext* context, const VulkanPipelineConfig* config, VulkanPipeline* out_pipeline);
	void pipeline_destroy(VulkanContext* context, VulkanPipeline* pipeline);

	void pipeline_bind(VulkanCommandBuffer* command_buffer, VkPipelineBindPoint bind_point, VulkanPipeline* pipeline);

}


