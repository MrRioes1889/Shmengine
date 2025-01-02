#pragma once

#include "Defines.hpp"
#include "utility/String.hpp"
#include "utility/math/Transform.hpp"
#include "systems/GeometrySystem.hpp"

struct LightingInfo;

enum class MeshState
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

struct MeshGeometryConfig
{
	GeometrySystem::GeometryConfig* data_config;
	const char *material_name;
};

struct MeshGeometry
{
	char material_name[max_material_name_length];

	GeometryData* g_data;
	Material* material;
};

struct MeshConfig
{
	uint32 g_configs_count;

	const char* name;
	MeshGeometryConfig* g_configs;
};

struct Mesh
{
	String name;

	MeshState state;
	UniqueId unique_id;
	uint8 generation;
	Darray<MeshGeometry> geometries;
	Math::Transform transform;
};

SHMAPI bool32 mesh_init(MeshConfig* config, Mesh* out_mesh);
SHMAPI bool32 mesh_init_from_resource(const char* resource_name, Mesh* out_mesh);
SHMAPI bool32 mesh_destroy(Mesh* mesh);
SHMAPI bool32 mesh_load(Mesh* mesh);
SHMAPI bool32 mesh_unload(Mesh* mesh);
