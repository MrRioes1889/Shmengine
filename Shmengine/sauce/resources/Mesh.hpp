#pragma once

#include "Defines.hpp"
#include "ResourceTypes.hpp"
#include "systems/GeometrySystem.hpp"
#include "systems/MaterialSystem.hpp"
#include "utility/String.hpp"
#include "utility/math/Transform.hpp"

struct LightingInfo;
struct GeometryConfig;
struct GeometryData;
struct Material;

struct MeshGeometryConfig
{
	GeometryConfig* data_config;
	const char *material_name;
};

struct MeshGeometry
{
	char material_name[Constants::max_material_name_length];

	GeometrySystem::GeometryId g_id;
	MaterialId material_id;
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

	ResourceState state;
	UniqueId unique_id;
	uint8 generation;
	Sarray<MeshGeometry> geometries;
	Math::Extents3D extents;
	Math::Vec3f center;
	Math::Transform transform;
};

SHMAPI bool8 mesh_init(MeshConfig* config, Mesh* out_mesh);
SHMAPI bool8 mesh_init_from_resource(const char* resource_name, Mesh* out_mesh);
SHMAPI bool8 mesh_destroy(Mesh* mesh);
