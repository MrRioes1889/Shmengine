#pragma once

#include "Defines.hpp"
#include "ResourceTypes.hpp"
#include "utility/String.hpp"
#include "utility/math/Transform.hpp"
#include "systems/GeometrySystem.hpp"
#include "systems/MaterialSystem.hpp"

struct Material;
struct LightinInfo;
namespace Renderer
{
	struct RenderViewInstanceData;
}

struct TerrainVertex
{
	Math::Vec3f position;
	Math::Vec3f normal;
	Math::Vec2f tex_coords;
	Math::Vec4f color;
	Math::Vec3f tangent;

	float32 material_weights[Constants::max_terrain_materials_count];
};

struct TerrainVertexInfo
{
	float32 height;
};

struct TerrainConfig
{
	const char* name;
	uint32 tile_count_x;
	uint32 tile_count_z;
	float32 tile_scale_x;
	float32 tile_scale_z;

	float32 scale_y;

	uint32 materials_count;
	const char** material_names;

	const char* heightmap_name;
};

struct Terrain
{
	struct SubMaterial
	{
		char name[Constants::max_material_name_length];
		Material* mat;
	};

	String name;	
	Math::Transform xform;
	UniqueId unique_id;
	ResourceState state;

	uint32 tile_count_x;
	uint32 tile_count_z;
	float32 tile_scale_x;
	float32 tile_scale_z;

	float32 scale_y;

	GeometryData geometry;

	Sarray<TerrainVertexInfo> vertex_infos;

	MaterialTerrainProperties material_properties;
	
	Darray<SubMaterial> materials;

	uint32 shader_instance_id;
};

SHMAPI bool8 terrain_init(TerrainConfig* config, Terrain* out_terrain);
SHMAPI bool8 terrain_init_from_resource(const char* resource_name, Terrain* out_terrain);
SHMAPI bool8 terrain_destroy(Terrain* terrain);
SHMAPI bool8 terrain_load(Terrain* terrain);
SHMAPI bool8 terrain_unload(Terrain* terrain);

SHMAPI bool8 terrain_update(Terrain* terrain);