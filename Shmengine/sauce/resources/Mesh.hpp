#pragma once

#include "ResourceTypes.hpp"

SHMAPI bool32 mesh_load_from_resource(const char* resource_name, Mesh* out_mesh);

SHMAPI void mesh_unload(Mesh* m);
