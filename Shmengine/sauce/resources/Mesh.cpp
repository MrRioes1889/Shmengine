#include "Mesh.hpp"

#include "core/Memory.hpp"
#include "core/Logging.hpp"
#include "renderer/RendererTypes.hpp"
#include "resources/loaders/MeshLoader.hpp"

#include "systems/JobSystem.hpp"
#include "systems/GeometrySystem.hpp"

struct MeshLoadParams
{
	const char* resource_name;
	Mesh* out_mesh;
	MeshResourceData mesh_resource;
};

static void mesh_load_job_success(void* params) {
    MeshLoadParams* mesh_params = (MeshLoadParams*)params;

    // This also handles the GPU upload. Can't be jobified until the renderer is multithreaded.
    Darray<GeometrySystem::GeometryConfig>* configs = &mesh_params->mesh_resource.g_configs;
    mesh_params->out_mesh->geometries.init(configs->count, 0);
    for (uint32 i = 0; i < configs->count; ++i) {
        Geometry** g = mesh_params->out_mesh->geometries.push(0);
        *g = GeometrySystem::acquire_from_config(&(*configs)[i], true);
    }
    mesh_params->out_mesh->generation++;   

    SHMTRACEV("Successfully loaded mesh '%s'.", mesh_params->resource_name);

    configs->free_data();
    ResourceSystem::mesh_loader_unload(&mesh_params->mesh_resource);
}

static void mesh_load_job_fail(void* params) {
    MeshLoadParams* mesh_params = (MeshLoadParams*)params;

    SHMERRORV("Failed to load mesh '%s'.", mesh_params->resource_name);

    ResourceSystem::mesh_loader_unload(&mesh_params->mesh_resource);
}

static bool32 mesh_load_job_start(void* params, void* result_data) {
    MeshLoadParams* load_params = (MeshLoadParams*)params;
    bool32 result = ResourceSystem::mesh_loader_load(load_params->resource_name, 0, &load_params->mesh_resource);

    // NOTE: The load params are also used as the result data here, only the mesh_resource field is populated now.
    Memory::copy_memory(load_params, result_data, sizeof(MeshLoadParams));

    return result;
}

bool32 mesh_load_from_resource(const char* resource_name, Mesh* out_mesh)
{
    out_mesh->generation = INVALID_ID8;

    JobSystem::JobInfo job = JobSystem::job_create(mesh_load_job_start, mesh_load_job_success, mesh_load_job_fail, sizeof(MeshLoadParams), sizeof(MeshLoadParams));
    MeshLoadParams* params = (MeshLoadParams*)job.params;
    params->resource_name = resource_name;
    params->out_mesh = out_mesh;
    params->mesh_resource = {};  
    JobSystem::submit(job);

    return true;
}

void mesh_unload(Mesh* m)
{
    for (uint32 i = 0; i < m->geometries.count; ++i)
        GeometrySystem::release(m->geometries[i]);

    m->geometries.free_data();

    // For good measure, invalidate the geometry so it doesn't attempt to be rendered.
    m->generation = INVALID_ID8;
}