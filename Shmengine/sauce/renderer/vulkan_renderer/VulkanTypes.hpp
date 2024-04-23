#pragma once

#include "Defines.hpp"
#include "core/Assert.hpp"

#include <vulkan/vulkan.h>

#define VK_CHECK(x)						\
	{									\
		SHMASSERT(x == VK_SUCCESS);		\
	}							

struct VulkanContext
{
	VkInstance instance;
	VkAllocationCallbacks* allocator_callbacks;

#if defined(_DEBUG)
	VkDebugUtilsMessengerEXT debug_messenger;
#endif
};