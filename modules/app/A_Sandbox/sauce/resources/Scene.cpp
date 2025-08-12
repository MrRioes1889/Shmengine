#include "Scene.hpp"

#include "loaders/SceneLoader.hpp"

#include <core/Logging.hpp>
#include <core/FrameData.hpp>
#include <utility/Math.hpp>
#include <utility/math/Geometry.hpp>
#include <resources/loaders/MeshLoader.hpp>
#include <resources/Mesh.hpp>
#include <resources/Terrain.hpp>
#include <resources/Skybox.hpp>
#include <resources/Box3D.hpp>
#include <renderer/Camera.hpp>
#include <renderer/RendererFrontend.hpp>
#include <renderer/Geometry.hpp>
#include <systems/RenderViewSystem.hpp>
#include <systems/GeometrySystem.hpp>
#include <systems/ShaderSystem.hpp>

static uint32 global_scene_id = 0;

bool8 scene_init(SceneConfig* config, Scene* out_scene)
{
	if (out_scene->state >= ResourceState::Initialized)
		return false;

	out_scene->state = ResourceState::Initializing;

	out_scene->name = config->name;
	out_scene->description = config->description;
	out_scene->enabled = false;

	out_scene->transform = config->transform;

	out_scene->dir_lights.init(1, DarrayFlags::NonResizable);
	out_scene->p_lights.init(config->max_p_lights_count, DarrayFlags::NonResizable);
	out_scene->p_light_boxes.init(config->max_p_lights_count, DarrayFlags::NonResizable);
	out_scene->meshes.init(config->max_meshes_count, DarrayFlags::NonResizable);
	out_scene->terrains.init(config->max_terrains_count, DarrayFlags::NonResizable);

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

bool8 scene_init_from_resource(const char* resource_name, Scene* out_scene)
{
	if (out_scene->state >= ResourceState::Initialized)
		return false;

	SceneResourceData resource = {};
	if (!ResourceSystem::scene_loader_load(resource_name, 0, &resource))
	{
		SHMERRORV("Failed to load scene resource %s.", resource_name);
		return false;
	}

	SceneConfig config = ResourceSystem::scene_loader_get_config_from_resource(&resource);

	bool8 success = scene_init(&config, out_scene);
	ResourceSystem::scene_loader_unload(&resource);
	return success;
}

bool8 scene_destroy(Scene* scene)
{

	if (scene->state != ResourceState::Initialized)
		return false;

	if (scene->skybox.state >= ResourceState::Initialized)
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

	scene->state = ResourceState::Destroyed;

	return true;

}

bool8 scene_update(Scene* scene)
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

	if (scene->state == ResourceState::Initializing)
	{
		bool8 objects_initialized = true;

		if (scene->skybox.state != ResourceState::Initialized)
			objects_initialized = false;

		for (uint32 i = 0; i < scene->meshes.count; i++)
		{
			if (scene->meshes[i].state != ResourceState::Initialized)
			{
				objects_initialized = false;
				break;
			}
		}

		for (uint32 i = 0; i < scene->terrains.count; i++)
		{
			if (scene->terrains[i].state != ResourceState::Initialized)
			{
				objects_initialized = false;
				break;
			}
		}

		for (uint32 i = 0; i < scene->p_light_boxes.count; i++)
		{
			if (scene->p_light_boxes[i].state != ResourceState::Initialized)
			{
				objects_initialized = false;
				break;
			}
		}

		if (objects_initialized)
			scene->state = ResourceState::Initialized;
	}

	return true;
}

bool8 scene_add_directional_light(Scene* scene, DirectionalLight light)
{
	if (scene->dir_lights.count == scene->dir_lights.capacity)
		return false;

	scene->dir_lights.emplace(light);

	return true;
}

//bool8 scene_remove_directional_light(Scene* scene, uint32 index)
//{
//	if (index > scene->dir_lights.count - 1)
//		return false;
//
//	scene->dir_lights.remove_at(index);
//	return true;
//}

bool8 scene_add_point_light(Scene* scene, PointLight light)
{
	if (scene->p_lights.count == scene->p_lights.capacity)
		return false;

	scene->p_lights.emplace(light);

	Box3D* light_box = &scene->p_light_boxes[scene->p_light_boxes.emplace()];

	if (!box3D_init({ 0.3f, 0.3f, 0.3f }, light.color, light_box))
	{
		SHMERROR("Failed to initialize light box!");
		return false;
	}

	return true;
}

//bool8 scene_remove_point_light(Scene* scene, uint32 index)
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

