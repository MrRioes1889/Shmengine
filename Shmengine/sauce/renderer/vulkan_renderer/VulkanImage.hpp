#pragma once

#include "VulkanTypes.hpp"

namespace Renderer::Vulkan
{
	void vulkan_image_create(
		VulkanContext* context,
		TextureType type,
		uint32 width,
		uint32 height,
		VkFormat format,
		VkImageTiling tiling,
		VkImageUsageFlags usage,
		VkMemoryPropertyFlags memory_flags,
		bool32 create_view,
		VkImageAspectFlags view_aspect_flags,
		VulkanImage* out_image
	);

	void vulkan_image_destroy(VulkanContext* context, VulkanImage* image);

	void vulkan_image_view_create(
		VulkanContext* context,
		TextureType type,
		VkFormat format,
		VulkanImage* image,
		VkImageAspectFlags aspect_flags
	);

	void vulkan_image_transition_layout(
		VulkanContext* context,
		TextureType type,
		VulkanCommandBuffer* command_buffer,
		VulkanImage* image,
		VkFormat format,
		VkImageLayout old_layout,
		VkImageLayout new_layout
	);

	void vulkan_image_copy_from_buffer(VulkanContext* context, TextureType type, VulkanImage* image, VkBuffer buffer, VulkanCommandBuffer* command_buffer);
}



