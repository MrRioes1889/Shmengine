#pragma once

#include "VulkanTypes.hpp"

void vulkan_framebuffer_create(
	VulkanContext* context,
	VulkanRenderpass* renderpass,
	uint32 attachment_count,
	VkImageView* attachments,
	VulkanFramebuffer* out_framebuffer
);

void vullkan_framebuffer_destroy(VulkanContext* context, VulkanFramebuffer* framebuffer);
