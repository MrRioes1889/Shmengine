#pragma once

#include "VulkanTypes.hpp"
#include "utility/Math.hpp"

namespace RenderPassClearFlag
{
	enum
	{
		NONE = 0,
		COLOR_BUFFER = 1 << 0,
		DEPTH_BUFFER = 1 << 1,
		STENCIL_BUFFER = 1 << 2
	};
}

void vulkan_renderpass_create(
	VulkanContext* context,
	VulkanRenderpass* out_renderpass,
	Math::Vec2i offset,
	Math::Vec2u dim,
	Math::Vec4f clear_color,
	float32 depth,
	uint32 stencil,
	uint32 clear_flags,
	bool32 has_prev_pass,
	bool32 has_next_pass
);
void vulkan_renderpass_destroy(VulkanContext* context, VulkanRenderpass* renderpass);

void vulkan_renderpass_begin(VulkanCommandBuffer* command_buffer, VulkanRenderpass* renderpass, VkFramebuffer framebuffer);
void vulkan_renderpass_end(VulkanCommandBuffer* command_buffer, VulkanRenderpass* renderpass);

