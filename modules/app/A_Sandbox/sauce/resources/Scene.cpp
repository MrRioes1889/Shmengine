#include "Scene.hpp"

#include "loaders/SceneLoader.hpp"

#include <core/Logging.hpp>
#include <core/FrameData.hpp>
#include <utility/Math.hpp>
#include <resources/loaders/MeshLoader.hpp>
#include <resources/Mesh.hpp>
#include <resources/Terrain.hpp>
#include <resources/Skybox.hpp>
#include <resources/Box3D.hpp>
#include <renderer/Camera.hpp>
#include <renderer//RendererFrontend.hpp>
#include <systems/RenderViewSystem.hpp>
#include <systems/GeometrySystem.hpp>
#include <systems/ShaderSystem.hpp>

static uint32 global_scene_id = 0;

bool32 scene_init(SceneConfig* config, Scene* out_scene)
{

	if (out_scene->state >= ResourceState::INITIALIZED)
		return false;

	out_scene->state = ResourceState::INITIALIZING;

	out_scene->name = config->name;
	out_scene->description = config->description;
	out_scene->enabled = false;

	out_scene->transform = config->transform;

	out_scene->dir_lights.init(1, DarrayFlags::NON_RESIZABLE);
	out_scene->p_lights.init(config->max_p_lights_count, DarrayFlags::NON_RESIZABLE);
	out_scene->p_light_boxes.init(config->max_p_lights_count, DarrayFlags::NON_RESIZABLE);
	out_scene->meshes.init(config->max_meshes_count, DarrayFlags::NON_RESIZABLE);
	out_scene->terrains.init(config->max_terrains_count, DarrayFlags::NON_RESIZABLE);

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

	if (config->terrain_configs_count)
	{
		for (uint32 i = 0; i < config->terrain_configs_count; i++)
		{

			SceneTerrainConfig* terrain_config = &config->terrain_configs[i];

			if (!scene_add_terrain(out_scene, terrain_config))
			{
				SHMERROR("Failed to create terrain.");
				return false;
			}

			Terrain* added_terrain = &out_scene->terrains[out_scene->terrains.count - 1];
		}
	}

	if (config->mesh_configs_count)
	{
		for (uint32 i = 0; i < config->mesh_configs_count; i++)
		{

			SceneMeshConfig* mesh_config = &config->mesh_configs[i];

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

	out_scene->id = global_scene_id++;
	scene_update(out_scene);

	return true;

}

bool32 scene_init_from_resource(const char* resource_name, Scene* out_scene)
{
	if (out_scene->state >= ResourceState::INITIALIZED)
		return false;

	out_scene->state = ResourceState::INITIALIZING;

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
	config.max_terrains_count = resource.max_terrains_count;
	config.max_p_lights_count = resource.max_p_lights_count;
	config.transform = resource.transform;

	Sarray<SkyboxConfig> skybox_configs(resource.skyboxes.capacity, 0);
	for (uint32 i = 0; i < resource.skyboxes.capacity; i++)
	{
		SkyboxConfig* sb_config = &skybox_configs[i];
		sb_config->name = resource.skyboxes[i].name.c_str();
		sb_config->cubemap_name = resource.skyboxes[i].cubemap_name.c_str();
	}
	
	Sarray<SceneMeshConfig> mesh_configs(resource.meshes.capacity, 0);

	uint32 mesh_geometry_count = 0;
	for (uint32 mesh_i = 0; mesh_i < resource.meshes.capacity; mesh_i++)
	{
		if (!resource.meshes[mesh_i].resource_name.is_empty())
			continue;

		mesh_geometry_count += resource.meshes[mesh_i].geometries.count;
	}
	
	Sarray<MeshGeometryConfig> mesh_geometry_configs(mesh_geometry_count, 0);
	uint32 mesh_geometry_configs_i = 0;

	for (uint32 mesh_i = 0; mesh_i < resource.meshes.capacity; mesh_i++)
	{
		SceneMeshConfig* m_config = &mesh_configs[mesh_i];

		m_config->parent_name = resource.meshes[mesh_i].parent_name.c_str();
		m_config->transform = resource.meshes[mesh_i].transform;
		if (!resource.meshes[mesh_i].resource_name.is_empty())
		{
			m_config->resource_name = resource.meshes[mesh_i].resource_name.c_str();
			continue;
		}

		for (uint32 geo_i = 0; geo_i < resource.meshes[mesh_i].geometries.count; geo_i++)
		{
			mesh_geometry_configs[mesh_geometry_configs_i].material_name = resource.meshes[mesh_i].geometries[geo_i].material_name;
			mesh_geometry_configs[mesh_geometry_configs_i].data_config = &resource.meshes[mesh_i].geometries[geo_i].data_config;
		}		

		m_config->m_config.name = resource.meshes[mesh_i].name.c_str();
		m_config->m_config.g_configs_count = resource.meshes[mesh_i].geometries.count;
		m_config->m_config.g_configs = &mesh_geometry_configs[mesh_geometry_configs_i];

		mesh_geometry_configs_i += resource.meshes[mesh_i].geometries.count;
	}

	Sarray<SceneTerrainConfig> terrain_configs(resource.terrains.capacity, 0);
	for (uint32 i = 0; i < resource.terrains.capacity; i++)
	{
		SceneTerrainConfig* t_config = &terrain_configs[i];
		t_config->t_config.name = resource.terrains[i].name.c_str();
		t_config->resource_name = resource.terrains[i].resource_name.c_str();
		t_config->xform = resource.terrains[i].xform;
	}

	config.skybox_configs_count = skybox_configs.capacity;
	config.skybox_configs = skybox_configs.data;
	config.mesh_configs_count = mesh_configs.capacity;
	config.mesh_configs = mesh_configs.data;
	config.terrain_configs_count = terrain_configs.capacity;
	config.terrain_configs = terrain_configs.data;
	config.dir_light_count = resource.dir_lights.capacity;
	config.dir_lights = resource.dir_lights.data;
	config.point_light_count = resource.point_lights.capacity;
	config.point_lights = resource.point_lights.data;

	bool32 success = scene_init(&config, out_scene);
	skybox_configs.free_data();
	mesh_configs.free_data();
	mesh_geometry_configs.free_data();
	terrain_configs.free_data();
	ResourceSystem::scene_loader_unload(&resource);
	return success;

}

bool32 scene_destroy(Scene* scene)
{

	if (scene->state != ResourceState::UNLOADED && !scene_unload(scene))
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
			SHMERROR("Failed to destroy mesh.");
			return false;
		}
	}

	for (uint32 i = 0; i < scene->terrains.count; i++)
	{
		if (!terrain_destroy(&scene->terrains[i]))
		{
			SHMERROR("Failed to destroy terrain.");
			return false;
		}
	}

	scene->dir_lights.free_data();
	scene->p_lights.free_data();
	scene->meshes.free_data();
	scene->terrains.free_data();

	scene->name.free_data();
	scene->description.free_data();

	scene->state = ResourceState::DESTROYED;

	return true;

}

