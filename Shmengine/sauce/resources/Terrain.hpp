#pragma once

#include "Defines.hpp"
#include "utility/String.hpp"
#include "utility/math/Transform.hpp"
#include "systems/GeometrySystem.hpp"
#include "systems/MaterialSystem.hpp"

struct Material;

enum class TerrainState
{
	UNINITIALIZED,
	DESTROYED,
	INITIALIZING,
	INITIALIZED,
	LOADING,
	LOADED,
	UNLOADING,
	UNLOADED
};

struct TerrainVertex
{
	Math::Vec3f position;
	Math::Vec3f normal;
	Math::Vec2f tex_coords;
	Math::Vec4f color;
	Math::Vec4f tangent;

	float32 material_weights[max_terrain_materials_count];
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
		char name[max_material_name_length];
	};

	String name;	
	Math::Transform xform;
	TerrainState state;

	uint32 tile_count_x;
	uint32 tile_count_z;
	float32 tile_scale_x;
	float32 tile_scale_z;

	float32 scale_y;

	GeometryData geometry;

	Sarray<TerrainVertexInfo> vertex_infos;

	MaterialTerrainProperties material_properties;
	
	Darray<SubMaterial> material_names;
	TextureMap texture_maps[max_terrain_materials_count * 3];
	uint32 shader_instance_id;

	uint32 render_frame_number;
};

SHMAPI bool32 terrain_init(TerrainConfig* config, Terrain* out_terrain);
SHMAPI bool32 terrain_init_from_resource(const char* resource_name, Terrain* out_terrain);
SHMAPI bool32 terrain_destroy(Terrain* terrain);
SHMAPI bool32 terrain_load(Terrain* terrain);
SHMAPI bool32 terrain_unload(Terrain* terrain);

SHMAPI bool32 terrain_update(Terrain* terrain);