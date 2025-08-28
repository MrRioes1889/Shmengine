#include "renderer/RendererFrontend.hpp"
#include "core/Memory.hpp"
#include "core/Logging.hpp"
#include "core/Identifier.hpp"
#include "resources/loaders/MeshLoader.hpp"
#include "utility/math/Transform.hpp"

#include "systems/JobSystem.hpp"
#include "systems/MaterialSystem.hpp"
#include "systems/ShaderSystem.hpp"

namespace Renderer
{
	static bool8 _mesh_init(MeshConfig* config, Mesh* out_mesh);
	static void _mesh_destroy(Mesh* mesh);

	static void _mesh_init_from_resource_job_success(void* params);
	static void _mesh_init_from_resource_job_fail(void* params);
	static bool8 _mesh_init_from_resource_job(uint32 thread_index, void* user_data);

	bool8 mesh_init(MeshConfig* config, Mesh* out_mesh)
	{
		if (out_mesh->state >= ResourceState::Initialized)
			return false;

		out_mesh->state = ResourceState::Initializing;
		_mesh_init(config, out_mesh);
		for (uint32 i = 0; i < out_mesh->geometries.capacity; i++)
		{
			MeshGeometry* g = &out_mesh->geometries[i];
			Renderer::geometry_load(&g->geometry_data);
			const char* material_name = config->g_configs[i].material_name;

			Material* material;
			g->material_id = MaterialSystem::acquire_material_id(material_name, &material);
			if (material)
				Renderer::material_init_from_resource_async(material_name, material);
		}

		out_mesh->state = ResourceState::Initialized;

		return true;
	}

	struct MeshLoadParams
	{
		Mesh* out_mesh;
		const char* resource_name;
		MeshResourceData resource;
	};

	bool8 mesh_init_from_resource_async(const char* resource_name, Mesh* out_mesh)
	{
		if (out_mesh->state >= ResourceState::Initialized)
			return false;

		out_mesh->state = ResourceState::Initializing;

		JobSystem::JobInfo job = JobSystem::job_create(_mesh_init_from_resource_job, _mesh_init_from_resource_job_success, _mesh_init_from_resource_job_fail, sizeof(MeshLoadParams));
		MeshLoadParams* params = (MeshLoadParams*)job.user_data;
		params->out_mesh = out_mesh;
		params->resource_name = resource_name;
		params->resource = {};
		JobSystem::submit(job);

		return true;
	}

	bool8 mesh_destroy(Mesh* mesh)
	{
		if (mesh->state != ResourceState::Initialized)
			return false;

		mesh->state = ResourceState::Destroying;
		_mesh_destroy(mesh);
		mesh->state = ResourceState::Destroyed;
		return true;
	}

	static bool8 _mesh_init(MeshConfig* config, Mesh* out_mesh)
	{
		out_mesh->name = config->name;
		out_mesh->extents = {};
		out_mesh->center = {};
		out_mesh->transform = Math::transform_create();
		out_mesh->geometries.init(config->g_configs_count, 0);
		for (uint32 i = 0; i < config->g_configs_count; i++)
		{
			MeshGeometry* g = &out_mesh->geometries[i];
			Renderer::geometry_init(&config->g_configs[i].geo_config, &g->geometry_data);
			g->material_id.invalidate();

			GeometryData* g_data = &g->geometry_data;
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

		out_mesh->unique_id = identifier_acquire_new_id(out_mesh);
		return true;
	}

	static void _mesh_destroy(Mesh* mesh)
	{
		identifier_release_id(mesh->unique_id);

		for (uint32 i = 0; i < mesh->geometries.capacity; ++i)
		{     
			GeometryData* g_data = &mesh->geometries[i].geometry_data;
			Renderer::geometry_unload(g_data);
			Material* material = MaterialSystem::get_material(mesh->geometries[i].material_id);
			if (material)
				MaterialSystem::release_material_id(material->name, &material);
			if (material)
				Renderer::material_destroy(material);

			mesh->geometries[i].material_id.invalidate();
		}

		mesh->name.free_data();
	}

	static void _mesh_init_from_resource_job_success(void* params) 
	{
		MeshLoadParams* load_params = (MeshLoadParams*)params;
		Mesh* mesh = load_params->out_mesh;  

		MeshConfig config = ResourceSystem::mesh_loader_get_config_from_resource(load_params->resource_name, &load_params->resource);
		for (uint32 i = 0; i < mesh->geometries.capacity; i++)
		{
			MeshGeometry* g = &mesh->geometries[i];
			Renderer::geometry_load(&g->geometry_data);
			const char* material_name = config.g_configs[i].material_name;

			Material* material;
			g->material_id = MaterialSystem::acquire_material_id(material_name, &material);
			if (material)
				Renderer::material_init_from_resource_async(material_name, material);
		}

		load_params->out_mesh->state = ResourceState::Initialized;

		ResourceSystem::mesh_loader_unload(&load_params->resource);
		SHMTRACEV("Successfully loaded mesh '%s'.", mesh->name.c_str());
	}

	static void _mesh_init_from_resource_job_fail(void* params) 
	{
		MeshLoadParams* load_params = (MeshLoadParams*)params;
		Mesh* mesh = load_params->out_mesh;

		ResourceSystem::mesh_loader_unload(&load_params->resource);
		_mesh_destroy(mesh);
		mesh->state = ResourceState::Destroyed;
		SHMERRORV("Failed to load mesh '%s'.", mesh->name.c_str());
	}

	static bool8 _mesh_init_from_resource_job(uint32 thread_index, void* user_data) 
	{
		MeshLoadParams* load_params = (MeshLoadParams*)user_data;
		Mesh* mesh = load_params->out_mesh;

		if (!ResourceSystem::mesh_loader_load(load_params->resource_name, &load_params->resource))
		{
			SHMERRORV("Failed to load mesh from resource '%s'", load_params->resource_name);
			return false;
		}

		MeshConfig config = ResourceSystem::mesh_loader_get_config_from_resource(load_params->resource_name, &load_params->resource);
		return _mesh_init(&config, mesh);
	}
}