#pragma once

#include "VulkanTypes.hpp"
#include "utility/Math.hpp"

void vulkan_renderpass_create(
	VulkanContext* context,
	VulkanRenderpass* out_renderpass,
	Math::Vec2i offset, 
	Math::Vec2ui dim,
	Math::Vec4f clear_color,
	float32 depth,
	uint32 stencil
);
void vulkan_renderpass_destroy(VulkanContext* context, VulkanRenderpass* renderpass);

void vulkan_renderpass_begin(VulkanCommandBuffer* command_buffer, VulkanRenderpass* renderpass, VkFramebuffer framebuffer);
void vulkan_renderpass_end(VulkanCommandBuffer* command_buffer, VulkanRenderpass* renderpass);

