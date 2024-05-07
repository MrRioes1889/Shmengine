#pragma once

#include "VulkanTypes.hpp"

struct VulkanContext;
namespace Platform
{
	struct PlatformState;

	bool32 create_vulkan_surface(PlatformState* plat_state, VulkanContext* context);
}



