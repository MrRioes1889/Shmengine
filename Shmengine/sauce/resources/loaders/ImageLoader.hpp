#pragma once

#include "systems/ResourceSystem.hpp"

namespace ResourceSystem
{
	bool32 image_loader_load(const char* name, void* params, ImageConfig* out_config);
	void image_loader_unload(ImageConfig* config);
}

