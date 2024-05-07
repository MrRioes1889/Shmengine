#pragma once

#include "Defines.hpp"
#include "core/Assert.hpp"

#include <vulkan/vulkan.h>

#define VK_CHECK(x)						\
	{									\
		SHMASSERT((x) == VK_SUCCESS);		\
	}	

struct VulkanSwapchainSupportInfo
{
	VkSurfaceCapabilitiesKHR capabilities;
	uint32 format_count;
	VkSurfaceFormatKHR* formats;
	uint32 present_mode_count;
	VkPresentModeKHR* present_modes;
};

struct VulkanDevice
{
	VkPhysicalDevice physical_device;
	VkDevice logical_device;
	VulkanSwapchainSupportInfo swapchain_support;

	int32 graphics_queue_index;
	int32 present_queue_index;
	int32 transfer_queue_index;

	VkPhysicalDeviceProperties properties;
	VkPhysicalDeviceFeatures features;
	VkPhysicalDeviceMemoryProperties memory;
};

struct VulkanContext
{
	VkInstance instance;
	VkAllocationCallbacks* allocator_callbacks;
	VkSurfaceKHR surface;
	VulkanDevice device;

#if defined(_DEBUG)
	VkDebugUtilsMessengerEXT debug_messenger;
#endif

};