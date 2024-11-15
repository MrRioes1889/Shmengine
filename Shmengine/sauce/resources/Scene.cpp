#include "Scene.hpp"

#include "core/Logging.hpp"
#include "core/FrameData.hpp"
#include "utility/Math.hpp"
#include "resources/Mesh.hpp"
#include "resources/Skybox.hpp"
#include "renderer/Camera.hpp"
#include "systems/RenderViewSystem.hpp"
#include "systems/GeometrySystem.hpp"

// TODO: Meshes and Skybox states are atomatically coupled to scene state for now. Change to a system where scenes can reference resources without influencing their state directly.

static uint32 global_scene_id = 0;

bool32 scene_create(void* config, Scene* out_scene)
{

	*out_scene = {};

	out_scene->enabled = false;
	out_scene->state = SceneState::UNINITIALIZED;
	out_scene->transform = Math::transform_create();
	out_scene->id = global_scene_id++;

	return true;

}

bool32 scene_destroy(Scene* scene)
{

	if (scene->state != SceneState::UNLOADED || !scene_unload(scene))
		return false;

	if (scene->skybox)
		skybox_destroy(scene->skybox);

	for (uint32 i = 0; i < scene->meshes.count; i++)
		mesh_destroy(scene->meshes[i]);

	scene->dir_lights.free_data();
	scene->p_lights.free_data();
	scene->meshes.free_data();
	scene->skybox = 0;

	scene->geometries.free_data();

	scene->state = SceneState::DESTROYED;

	return true;

}

bool32 scene_init(Scene* scene)
{

	if (scene->state != SceneState::UNINITIALIZED)
		return false;

	scene->dir_lights.init(1, DarrayFlags::NON_RESIZABLE);
	scene->p_lights.init(10, DarrayFlags::NON_RESIZABLE);
	scene->meshes.init(1, 0);
	scene->skybox = 0;

	scene->geometries.init(512, 0);

	if (scene->skybox && scene->skybox->state == SkyboxState::UNINITIALIZED)
	{
		if (!skybox_init(scene->skybox))
		{
			SHMERROR("Failed to init skybox.");
			return false;
		}
	}

	for (uint32 i = 0; i < scene->meshes.count; i++)
	{
		if (scene->meshes[i]->state != MeshState::UNINITIALIZED)
			continue;

		if (!mesh_init(scene->meshes[i]))
		{
			SHMERROR("Failed to init mesh.");
			return false;
		}
	}

	scene->state = SceneState::INITIALIZED;

	return true;

}

bool32 scene_load(Scene* scene)
{

	if (scene->state != SceneState::INITIALIZED && scene->state != SceneState::UNLOADED)
		return false;

	scene->state = SceneState::LOADING;

	if (scene->skybox && (scene->skybox->state == SkyboxState::INITIALIZED || scene->skybox->state == SkyboxState::UNLOADED))
	{
		if (!skybox_load(scene->skybox))
		{
			SHMERROR("Failed to load skybox.");
			return false;
		}
	}

	for (uint32 i = 0; i < scene->meshes.count; i++)
	{
		if (scene->meshes[i]->state != MeshState::INITIALIZED && scene->meshes[i]->state != MeshState::UNLOADED)
			continue;

		if (!mesh_load(scene->meshes[i]))
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

	if (scene->skybox)
	{
		if (!skybox_unload(scene->skybox))
		{
			SHMERROR("Failed to unload skybox.");
			return false;
		}
	}

	for (uint32 i = 0; i < scene->meshes.count; i++)
	{
		if (!mesh_unload(scene->meshes[i]))
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

		if (scene->skybox && scene->skybox->state != SkyboxState::LOADED)
			objects_loaded = false;

		for (uint32 i = 0; i < scene->meshes.count; i++)
		{
			if (scene->meshes[i]->state != MeshState::LOADED)
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

	if (scene->skybox)
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
			skybox_data->skybox = scene->skybox;
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

	scene->geometries.clear();

	for (uint32 i = 0; i < scene->meshes.count; i++)
	{
		Mesh* m = scene->meshes[i];
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
				// TODO: Allocate via frame allocator instead
				Renderer::GeometryRenderData* render_data = scene->geometries.emplace();
				render_data->model = model;
				render_data->geometry = g;
				render_data->unique_id = m->unique_id;
			}
		}
	}

	if (!scene->geometries.count)
		return true;

	Renderer::WorldPacketData* world_packet = (Renderer::WorldPacketData*)frame_data->frame_allocator->allocate(sizeof(Renderer::WorldPacketData));
	world_packet->geometries = scene->geometries.data;
	world_packet->geometries_count = scene->geometries.count;
	world_packet->dir_light = scene->dir_lights.count > 0 ? &scene->dir_lights[0] : 0;
	world_packet->p_lights_count = scene->p_lights.count;
	world_packet->p_lights = scene->p_lights.data;

	frame_data->drawn_geometry_count += scene->geometries.count;

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

bool32 scene_add_mesh(Scene* scene, Mesh* mesh)
{

	scene->meshes.emplace(mesh);

	if (scene->state >= SceneState::INITIALIZED && mesh->state < MeshState::INITIALIZED)
	{
		if (!mesh_init(mesh))
		{
			SHMERROR("Failed to initialize mesh!");
			return false;
		}
	}

	if (scene->state == SceneState::LOADED && mesh->state != MeshState::LOADED)
	{
		if (!mesh_load(mesh))
		{
			SHMERROR("Failed to initialize mesh!");
			return false;
		}
	}

	return true;
}

bool32 scene_remove_mesh(Scene* scene, Mesh* mesh)
{
	for (uint32 i = 0; i < scene->meshes.count; i++)
	{
		if (scene->meshes[i] == mesh)
		{
			scene->meshes.remove_at(i);
			break;
		}		
	}

	return true;
}

bool32 scene_add_skybox(Scene* scene, Skybox* skybox)
{

	scene->skybox = skybox;

	if (scene->state >= SceneState::INITIALIZED && skybox->state < SkyboxState::INITIALIZED)
	{
		if (!skybox_init(skybox))
		{
			SHMERROR("Failed to initialize skybox!");
			return false;
		}
	}

	if (scene->state == SceneState::LOADED && skybox->state != SkyboxState::LOADED)
	{
		if (!skybox_load(skybox))
		{
			SHMERROR("Failed to initialize skybox!");
			return false;
		}
	}

	return true;
}

bool32 scene_remove_skybox(Scene* scene, Skybox* skybox)
{
	if (scene->skybox == skybox)
		scene->skybox = 0;

	return true;
}
