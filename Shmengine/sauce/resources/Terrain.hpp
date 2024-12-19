#pragma once

#include "Defines.hpp"
#include "utility/String.hpp"
#include "utility/math/Transform.hpp"
#include "systems/GeometrySystem.hpp"

struct Material;

inline static const uint32 max_terrain_materials_count = 4;

enum class TerrainState
{
	UNINITIALIZED,
	DESTROYED,
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

	//float32 material_weights[max_terrain_materials_count];
};

//struct TerrainVertexData
//{
//	float32 height;
//};

struct TerrainConfig
{
	const char* name;
	uint32 tile_count_x;
	uint32 tile_count_z;
	float32 tile_scale_x;
	float32 tile_scale_z;

	Math::Transform xform;

	uint32 materials_count;
	const char** material_names;
};

struct Terrain
{
	struct PendingMaterial
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

	GeometryData geometry;
	
	Darray<PendingMaterial> pending_materials;
	Darray<Material*> materials;
};

SHMAPI bool32 terrain_init(TerrainConfig* config, Terrain* out_terrain);
SHMAPI bool32 terrain_destroy(Terrain* terrain);
SHMAPI bool32 terrain_load(Terrain* terrain);
SHMAPI bool32 terrain_unload(Terrain* terrain);