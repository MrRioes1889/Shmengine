#pragma once

#include <renderer/RendererTypes.hpp>

namespace Renderer::Vulkan
{
	extern "C"
	{
		SHMAPI bool8 create_module(Module* out_module);
		SHMAPI void destroy_module(Module* module);
	}
}
