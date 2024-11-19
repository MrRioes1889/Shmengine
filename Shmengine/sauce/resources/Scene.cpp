#include "Scene.hpp"

#include "core/Logging.hpp"
#include "core/FrameData.hpp"
#include "utility/Math.hpp"
#include "resources/loaders/SceneLoader.hpp"
#include "resources/Mesh.hpp"
#include "resources/Skybox.hpp"
#include "renderer/Camera.hpp"
#include "systems/RenderViewSystem.hpp"
#include "systems/GeometrySystem.hpp"

static uint32 global_scene_id = 0;

bool32 scene_init(SceneConfig* config, Scene* out_scene)
{

	if (out_scene->state >= SceneState::INITIALIZED)
		return false;

	*out_scene = {};

	out_scene->name = config->name;
	out_scene->description = config->description;
	out_scene->enabled = false;

	out_scene->transform = config->transform;

	out_scene->dir_lights.init(1, DarrayFlags::NON_RESIZABLE);
	out_scene->p_lights.init(config->max_p_lights_count, DarrayFlags::NON_RESIZABLE);
	out_scene->meshes.init(config->max_meshes_count, DarrayFlags::NON_RESIZABLE);

	if (config->skybox_configs_count)
	{
		if (!scene_add_skybox(out_scene, &config->skybox_configs[0]))
		{
			SHMERROR("Failed to create skybox.");
			return false;
		}
	}
	
	if (config->dir_light_count)
	{
		for (uint32 i = 0; i < config->dir_light_count; i++)
		{
			if (!scene_add_directional_light(out_scene, config->dir_lights[i]))
			{
				SHMERROR("Failed to add directional light.");
				return false;
			}
		}		
	}

	if (config->point_light_count)
	{
		for (uint32 i = 0; i < config->point_light_count; i++)
		{
			if (!scene_add_point_light(out_scene, config->point_lights[i]))
			{
				SHMERROR("Failed to add point light.");
				return false;
			}
		}
	}

	if (config->mesh_configs_count)
	{
		for (uint32 i = 0; i < config->mesh_configs_count; i++)
		{

			MeshConfig* mesh_config = &config->mesh_configs[i];

			if (!scene_add_mesh(out_scene, mesh_config))
			{
				SHMERROR("Failed to create mesh.");
				return false;
			}

			Mesh* added_mesh = &out_scene->meshes[out_scene->meshes.count - 1];

			if (mesh_config->parent_name == 0)
			{
				added_mesh->transform.parent = &out_scene->transform;
				continue;
			}

			for (uint32 j = 0; j < out_scene->meshes.count; j++)
			{
				if (out_scene->meshes[j].name.equal(mesh_config->parent_name))
				{
					added_mesh->transform.parent = &out_scene->meshes[j].transform;
					break;
				}
			}
		}
	}

	out_scene->state = SceneState::INITIALIZED;	
	out_scene->id = global_scene_id++;

	return true;

}

bool32 scene_init_from_resource(const char* resource_name, Scene* out_scene)
{
	if (out_scene->state >= SceneState::INITIALIZED)
		return false;

	SceneResourceData resource = {};
	if (!ResourceSystem::scene_loader_load(resource_name, 0, &resource))
	{
		SHMERRORV("Failed to load scene resource %s.", resource_name);
		return false;
	}

	SceneConfig config = {};

	config.name = resource.name.c_str();
	config.description = resource.description.c_str();
	config.max_meshes_count = resource.max_meshes_count;
	config.max_p_lights_count = resource.max_p_lights_count;
	config.transform = resource.transform;

	Sarray<SkyboxConfig> skybox_configs(resource.skyboxes.capacity, 0);
	for (uint32 i = 0; i < resource.skyboxes.capacity; i++)
	{
		SkyboxConfig* sb_config = &skybox_configs[i];
		sb_config->name = resource.skyboxes[i].name.c_str();
		sb_config->cubemap_name = resource.skyboxes[i].cubemap_name.c_str();
	}
	
	Sarray<MeshConfig> mesh_configs(resource.meshes.capacity, 0);
	for (uint32 i = 0; i < resource.meshes.capacity; i++)
	{
		MeshConfig* m_config = &mesh_configs[i];
		m_config->name = resource.meshes[i].name.c_str();
		m_config->resource_name = resource.meshes[i].resource_name.c_str();
		m_config->parent_name = resource.meshes[i].parent_name.c_str();
		m_config->g_configs = &resource.meshes[i].g_configs;
		m_config->transform = resource.meshes[i].transform;
	}

	config.skybox_configs_count = skybox_configs.capacity;
	config.skybox_configs = skybox_configs.data;
	config.mesh_configs_count = mesh_configs.capacity;
	config.mesh_configs = mesh_configs.data;
	config.dir_light_count = resource.dir_lights.capacity;
	config.dir_lights = resource.dir_lights.data;
	config.point_light_count = resource.point_lights.capacity;
	config.point_lights = resource.point_lights.data;

	bool32 success = scene_init(&config, out_scene);

	ResourceSystem::scene_loader_unload(&resource);
	return success;

}

