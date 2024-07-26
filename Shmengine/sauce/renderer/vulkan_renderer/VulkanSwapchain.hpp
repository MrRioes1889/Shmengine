#pragma once

#include "VulkanTypes.hpp"

namespace Renderer::Vulkan
{
	bool32 vulkan_swapchain_create(VulkanContext* context, uint32 width, uint32 height, VulkanSwapchain* out_swapchain);
	bool32 vulkan_swapchain_recreate(VulkanContext* context, uint32 width, uint32 height, VulkanSwapchain* swapchain);
	void vulkan_swapchain_destroy(VulkanContext* context, VulkanSwapchain* swapchain);

	bool32 vulkan_swapchain_acquire_next_image_index(
		VulkanContext* context,
		VulkanSwapchain* swapchain,
		uint64 timeout_ns,
		VkSemaphore image_available_semaphore,
		VkFence fence,
		uint32* out_image_index);

	void vulkan_swapchain_present(
		VulkanContext* context,
		VulkanSwapchain* swapchain,
		VkQueue present_queue,
		VkSemaphore render_complete_semaphore,
		uint32 present_image_index);
}


