#pragma once

#include "Defines.hpp"

struct Mesh;
struct MeshConfig;

SHMAPI bool8 mesh_init(MeshConfig* config, Mesh* out_mesh);
SHMAPI bool8 mesh_init_from_resource(const char* resource_name, Mesh* out_mesh);
SHMAPI bool8 mesh_destroy(Mesh* mesh);
