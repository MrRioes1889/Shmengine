#pragma once

#include "Defines.hpp"
#include "VulkanTypes.hpp"

namespace Renderer::Vulkan
{
	void shaders_init_context(VulkanContext* c);
	void renderpass_init_context(VulkanContext* c);
}