bool32 scene_destroy(Scene* scene)
{

	if (scene->state != SceneState::UNLOADED && !scene_unload(scene))
		return false;

	if (scene->skybox.state >= SkyboxState::INITIALIZED)
	{
		if (!skybox_destroy(&scene->skybox))
		{
			SHMERROR("Failed to destroy skybox.");
			return false;
		}
	}

	for (uint32 i = 0; i < scene->meshes.count; i++)
	{
		if (!mesh_destroy(&scene->meshes[i]))
		{
			SHMERROR("Failed to destroy skybox.");
			return false;
		}
	}

	scene->dir_lights.free_data();
	scene->p_lights.free_data();
	scene->meshes.free_data();

	scene->name.free_data();
	scene->description.free_data();

	scene->state = SceneState::DESTROYED;

	return true;

}

bool32 scene_load(Scene* scene)
{

	if (scene->state != SceneState::INITIALIZED && scene->state != SceneState::UNLOADED)
		return false;

	scene->state = SceneState::LOADING;

	if (scene->skybox.state >= SkyboxState::INITIALIZED)
	{
		if (!skybox_load(&scene->skybox))
		{
			SHMERROR("Failed to load skybox.");
			return false;
		}
	}

	for (uint32 i = 0; i < scene->meshes.count; i++)
	{
		if (!mesh_load(&scene->meshes[i]))
		{
			SHMERROR("Failed to load mesh.");
			return false;
		}
	}

	return true;

}

bool32 scene_unload(Scene* scene)
{

	if (scene->state <= SceneState::INITIALIZED)
		return true;
	else if (scene->state != SceneState::LOADED)
		return false;

	scene->state = SceneState::UNLOADING;

	if (scene->skybox.state >= SkyboxState::INITIALIZED)
	{
		if (!skybox_unload(&scene->skybox))
		{
			SHMERROR("Failed to unload skybox.");
			return false;
		}
	}

	for (uint32 i = 0; i < scene->meshes.count; i++)
	{
		if (!mesh_unload(&scene->meshes[i]))
		{
			SHMERROR("Failed to unload mesh.");
			return false;
		}
	}

	scene->state = SceneState::UNLOADED;

	return true;

}

bool32 scene_update(Scene* scene)
{

	if (scene->state == SceneState::LOADING)
	{
		bool32 objects_loaded = true;

		if (scene->skybox.state != SkyboxState::LOADED)
			objects_loaded = false;

		for (uint32 i = 0; i < scene->meshes.count; i++)
		{
			if (scene->meshes[i].state != MeshState::LOADED)
			{
				objects_loaded = false;
				break;
			}
		}

		if (objects_loaded)
			scene->state = SceneState::LOADED;
	}
	/*else if (scene->state == SceneState::UNLOADING)
	{
		bool32 objects_unloaded = true;

		if (scene->skybox && scene->skybox->state > SkyboxState::UNLOADED)
			objects_unloaded = false;

		for (uint32 i = 0; i < scene->meshes.count; i++)
		{
			if (scene->meshes[i]->state > MeshState::UNLOADED)
			{
				objects_unloaded = false;
				break;
			}
		}

		if (objects_unloaded)
			scene->state = SceneState::UNLOADED;
	}*/

	return true;
}

