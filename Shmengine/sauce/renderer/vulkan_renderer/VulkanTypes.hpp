#pragma once

#include "Defines.hpp"
#include "core/Assert.hpp"
#include "containers/Sarray.hpp"
#include "utility/math/Math.hpp"

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

	VkCommandPool graphics_command_pool;

	VkQueue graphics_queue;
	VkQueue present_queue;
	VkQueue transfer_queue;
	int32 graphics_queue_index;
	int32 present_queue_index;
	int32 transfer_queue_index;

	VkFormat depth_format;
};

struct VulkanImage
{
	VkImage handle;
	VkDeviceMemory memory;
	VkImageView view;
	uint32 width;
	uint32 height;
};

enum class VulkanRenderpassState
{
	READY,
	RECORDING,
	IN_RENDER_PASS,
	RECORDING_ENDED,
	SUBMITTED,
	NOT_ALLOCATED
};

struct VulkanRenderpass
{
	VkRenderPass handle;
	Math::Vec2i offset;
	Math::Vec2ui dim;
	Math::Vec4f clear_color;
	float32 depth;
	uint32 stencil;
	VulkanRenderpassState state;
};

struct VulkanSwapchain
{
	VulkanImage depth_attachment;

	VkSurfaceFormatKHR image_format;
	VkSwapchainKHR handle;
	Sarray<VkImage> images = {};
	Sarray<VkImageView> views = {};
	uint32 max_frames_in_flight;
};

enum class VulkanCommandBufferState
{
	READY,
	RECORDING,
	IN_RENDER_PASS,
	RECORDING_ENDED,
	SUBMITTED,
	NOT_ALLOCATED
};

struct VulkanCommandBuffer
{
	VkCommandBuffer handle;
	VulkanCommandBufferState state;
};

struct VulkanContext
{
	int32(*find_memory_index)(uint32 type_filter, uint32 property_flags);

	VkInstance instance;
	VkAllocationCallbacks* allocator_callbacks;
	VkSurfaceKHR surface;
	VulkanDevice device;

	VulkanSwapchain swapchain;
	VulkanRenderpass main_renderpass;

	Sarray<VulkanCommandBuffer> graphics_command_buffers = {};

#if defined(_DEBUG)
	VkDebugUtilsMessengerEXT debug_messenger;
#endif

	uint32 image_index;
	uint32 current_frame;
	bool32 recreating_swapchain;

	uint32 framebuffer_width;
	uint32 framebuffer_height;
};