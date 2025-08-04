#include "Mesh.hpp"

#include "core/Memory.hpp"
#include "core/Logging.hpp"
#include "core/Identifier.hpp"
#include "renderer/RendererFrontend.hpp"
#include "renderer/Geometry.hpp"
#include "resources/loaders/MeshLoader.hpp"

#include "systems/JobSystem.hpp"
#include "systems/GeometrySystem.hpp"
#include "systems/MaterialSystem.hpp"
#include "systems/ShaderSystem.hpp"

static void mesh_load_job_success(void* params);
static void mesh_load_job_fail(void* params);
static bool8 mesh_load_job_start(void* params, void* result_data);
static bool8 mesh_load_async(Mesh* mesh, bool8 reload);

bool8 mesh_init(MeshConfig* config, Mesh* out_mesh)
{
    if (out_mesh->state >= ResourceState::Initialized)
        return false;

    out_mesh->state = ResourceState::Initializing;

    out_mesh->name = config->name;
    out_mesh->extents = {};
    out_mesh->center = {};
    out_mesh->transform = Math::transform_create();
    out_mesh->geometries.init(config->g_configs_count, 0);
    for (uint32 i = 0; i < config->g_configs_count; i++)
    {
        MeshGeometry* g = &out_mesh->geometries[i];
        CString::copy(config->g_configs[i].material_name, g->material_name, Constants::max_material_name_length);
        g->g_id = GeometrySystem::create_geometry(config->g_configs[i].data_config, true);
        g->material = 0;

        GeometryData* g_data = GeometrySystem::get_geometry_data(g->g_id);
        if (g_data->extents.max.x > out_mesh->extents.max.x)
            out_mesh->extents.max.x = g_data->extents.max.x;
        if (g_data->extents.max.y > out_mesh->extents.max.y)
            out_mesh->extents.max.y = g_data->extents.max.y;
        if (g_data->extents.max.z > out_mesh->extents.max.z)
            out_mesh->extents.max.z = g_data->extents.max.z;

        if (g_data->extents.min.x < out_mesh->extents.min.x)
            out_mesh->extents.min.x = g_data->extents.min.x;
        if (g_data->extents.min.y < out_mesh->extents.min.y)
            out_mesh->extents.min.y = g_data->extents.min.y;
        if (g_data->extents.min.z < out_mesh->extents.min.z)
            out_mesh->extents.min.z = g_data->extents.min.z;
    }

    out_mesh->center = (out_mesh->extents.min + out_mesh->extents.max) * 0.5f;

    out_mesh->generation = Constants::max_u8;

    out_mesh->state = ResourceState::Initialized;

    return true;
}

bool8 mesh_init_from_resource(const char* resource_name, Mesh* out_mesh)
{

    out_mesh->state = ResourceState::Initializing;

    MeshResourceData resource = {};
    if (!ResourceSystem::mesh_loader_load(resource_name, &resource))
    {
        SHMERRORV("Failed to load mesh from resource '%s'", resource_name);
        out_mesh->state = ResourceState::Uninitialized;
        return false;
    }

    MeshConfig config = {};
    config.name = resource_name;

    Sarray<MeshGeometryConfig> g_configs(resource.geometries.count, 0);
    for (uint32 i = 0; i < resource.geometries.count; i++)
    {
        g_configs[i].material_name = resource.geometries[i].material_name;
        g_configs[i].data_config = &resource.geometries[i].data_config;
    }

    config.g_configs = g_configs.data;
    config.g_configs_count = g_configs.capacity;

    mesh_init(&config, out_mesh);
    g_configs.free_data();
    ResourceSystem::mesh_loader_unload(&resource);

    return true;

}

bool8 mesh_destroy(Mesh* mesh)
{
    if (mesh->state != ResourceState::Unloaded && !mesh_unload(mesh))
        return false;

    for (uint16 i = 0; i < mesh->geometries.capacity; i++)
        GeometrySystem::release(mesh->geometries[i].g_id);

    mesh->name.free_data();

    mesh->state = ResourceState::Destroyed;
    return true;
}

bool8 mesh_load(Mesh* mesh)
{

    if (mesh->state != ResourceState::Initialized && mesh->state != ResourceState::Unloaded)
        return false;

    bool8 is_reload = mesh->state == ResourceState::Unloaded;

    mesh->state = ResourceState::Loading;
    mesh->generation = Constants::max_u8;
    mesh->unique_id = identifier_acquire_new_id(mesh);

    mesh_load_async(mesh, is_reload);

    return true;

}

bool8 mesh_unload(Mesh* mesh)
{
    if (mesh->state <= ResourceState::Initialized)
        return true;
    else if (mesh->state != ResourceState::Loaded)
        return false;

    mesh->state = ResourceState::Unloading;

    mesh->generation = Constants::max_u8;
    identifier_release_id(mesh->unique_id);

    for (uint32 i = 0; i < mesh->geometries.capacity; ++i)
    {     
        GeometryData* g_data = GeometrySystem::get_geometry_data(mesh->geometries[i].g_id);
        Renderer::geometry_unload(g_data);
        if (mesh->geometries[i].material)
        {      
            MaterialSystem::release(mesh->geometries[i].material->name);
            mesh->geometries[i].material = 0;
        }
    }

    mesh->state = ResourceState::Unloaded;

    return true;

}

struct MeshLoadParams
{
	Mesh* out_mesh;
    bool8 is_reload;
};

static void mesh_load_job_success(void* params) 
{
    MeshLoadParams* mesh_params = (MeshLoadParams*)params;
    Mesh* mesh = mesh_params->out_mesh;  

    mesh_params->out_mesh->generation++;
    mesh_params->out_mesh->state = ResourceState::Loaded;

    SHMTRACEV("Successfully loaded mesh '%s'.", mesh->name.c_str());

}

static void mesh_load_job_fail(void* params) 
{
    MeshLoadParams* mesh_params = (MeshLoadParams*)params;
    Mesh* mesh = mesh_params->out_mesh;

    SHMERRORV("Failed to load mesh '%s'.", mesh->name.c_str());
}

static bool8 mesh_load_job_start(uint32 thread_index, void* user_data) 
{
    MeshLoadParams* load_params = (MeshLoadParams*)user_data;
    Mesh* mesh = load_params->out_mesh;

    for (uint32 i = 0; i < mesh->geometries.capacity; i++) 
    {
        MeshGeometry* g = &mesh->geometries[i];

        if (g->material_name[0])
            g->material = MaterialSystem::acquire(g->material_name, true);

        if (!g->material)
            g->material = MaterialSystem::get_default_material();
    }

    return true;
}

static bool8 mesh_load_async(Mesh* mesh, bool8 reload)
{
    mesh->generation = Constants::max_u8;

    JobSystem::JobInfo job = JobSystem::job_create(mesh_load_job_start, mesh_load_job_success, mesh_load_job_fail, sizeof(MeshLoadParams));
    MeshLoadParams* params = (MeshLoadParams*)job.user_data;
    params->out_mesh = mesh;
    params->is_reload = reload;
    JobSystem::submit(job);

    return true;
}