bool32 scene_build_render_packet(Scene* scene, const Math::Frustum* camera_frustum, FrameData* frame_data, Renderer::RenderPacket* packet)
{

	if (scene->state != SceneState::LOADED)
		return false;

	if (scene->skybox.state >= SkyboxState::INITIALIZED)
	{
		Renderer::RenderViewPacket* skybox_view_packet = 0;
		for (uint32 i = 0; i < packet->views.capacity; i++)
		{
			if (packet->views[i].view->type == Renderer::RenderViewType::SKYBOX)
			{
				skybox_view_packet = &packet->views[i];
				break;
			}
		}

		if (skybox_view_packet)
		{
			Renderer::SkyboxPacketData* skybox_data = (Renderer::SkyboxPacketData*)frame_data->frame_allocator->allocate(sizeof(Renderer::SkyboxPacketData));
			skybox_data->skybox = &scene->skybox;
			if (!RenderViewSystem::build_packet(RenderViewSystem::get("skybox"), frame_data->frame_allocator, skybox_data, skybox_view_packet))
			{
				SHMERROR("Failed to build packet for view 'skybox'.");
				return false;
			}
		}		
	}

	if (!scene->meshes.count)
		return true;

	Renderer::RenderViewPacket* world_view_packet = 0;
	for (uint32 i = 0; i < packet->views.capacity; i++)
	{
		if (packet->views[i].view->type == Renderer::RenderViewType::WORLD)
		{
			world_view_packet = &packet->views[i];
			break;
		}
	}

	if (!world_view_packet)
		return true;

	uint32 geometries_count = 0;
	Renderer::GeometryRenderData* geometries = 0;

	for (uint32 i = 0; i < scene->meshes.count; i++)
	{
		Mesh* m = &scene->meshes[i];
		if (m->generation == INVALID_ID8)
			continue;

		Math::Mat4 model = Math::transform_get_world(m->transform);
		for (uint32 j = 0; j < m->geometries.count; j++)
		{
			Geometry* g = m->geometries[j];
			Math::Vec3f extents_max = Math::vec_mul_mat(g->extents.max, model);

			Math::Vec3f center = Math::vec_mul_mat(g->center, model);
			Math::Vec3f half_extents = { Math::abs(extents_max.x - center.x), Math::abs(extents_max.y - center.y), Math::abs(extents_max.z - center.z) };

			if (Math::frustum_intersects_aabb(*camera_frustum, center, half_extents))
			{
				Renderer::GeometryRenderData* render_data = (Renderer::GeometryRenderData*)frame_data->frame_allocator->allocate(sizeof(Renderer::GeometryRenderData));
				render_data->model = model;
				render_data->geometry = g;
				render_data->unique_id = m->unique_id;

				geometries_count++;
				if (!geometries)
					geometries = render_data;
			}
		}
	}

	if (!geometries_count)
		return true;

	Renderer::WorldPacketData* world_packet = (Renderer::WorldPacketData*)frame_data->frame_allocator->allocate(sizeof(Renderer::WorldPacketData));
	world_packet->geometries = geometries;
	world_packet->geometries_count = geometries_count;
	world_packet->dir_light = scene->dir_lights.count > 0 ? &scene->dir_lights[0] : 0;
	world_packet->p_lights_count = scene->p_lights.count;
	world_packet->p_lights = scene->p_lights.data;

	frame_data->drawn_geometry_count += geometries_count;

	if (!RenderViewSystem::build_packet(RenderViewSystem::get("world"), frame_data->frame_allocator, world_packet, world_view_packet))
	{
		SHMERROR("Failed to build packet for view 'world'.");
		return false;
	}

	return true;
}