bool32 scene_load(Scene* scene)
{

	if (scene->state != ResourceState::INITIALIZED && scene->state != ResourceState::UNLOADED)
		return false;

	scene->state = ResourceState::LOADING;

	if (scene->skybox.state >= SkyboxState::INITIALIZED)
	{
		if (!skybox_load(&scene->skybox))
		{
			SHMERROR("Failed to load skybox.");
			return false;
		}
	}

	for (uint32 i = 0; i < scene->terrains.count; i++)
	{
		if (!terrain_load(&scene->terrains[i]))
		{
			SHMERROR("Failed to load terrain.");
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

	for (uint32 i = 0; i < scene->p_light_boxes.count; i++)
	{
		if (!box3D_load(&scene->p_light_boxes[i]))
		{
			SHMERROR("Failed to load light box.");
			return false;
		}
	}

	return true;

}

bool32 scene_unload(Scene* scene)
{

	if (scene->state <= ResourceState::INITIALIZED)
		return true;
	else if (scene->state != ResourceState::LOADED)
		return false;

	scene->state = ResourceState::UNLOADING;

	for (uint32 i = 0; i < scene->meshes.count; i++)
	{
		if (!mesh_unload(&scene->meshes[i]))
		{
			SHMERROR("Failed to unload mesh.");
			return false;
		}
	}

	for (uint32 i = 0; i < scene->terrains.count; i++)
	{
		if (!terrain_unload(&scene->terrains[i]))
		{
			SHMERROR("Failed to unload terrain.");
			return false;
		}
	}

	if (scene->skybox.state >= SkyboxState::INITIALIZED)
	{
		if (!skybox_unload(&scene->skybox))
		{
			SHMERROR("Failed to unload skybox.");
			return false;
		}
	}

	for (uint32 i = 0; i < scene->p_light_boxes.count; i++)
	{
		if (!box3D_unload(&scene->p_light_boxes[i]))
		{
			SHMERROR("Failed to unload light box.");
			return false;
		}
	}

	scene->state = ResourceState::UNLOADED;

	return true;

}

bool32 scene_update(Scene* scene)
{

	for (uint32 i = 0; i < scene->terrains.count; i++)
		terrain_update(&scene->terrains[i]);

	for (uint32 i = 0; i < scene->p_light_boxes.count; i++)
	{
		Math::Vec3f new_pos = { scene->p_lights[i].position.x, scene->p_lights[i].position.y, scene->p_lights[i].position.z };
		Math::transform_set_position(scene->p_light_boxes[i].xform, new_pos);
		box3D_set_color(&scene->p_light_boxes[i], scene->p_lights[i].color);
		box3D_update(&scene->p_light_boxes[i]);
	}	

	if (scene->state == ResourceState::INITIALIZING)
	{
		bool32 objects_initialized = true;

		if (scene->skybox.state != SkyboxState::INITIALIZED)
			objects_initialized = false;

		for (uint32 i = 0; i < scene->meshes.count; i++)
		{
			if (scene->meshes[i].state != MeshState::INITIALIZED)
			{
				objects_initialized = false;
				break;
			}
		}

		for (uint32 i = 0; i < scene->terrains.count; i++)
		{
			if (scene->terrains[i].state != TerrainState::INITIALIZED)
			{
				objects_initialized = false;
				break;
			}
		}

		for (uint32 i = 0; i < scene->p_light_boxes.count; i++)
		{
			if (scene->p_light_boxes[i].state != ResourceState::INITIALIZED)
			{
				objects_initialized = false;
				break;
			}
		}

		if (objects_initialized)
			scene->state = ResourceState::INITIALIZED;
	}
	else if (scene->state == ResourceState::LOADING)
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

		for (uint32 i = 0; i < scene->terrains.count; i++)
		{
			if (scene->terrains[i].state != TerrainState::LOADED)
			{
				objects_loaded = false;
				break;
			}
		}

		for (uint32 i = 0; i < scene->p_light_boxes.count; i++)
		{
			if (scene->p_light_boxes[i].state != ResourceState::LOADED)
			{
				objects_loaded = false;
				break;
			}
		}

		if (objects_loaded)
			scene->state = ResourceState::LOADED;
	}
	/*else if (scene->state == ResourceState::UNLOADING)
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
			scene->state = ResourceState::UNLOADED;
	}*/

	return true;
}

bool32 scene_add_directional_light(Scene* scene, DirectionalLight light)
{
	if (scene->dir_lights.count == scene->dir_lights.capacity)
		return false;

	scene->dir_lights.emplace(light);

	return true;
}

//bool32 scene_remove_directional_light(Scene* scene, uint32 index)
//{
//	if (index > scene->dir_lights.count - 1)
//		return false;
//
//	scene->dir_lights.remove_at(index);
//	return true;
//}

bool32 scene_add_point_light(Scene* scene, PointLight light)
{
	if (scene->p_lights.count == scene->p_lights.capacity)
		return false;

	scene->p_lights.emplace(light);

	Box3D* light_box = &scene->p_light_boxes[scene->p_light_boxes.emplace()];

	if (!box3D_init("", { 0.3f, 0.3f, 0.3f }, light.color, light_box))
	{
		SHMERROR("Failed to initialize light box!");
		return false;
	}

	if (scene->state == ResourceState::LOADED)
	{
		if (!box3D_load(light_box))
		{
			SHMERROR("Failed to initialize light box!");
			return false;
		}
	}

	return true;
}

//bool32 scene_remove_point_light(Scene* scene, uint32 index)
//{
//	if (index > scene->p_lights.count - 1)
//		return false;
//
//	scene->p_lights.remove_at(index);
//
//	if (!box3D_destroy(&scene->p_light_boxes[index]))
//	{
//		SHMERROR("Failed to destroy light box!");
//		return false;
//	}
//
//	return true;
//}

bool32 scene_add_mesh(Scene* scene, SceneMeshConfig* config)
{

	Mesh* mesh = &scene->meshes[scene->meshes.emplace()];

	bool32 initialized = false;
	if (config->resource_name)
		initialized = mesh_init_from_resource(config->resource_name, mesh);		
	else
		initialized = mesh_init(&config->m_config, mesh);

	if (!initialized)
	{
		SHMERROR("Failed to initialize mesh!");
		return false;
	}

	mesh->transform = config->transform;

	if (scene->state == ResourceState::LOADED)
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

//static bool32 scene_remove_mesh(Scene* scene, uint32 index)
//{
//	for (uint32 i = 0; i < scene->meshes.count; i++)
//	{
//		if (scene->meshes[i].transform.parent == &scene->meshes[index].transform)
//		{
//			SHMERROR("Failed to destroy mesh due to existing children!");
//			return false;
//		}		
//	}
//
//	if (!mesh_destroy(&scene->meshes[index]))
//	{
//		SHMERROR("Failed to destroy mesh!");
//		return false;
//	}
//
//	scene->meshes.remove_at(index);
//
//	return true;
//}

//bool32 scene_remove_mesh(Scene* scene, const char* name)
//{
//
//	uint32 mesh_index = INVALID_ID;
//	for (uint32 i = 0; i < scene->meshes.count; i++)
//	{
//		if (scene->meshes[i].name.equal_i(name))
//		{
//			mesh_index = i;
//			break;
//		}
//	}
//	
//	if (mesh_index == INVALID_ID)
//		return false;
//
//	return scene_remove_mesh(scene, mesh_index);
//}

bool32 scene_add_terrain(Scene* scene, SceneTerrainConfig* config)
{

	Terrain* terrain = &scene->terrains[scene->terrains.emplace()];

	bool32 initialized = false;
	if (config->resource_name)
	{
		initialized = terrain_init_from_resource(config->resource_name, terrain);		
	}		
	else
	{
		initialized = terrain_init(&config->t_config, terrain);
	}		

	if (!initialized)
	{
		SHMERROR("Failed to initialize terrain!");
		return false;
	}

	terrain->xform = config->xform;

	if (scene->state == ResourceState::LOADED)
	{
		if (!terrain_load(terrain))
		{
			SHMERROR("Failed to initialize terrain!");
			return false;
		}
	}

	return true;
}

//bool32 scene_remove_terrain(Scene* scene, const char* name)
//{
//
//	uint32 terrain_index = INVALID_ID;
//	for (uint32 i = 0; i < scene->terrains.count; i++)
//	{
//		if (scene->terrains[i].name.equal_i(name))
//		{
//			terrain_index = i;
//			break;
//		}
//	}
//
//	if (terrain_index == INVALID_ID)
//		return false;
//
//	if (!terrain_destroy(&scene->terrains[terrain_index]))
//	{
//		SHMERROR("Failed to destroy terrain!");
//		return false;
//	}
//
//	scene->terrains.remove_at(terrain_index);
//
//	return true;
//}

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

	if (scene->state == ResourceState::LOADED)
	{
		if (!skybox_load(&scene->skybox))
		{
			SHMERROR("Failed to initialize skybox!");
			return false;
		}
	}

	return true;
}

//bool32 scene_remove_skybox(Scene* scene, const char* name)
//{
//	if (!scene->skybox.name.equal_i(name))
//		return false;
//
//	if (!skybox_destroy(&scene->skybox))
//	{
//		SHMERROR("Failed to destroy old skybox!");
//		return false;
//	}
//
//	return true;
//}

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

Terrain* scene_get_terrain(Scene* scene, const char* name)
{
	for (uint32 i = 0; i < scene->terrains.count; i++)
	{
		if (scene->terrains[i].name.equal_i(name))
			return &scene->terrains[i];
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

bool32 scene_draw(Scene* scene, RenderView* skybox_view, RenderView* world_view, const Math::Frustum* camera_frustum, FrameData* frame_data)
{

	if (scene->state != ResourceState::LOADED)
		return false;

	uint32 skybox_shader_id = ShaderSystem::get_skybox_shader_id();

	if (scene->skybox.state >= SkyboxState::INITIALIZED)
		frame_data->drawn_geometry_count += Renderer::skybox_draw(&scene->skybox, skybox_view, 0, skybox_shader_id, frame_data);

	LightingInfo lighting =
	{
		.dir_light = scene->dir_lights.count > 0 ? &scene->dir_lights[0] : 0,
		.p_lights_count = scene->p_lights.count,
		.p_lights = scene->p_lights.data
	};

	uint32 terrain_shader_id = ShaderSystem::get_terrain_shader_id();
	frame_data->drawn_geometry_count += Renderer::terrains_draw(scene->terrains.data, scene->terrains.count, world_view, 0, terrain_shader_id, lighting, frame_data);

	uint32 material_shader_id = ShaderSystem::get_material_shader_id();
	frame_data->drawn_geometry_count += Renderer::meshes_draw(scene->meshes.data, scene->meshes.count, world_view, 0, material_shader_id, lighting, frame_data, camera_frustum);

	uint32 color3D_shader_id = ShaderSystem::get_color3D_shader_id();
	frame_data->drawn_geometry_count += Renderer::boxes3D_draw(scene->p_light_boxes.data, scene->p_light_boxes.count, world_view, 0, color3D_shader_id, frame_data);

	return true;
}
