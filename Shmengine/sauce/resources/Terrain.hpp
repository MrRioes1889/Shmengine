#pragma once

#include "Defines.hpp"
#include "utility/String.hpp"
#include "utility/math/Transform.hpp"

struct GeometryData;
namespace GeometrySystem { struct GeometryConfig; }

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
	Math::Vec3f tangent;
};

struct TerrainConfig
{
	const char* name;
	uint32 tile_count_x;
	uint32 tile_count_z;
	uint32 tile_scale_z;
	uint32 tile_scale_z;

	uint32 materials_count;
	const char** material_names;
};

struct Terrain
{
	String name;
	Math::Transform xform;

	uint32 tile_count_x;
	uint32 tile_count_z;
	uint32 tile_scale_z;
	uint32 tile_scale_z;

	Math::Vec3f extents;
	Math::Vec3f origin;

	String resource_name;
	Darray<GeometrySystem::GeometryConfig> pending_g_configs;

	MeshState state;
	UniqueId unique_id;
	uint8 generation;
	Darray<GeometryData*> geometries;
	
};

SHMAPI bool32 mesh_init(MeshConfig* config, Mesh* out_mesh);
SHMAPI bool32 mesh_destroy(Mesh* mesh);
SHMAPI bool32 mesh_load(Mesh* mesh);
SHMAPI bool32 mesh_unload(Mesh* mesh);