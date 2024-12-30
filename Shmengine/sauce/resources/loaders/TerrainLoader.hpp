#pragma once

#include "Defines.hpp"
#include "containers/Sarray.hpp"
#include "utility/String.hpp"

struct TerrainVertexInfo;

struct TerrainResourceData
{
	struct SubMaterial
	{
		char name[max_material_name_length];
	};

	char name[max_terrain_name_length];
	uint32 tile_count_x;
	uint32 tile_count_z;
	float32 tile_scale_x;
	float32 tile_scale_z;
	float32 scale_y;

	uint32 sub_materials_count;
	SubMaterial sub_material_names[max_terrain_materials_count];

	char heightmap_name[max_texture_name_length];
};

namespace ResourceSystem
{
	bool32 terrain_loader_load(const char* name, void* params, TerrainResourceData* out_resource);
	void terrain_loader_unload(TerrainResourceData* resource);
}
