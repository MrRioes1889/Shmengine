#pragma once

#include "Defines.hpp"
#include "utility/String.hpp"
#include "utility/math/Transform.hpp"

struct Geometry;
namespace GeometrySystem { struct GeometryConfig; }

enum class MeshState
{
	UNINITIALIZED,
	DESTROYED,
	INITIALIZED,
	LOADING,
	LOADED,
	UNLOADING,
	UNLOADED
};

struct Mesh;

struct MeshConfig
{
	const char* name;
	const char* parent_name;
	const char* resource_name;
	Darray<GeometrySystem::GeometryConfig> g_configs;
	Math::Transform transform;
};

struct Mesh
{
	String name;
	String parent_name;
	String resource_name;
	Darray<GeometrySystem::GeometryConfig> pending_g_configs;

	MeshState state;
	UniqueId unique_id;
	uint8 generation;
	Darray<Geometry*> geometries;
	Math::Transform transform;
};

SHMAPI bool32 mesh_init(MeshConfig* config, Mesh* out_mesh);
SHMAPI bool32 mesh_destroy(Mesh* mesh);
SHMAPI bool32 mesh_load(Mesh* mesh);
SHMAPI bool32 mesh_unload(Mesh* mesh);
