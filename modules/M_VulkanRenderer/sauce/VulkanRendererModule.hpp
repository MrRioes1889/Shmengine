#pragma once

#include <renderer/RendererTypes.hpp>

namespace Renderer::Vulkan
{
	SHMAPI bool32 create_module(Module* out_module);
	SHMAPI void destroy_module(Module* backend);
}