bool8 scene_add_mesh(Scene* scene, SceneMeshConfig* config)
{
	Mesh* mesh = &scene->meshes[scene->meshes.emplace()];

	bool8 initialized = false;
	switch (config->type)
	{
	case SceneMeshType::Resource:
	{
		initialized = mesh_init_from_resource(config->resource_name, mesh);		
		break;
	}
	case SceneMeshType::Cube:
	{
		GeometryResourceData geo_resource = {};
		Renderer::generate_cube_geometry(
			config->cube_config.dim.x,
			config->cube_config.dim.y,
			config->cube_config.dim.z,
			config->cube_config.tiling.x,
			config->cube_config.tiling.y,
			config->name,
			&geo_resource
		);

		MeshGeometryConfig mesh_geo_config = {};
		mesh_geo_config.geo_config = Renderer::geometry_get_config_from_resource(&geo_resource);
		mesh_geo_config.material_name = config->cube_config.material_name;

		MeshConfig mesh_config = {};
		mesh_config.g_configs = &mesh_geo_config;
		mesh_config.g_configs_count = 1;
		mesh_config.name = config->name;

		initialized = mesh_init(&mesh_config, mesh);
		geo_resource.indices.free_data();
		geo_resource.vertices.free_data();
		break;
	}
	}

	if (!initialized)
	{
		SHMERROR("Failed to initialize mesh for scene!");
		return false;
	}

	mesh->transform = config->transform;

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

//static bool8 scene_remove_mesh(Scene* scene, uint32 index)
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

//bool8 scene_remove_mesh(Scene* scene, const char* name)
//{
//
//	uint32 mesh_index = Constants::max_u32;
//	for (uint32 i = 0; i < scene->meshes.count; i++)
//	{
//		if (scene->meshes[i].name.equal_i(name))
//		{
//			mesh_index = i;
//			break;
//		}
//	}
//	
//	if (mesh_index == Constants::max_u32)
//		return false;
//
//	return scene_remove_mesh(scene, mesh_index);
//}

bool8 scene_add_terrain(Scene* scene, SceneTerrainConfig* config)
{
	Terrain* terrain = &scene->terrains[scene->terrains.emplace()];

	bool8 initialized = false;
	if (config->resource_name)
		initialized = terrain_init_from_resource(config->resource_name, terrain);		
	else
		initialized = terrain_init(&config->t_config, terrain);

	terrain->xform = config->xform;
	return initialized;
}

//bool8 scene_remove_terrain(Scene* scene, const char* name)
//{
//
//	uint32 terrain_index = Constants::max_u32;
//	for (uint32 i = 0; i < scene->terrains.count; i++)
//	{
//		if (scene->terrains[i].name.equal_i(name))
//		{
//			terrain_index = i;
//			break;
//		}
//	}
//
//	if (terrain_index == Constants::max_u32)
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

bool8 scene_add_skybox(Scene* scene, SkyboxConfig* config)
{
	if (scene->skybox.state >= ResourceState::Initialized)
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

	return true;
}

//bool8 scene_remove_skybox(Scene* scene, const char* name)
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

bool8 scene_draw(Scene* scene, const Math::Frustum* camera_frustum, FrameData* frame_data)
{
	if (scene->state != ResourceState::Initialized)
		return false;

	if (scene->skybox.state >= ResourceState::Initialized)
		frame_data->drawn_geometry_count += RenderViewSystem::skybox_draw(&scene->skybox, frame_data);

	LightingInfo lighting =
	{
		.dir_light = scene->dir_lights.count > 0 ? &scene->dir_lights[0] : 0,
		.p_lights_count = scene->p_lights.count,
		.p_lights = scene->p_lights.data
	};

	frame_data->drawn_geometry_count += RenderViewSystem::terrains_draw(scene->terrains.data, scene->terrains.count, lighting, frame_data);
	frame_data->drawn_geometry_count += RenderViewSystem::meshes_draw(scene->meshes.data, scene->meshes.count, lighting, frame_data, camera_frustum);
	frame_data->drawn_geometry_count += RenderViewSystem::boxes3D_draw(scene->p_light_boxes.data, scene->p_light_boxes.count, frame_data);

	return true;
}

Math::Ray3DHitInfo scene_raycast(Scene* scene, Math::Ray3D ray) 
{
	Math::Ray3DHitInfo hit_info = {};

	for (uint32 i = 0; i < scene->meshes.count; ++i) {
		Mesh* mesh = &scene->meshes[i];
		Math::Mat4 model = Math::transform_get_world(mesh->transform);
		float32 dist;
		if (Math::ray3D_cast_obb(mesh->extents, model, ray, &dist) && (hit_info.type == Math::Ray3DHitType::NONE || dist < hit_info.distance)) 
		{
			hit_info.distance = dist;
			hit_info.type = Math::Ray3DHitType::OBB;
			hit_info.position = ray.origin + (ray.direction * hit_info.distance);
			hit_info.unique_id = mesh->unique_id;
		}
	}

	return hit_info;
}
