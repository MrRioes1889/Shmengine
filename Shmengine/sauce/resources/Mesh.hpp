#pragma once

#include "Defines.hpp"
#include "containers/Darray.hpp"
#include "utility/math/Transform.hpp"

struct Geometry;

struct MeshConfig
{
	const char* resource_name;
};

struct Mesh
{
	const char* resource_name;
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
