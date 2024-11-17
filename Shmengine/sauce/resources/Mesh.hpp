#pragma once

#include "Defines.hpp"
#include "containers/Darray.hpp"
#include "utility/math/Transform.hpp"

struct Geometry;
namespace GeometrySystem { struct GeometryConfig; }

enum class MeshState
{
	DESTROYED,
	UNINITIALIZED,
	INITIALIZED,
	LOADING,
	LOADED,
	UNLOADING,
	UNLOADED
};

struct MeshConfig
{
	const char* resource_name;
	Darray<GeometrySystem::GeometryConfig> g_configs;
};

struct Mesh
{
	const char* resource_name;
	Darray<GeometrySystem::GeometryConfig> pending_g_configs;

	MeshState state;
	UniqueId unique_id;
	uint8 generation;
	Darray<Geometry*> geometries;
	Math::Transform transform;
};

SHMAPI bool32 mesh_create(MeshConfig* config, Mesh* out_mesh);
SHMAPI void mesh_destroy(Mesh* mesh);
SHMAPI bool32 mesh_init(Mesh* mesh);
SHMAPI bool32 mesh_load(Mesh* mesh);
SHMAPI bool32 mesh_unload(Mesh* mesh);
