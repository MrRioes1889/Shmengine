#pragma once

#include "Defines.hpp"
#include "containers/Darray.hpp"
#include "utility/String.hpp"

struct MeshGeometryConfig;

struct SceneMeshResourceData
{
	Darray<MeshGeometryConfig> configs;
};

namespace ResourceSystem
{
	bool32 mesh_loader_load(const char* name, void* params, SceneMeshResourceData* out_resource);
	void mesh_loader_unload(SceneMeshResourceData* resource);
}