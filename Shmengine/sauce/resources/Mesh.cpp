#include "Mesh.hpp"

#include "core/Memory.hpp"
#include "core/Logging.hpp"
#include "core/Identifier.hpp"
#include "renderer/RendererTypes.hpp"
#include "resources/loaders/MeshLoader.hpp"

#include "systems/JobSystem.hpp"
#include "systems/GeometrySystem.hpp"

static void mesh_load_job_success(void* params);
static void mesh_load_job_fail(void* params);
static bool32 mesh_load_job_start(void* params, void* result_data);
static bool32 mesh_load_async(Mesh* mesh, bool32 reload);

bool32 mesh_init(MeshConfig* config, Mesh* out_mesh)
{
    if (out_mesh->state >= MeshState::INITIALIZED)
        return false;

    out_mesh->name = config->name;
    out_mesh->parent_name = config->parent_name;
    out_mesh->resource_name = config->resource_name;
    out_mesh->transform = config->transform;
    out_mesh->pending_g_configs.steal(*config->g_configs);

    out_mesh->generation = INVALID_ID8;

    out_mesh->state = MeshState::INITIALIZED;

    return true;
}

bool32 mesh_destroy(Mesh* mesh)
{
    if (mesh->state != MeshState::UNLOADED && !mesh_unload(mesh))
        return false;

    for (uint32 i = 0; i < mesh->pending_g_configs.count; i++)
    {
        mesh->pending_g_configs[i].vertices.free_data();
        mesh->pending_g_configs[i].indices.free_data();
    }
    mesh->pending_g_configs.free_data();

    mesh->name.free_data();
    mesh->parent_name.free_data();
    mesh->resource_name.free_data();

    mesh->state = MeshState::DESTROYED;
    return true;
}

bool32 mesh_load(Mesh* mesh)
{

    if (mesh->state != MeshState::INITIALIZED && mesh->state != MeshState::UNLOADED)
        return false;

    bool32 is_reload = mesh->state == MeshState::UNLOADED;

    mesh->state = MeshState::LOADING;
    mesh->generation = INVALID_ID8;
    mesh->unique_id = identifier_acquire_new_id(mesh);

    mesh_load_async(mesh, is_reload);

    return true;

}

bool32 mesh_unload(Mesh* mesh)
{

    if (mesh->state <= MeshState::INITIALIZED)
        return true;
    else if (mesh->state != MeshState::LOADED)
        return false;

    mesh->state = MeshState::UNLOADING;

    mesh->generation = INVALID_ID8;
    identifier_release_id(mesh->unique_id);

    if (!mesh->pending_g_configs.data)
        mesh->pending_g_configs.init(mesh->geometries.count, 0);

    for (uint32 i = 0; i < mesh->geometries.count; ++i)
    {
        GeometrySystem::GeometryConfig* out_config = 0;
        if (GeometrySystem::get_ref_count(mesh->geometries[i]) <= 1)
            out_config = mesh->pending_g_configs.emplace();

        GeometrySystem::release(mesh->geometries[i], out_config);
    }

    mesh->geometries.free_data();

    mesh->state = MeshState::UNLOADED;

    return true;

}

struct MeshLoadParams
{
	Mesh* out_mesh;
	MeshResourceData mesh_resource;
    bool32 is_reload;
};

static void mesh_load_job_success(void* params) {
    MeshLoadParams* mesh_params = (MeshLoadParams*)params;
    Mesh* mesh = mesh_params->out_mesh;

    Darray<GeometrySystem::GeometryConfig>* configs = &mesh_params->mesh_resource.g_configs;
    mesh->geometries.init(configs->count + mesh->pending_g_configs.count, 0);

    for (uint32 i = 0; i < configs->count; ++i) {
        Geometry** g = mesh->geometries.push(0);
        *g = GeometrySystem::acquire_from_config(&(*configs)[i], true);
    }

    for (uint32 i = 0; i < mesh->pending_g_configs.count; ++i) {
        Geometry** g = mesh->geometries.push(0);
        *g = GeometrySystem::acquire_from_config(&mesh->pending_g_configs[i], true);
    }

    mesh_params->out_mesh->generation++;
    mesh_params->out_mesh->state = MeshState::LOADED;

    SHMTRACEV("Successfully loaded mesh '%s'.", mesh->name.c_str());

    configs->free_data();
    mesh->pending_g_configs.free_data();

    ResourceSystem::mesh_loader_unload(&mesh_params->mesh_resource);
}

static void mesh_load_job_fail(void* params) {
    MeshLoadParams* mesh_params = (MeshLoadParams*)params;
    Mesh* mesh = mesh_params->out_mesh;

    SHMERRORV("Failed to load mesh '%s'.", mesh->resource_name.c_str());

    ResourceSystem::mesh_loader_unload(&mesh_params->mesh_resource);
}

static bool32 mesh_load_job_start(void* params, void* result_data) {
    MeshLoadParams* load_params = (MeshLoadParams*)params;
    Mesh* mesh = load_params->out_mesh;

    bool32 result;
    if (!load_params->is_reload && !(mesh->resource_name.is_empty()))
        result = ResourceSystem::mesh_loader_load(mesh->resource_name.c_str(), 0, &load_params->mesh_resource);
    else
        result = true;

    // NOTE: The load params are also used as the result data here, only the mesh_resource field is populated now.
    Memory::copy_memory(load_params, result_data, sizeof(MeshLoadParams));

    return result;
}

static bool32 mesh_load_async(Mesh* mesh, bool32 reload)
{
    mesh->generation = INVALID_ID8;

    JobSystem::JobInfo job = JobSystem::job_create(mesh_load_job_start, mesh_load_job_success, mesh_load_job_fail, sizeof(MeshLoadParams), sizeof(MeshLoadParams));
    MeshLoadParams* params = (MeshLoadParams*)job.params;
    params->out_mesh = mesh;
    params->is_reload = reload;
    params->mesh_resource = {};  
    JobSystem::submit(job);

    return true;
}