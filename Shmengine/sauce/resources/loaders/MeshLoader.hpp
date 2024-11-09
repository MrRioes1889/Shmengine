#pragma once

#include "systems/ResourceSystem.hpp"

namespace ResourceSystem
{
	bool32 mesh_loader_load(const char* name, void* params, MeshResourceData* out_resource);
	void mesh_loader_unload(MeshResourceData* resource);
}