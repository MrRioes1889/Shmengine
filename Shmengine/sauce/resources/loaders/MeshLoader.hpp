#pragma once

#include "Defines.hpp"
#include "containers/Darray.hpp"
#include "utility/MathTypes.hpp"
#include "utility/String.hpp"
#include "renderer/RendererTypes.hpp"

struct MeshGeometryConfig;

struct MeshGeometryResourceData
{
	GeometryResourceData geometry_data;
	char material_name[Constants::max_geometry_name_length];
};

struct MeshResourceData
{
	Darray<MeshGeometryResourceData> geometries;

	Sarray<MeshGeometryConfig> mesh_geometry_configs;
};

namespace ResourceSystem
{
	bool8 mesh_loader_load(const char* name, MeshResourceData* out_resource);
	void mesh_loader_unload(MeshResourceData* resource);

	MeshConfig mesh_loader_get_config_from_resource(MeshResourceData* resource);
}