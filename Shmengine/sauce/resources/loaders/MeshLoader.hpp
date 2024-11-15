#pragma once

#include "Defines.hpp"
#include "containers/Darray.hpp"

namespace GeometrySystem { struct GeometryConfig; }

struct MeshResourceData
{
	Darray<GeometrySystem::GeometryConfig> g_configs;
};

namespace ResourceSystem
{
	bool32 mesh_loader_load(const char* name, void* params, MeshResourceData* out_resource);
	void mesh_loader_unload(MeshResourceData* resource);
}