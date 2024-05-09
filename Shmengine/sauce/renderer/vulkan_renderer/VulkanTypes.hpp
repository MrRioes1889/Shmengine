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
	VkSurfaceFormatKHR* formats;
	VkPresentModeKHR* present_modes;
	uint32 format_count;
	uint32 present_mode_count;
};

struct VulkanDevice
{
	VkPhysicalDevice physical_device;
	VkDevice logical_device;
	VulkanSwapchainSupportInfo swapchain_support;

	VkPhysicalDeviceProperties properties;
	VkPhysicalDeviceFeatures features;
	VkPhysicalDeviceMemoryProperties memory;

	VkQueue graphics_queue;
	VkQueue present_queue;
	VkQueue transfer_queue;
	int32 graphics_queue_index;
	int32 present_queue_index;
	int32 transfer_queue_index;
};

struct VulkanSwapchain
{
	VkSurfaceFormatKHR image_format;
	VkSwapchainKHR handle;
	VkImage* images;
	VkImageView* views;
	uint32 image_count;
	uint32 max_frames_in_flight;
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