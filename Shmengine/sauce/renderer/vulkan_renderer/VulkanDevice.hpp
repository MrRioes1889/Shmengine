#pragma once

#include "VulkanTypes.hpp"

namespace Renderer::Vulkan
{
	bool32 vulkan_device_create(VulkanContext* context);
	void vulkan_device_destroy(VulkanContext* context);

	void vulkan_device_query_swapchain_support(VkPhysicalDevice device, VkSurfaceKHR surface, VulkanSwapchainSupportInfo* out_support_info);

	bool32 vulkan_device_detect_depth_format(VulkanDevice* device);
}