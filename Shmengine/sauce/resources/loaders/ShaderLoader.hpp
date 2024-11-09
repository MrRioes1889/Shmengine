#pragma once

#include "systems/ResourceSystem.hpp"

namespace Renderer { struct ShaderConfig; }

namespace ResourceSystem
{
	bool32 shader_loader_load(const char* name, void* params, Renderer::ShaderConfig* out_config);
	void shader_loader_unload(Renderer::ShaderConfig* config);
}