bool32 scene_add_directional_light(Scene* scene, DirectionalLight light)
{
	if (scene->dir_lights.count == scene->dir_lights.capacity)
		return false;

	scene->dir_lights.emplace(light);

	return true;
}

bool32 scene_remove_directional_light(Scene* scene, uint32 index)
{
	if (index > scene->dir_lights.count - 1)
		return false;

	scene->dir_lights.remove_at(index);
	return true;
}

bool32 scene_add_point_light(Scene* scene, PointLight light)
{
	if (scene->p_lights.count == scene->p_lights.capacity)
		return false;

	scene->p_lights.emplace(light);

	return true;
}

bool32 scene_remove_point_light(Scene* scene, uint32 index)
{
	if (index > scene->p_lights.count - 1)
		return false;

	scene->p_lights.remove_at(index);
	return true;
}

bool32 scene_add_mesh(Scene* scene, MeshConfig* config)
{

	Mesh* mesh = scene->meshes.emplace();

	if (!mesh_init(config, mesh))
	{
		SHMERROR("Failed to initialize mesh!");
		return false;
	}

	if (scene->state == SceneState::LOADED)
	{
		if (!mesh_load(mesh))
		{
			SHMERROR("Failed to initialize mesh!");
			return false;
		}
	}

	Mesh* added_mesh = &scene->meshes[scene->meshes.count - 1];

	if (config->parent_name == 0)
	{
		added_mesh->transform.parent = &scene->transform;
		return true;
	}

	for (uint32 j = 0; j < scene->meshes.count; j++)
	{
		if (scene->meshes[j].name.equal(config->parent_name))
		{
			added_mesh->transform.parent = &scene->meshes[j].transform;
			break;
		}
	}

	return true;
}

bool32 scene_remove_mesh(Scene* scene, const char* name)
{

	uint32 mesh_index = INVALID_ID;
	for (uint32 i = 0; i < scene->meshes.count; i++)
	{
		if (scene->meshes[i].name.equal_i(name))
		{
			mesh_index = i;
			break;
		}
	}
	
	if (mesh_index == INVALID_ID)
		return false;
	
	if (!mesh_destroy(&scene->meshes[mesh_index]))
	{
		SHMERROR("Failed to destroy old skybox!");
		return false;
	}

	scene->meshes.remove_at(mesh_index);

	return true;
}

bool32 scene_add_skybox(Scene* scene, SkyboxConfig* config)
{

	if (scene->skybox.state >= SkyboxState::INITIALIZED)
	{
		if (!skybox_destroy(&scene->skybox))
		{
			SHMERROR("Failed to destroy old skybox!");
			return false;
		}
	}

	if (!skybox_init(config, &scene->skybox))
	{
		SHMERROR("Failed to initialize skybox!");
		return false;
	}

	if (scene->state == SceneState::LOADED)
	{
		if (!skybox_load(&scene->skybox))
		{
			SHMERROR("Failed to initialize skybox!");
			return false;
		}
	}

	return true;
}

bool32 scene_remove_skybox(Scene* scene, const char* name)
{
	if (!scene->skybox.name.equal_i(name))
		return false;

	if (!skybox_destroy(&scene->skybox))
	{
		SHMERROR("Failed to destroy old skybox!");
		return false;
	}

	return true;
}

Skybox* scene_get_skybox(Scene* scene, const char* name)
{
	if (!scene->skybox.name.equal_i(name))
		return 0;

	return &scene->skybox;
}

Mesh* scene_get_mesh(Scene* scene, const char* name)
{
	for (uint32 i = 0; i < scene->meshes.count; i++)
	{
		if (scene->meshes[i].name.equal_i(name))
			return &scene->meshes[i];
	}

	return 0;
}

DirectionalLight* scene_get_dir_light(Scene* scene, uint32 index)
{
	if (index > scene->dir_lights.count - 1)
		return 0;

	return &scene->dir_lights[index];
}

PointLight* scene_get_point_light(Scene* scene, uint32 index)
{
	if (index > scene->p_lights.count - 1)
		return 0;

	return &scene->p_lights[index];
}
