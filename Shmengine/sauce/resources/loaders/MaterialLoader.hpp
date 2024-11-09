#pragma once

#include "Defines.hpp"

struct MaterialConfig;

namespace ResourceSystem
{
	bool32 material_loader_load(const char* name, void* params, MaterialConfig* out_config);
	void material_loader_unload(MaterialConfig* config);
}

