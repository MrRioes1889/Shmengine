#include "RenderViewSystem.hpp"

#include "core/Logging.hpp"
#include "core/FrameData.hpp"
#include "renderer/Camera.hpp"
#include "utility/CString.hpp"
#include "containers/Hashtable.hpp"
#include "renderer/RendererFrontend.hpp"
#include "systems/TextureSystem.hpp"
#include "optick.h"

#include "resources/Mesh.hpp"
#include "resources/Skybox.hpp"
#include "resources/Terrain.hpp"
#include "resources/UIText.hpp"
#include "resources/Terrain.hpp"
#include "resources/Box3D.hpp"
#include "resources/Line3D.hpp"
#include "resources/Gizmo3D.hpp"

namespace RenderViewSystem
{

	struct SystemState
	{
		SystemConfig config;

		Sarray<RenderView*> registered_views;
		Hashtable<uint32> lookup_table;

	};

	static SystemState* system_state = 0;
	static void unregister(RenderView* view);

	bool32 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config)
	{

		SystemConfig* sys_config = (SystemConfig*)config;
		system_state = (SystemState*)allocator_callback(allocator, sizeof(SystemState));

		system_state->config = *sys_config;

		uint64 view_array_size = sizeof(RenderView*) * sys_config->max_view_count;
		void* view_array_data = allocator_callback(allocator, view_array_size);
		system_state->registered_views.init(sys_config->max_view_count, 0, AllocationTag::ARRAY, view_array_data);

		uint64 hashtable_data_size = sizeof(uint32) * sys_config->max_view_count;
		void* hashtable_data = allocator_callback(allocator, hashtable_data_size);
		system_state->lookup_table.init(sys_config->max_view_count, HashtableFlag::EXTERNAL_MEMORY, AllocationTag::UNKNOWN, hashtable_data);

		system_state->lookup_table.floodfill(Constants::max_u32);

		for (uint32 i = 0; i < sys_config->max_view_count; ++i)
			system_state->registered_views[i] = 0;

		return true;
	}

	void system_shutdown(void* state)
	{
		system_state->lookup_table.floodfill(Constants::max_u32);
		for (uint32 i = 0; i < system_state->config.max_view_count; i++)
		{
			if (system_state->registered_views[i])
			{
				unregister(system_state->registered_views[i]);
				system_state->registered_views[i] = 0;
			}			
		}

		system_state = 0;
	}

	bool32 register_view(RenderView* view, uint32 renderpass_count, Renderer::RenderPassConfig* renderpass_configs)
	{

		uint32 ref_id = system_state->lookup_table.get_value(view->name);
		if (ref_id != Constants::max_u32) {
			SHMERRORV("RenderViewSystem::create - A view named '%s' already exists or caused a hash table collision. A new one will not be created.", view->name);
			return false;
		}

		for (uint32 i = 0; i < system_state->config.max_view_count; ++i) {
			if (!system_state->registered_views[i]) {
				ref_id = i;
				break;
			}
		}

		if (ref_id == Constants::max_u32) {
			SHMERROR("RenderViewSystem::create - No available space for a new view. Change system config to account for more.");
			return false;
		}

		view->renderpasses.init(renderpass_count, 0, AllocationTag::RENDERER);
		for (uint32 pass_i = 0; pass_i < renderpass_count; pass_i++)
		{
			if (!Renderer::renderpass_create(&renderpass_configs[pass_i], &view->renderpasses[pass_i]))
			{
				SHMERROR("Failed to create pick renderpass!");
				return false;
			}
		}

		if (!view->on_register(view))
		{
			SHMERROR("Failed to perform view's on_register function.");
			return false;
		}
		view->geometries.init(1, 0, AllocationTag::RENDERER);
		view->instances.init(1, 0, AllocationTag::RENDERER);
		view->objects.init(1, 0, AllocationTag::RENDERER);

		system_state->lookup_table.set_value(view->name, ref_id);
		system_state->registered_views[ref_id] = view;

		regenerate_render_targets(view);

		return true;

	}

	static void unregister(RenderView* view)
	{
		view->on_unregister(view);

		for (uint32 pass_i = 0; pass_i < view->renderpasses.capacity; pass_i++)
			Renderer::renderpass_destroy(&view->renderpasses[pass_i]);

		view->objects.free_data();
		view->instances.free_data();
		view->geometries.free_data();
		
		view->internal_data.free_data();
		view->renderpasses.free_data();
	}

	RenderView* get(const char* name)
	{
		uint32 id = system_state->lookup_table.get_value(name);
		if (id == Constants::max_u32)
			return 0;

		return system_state->registered_views[id];
	}

	bool32 build_packet(RenderView* view, FrameData* frame_data, const RenderViewPacketData* packet_data)
	{
		OPTICK_EVENT();
		return view->on_build_packet(view, frame_data, packet_data);
	}

	void on_window_resize(uint32 width, uint32 height)
	{
		for (uint32 i = 0; i < system_state->config.max_view_count; ++i) {
			if (system_state->registered_views[i]) {
				system_state->registered_views[i]->on_resize(system_state->registered_views[i], width, height);
			}
		}
	}

	bool32 on_render(RenderView* view, FrameData* frame_data, uint32 frame_number, uint64 render_target_index)
	{
		OPTICK_EVENT();
		return view->on_render(view, frame_data, frame_number, render_target_index);
	}

	void on_end_frame(RenderView* view)
	{
		view->geometries.clear();
		view->instances.clear();
		view->objects.clear();
		view->on_end_frame(view);
	}

	void regenerate_render_targets(RenderView* view)
	{
		for (uint32 p = 0; p < view->renderpasses.capacity; p++)
		{
			Renderer::RenderPass* pass = &view->renderpasses[p];

			for (uint32 rt = 0; rt < pass->render_targets.capacity; rt++)
			{
				Renderer::RenderTarget* target = &pass->render_targets[rt];

				Renderer::render_target_destroy(target, false);

				for (uint32 att = 0; att < target->attachments.capacity; att++) 
				{
					Renderer::RenderTargetAttachment* attachment = &target->attachments[att];
					if (attachment->source == Renderer::RenderTargetAttachmentSource::DEFAULT) 
					{
						if (attachment->type == Renderer::RenderTargetAttachmentType::COLOR) 
						{
							attachment->texture = Renderer::get_window_attachment(rt);
						}
						else if (attachment->type == Renderer::RenderTargetAttachmentType::DEPTH) 
						{
							attachment->texture = Renderer::get_depth_attachment(rt);
						}
						else 
						{
							SHMFATAL("Unsupported attachment type.");
							continue;
						}
					}
					else if (attachment->source == Renderer::RenderTargetAttachmentSource::VIEW) 
					{
						if (!view->regenerate_attachment_target) 
						{
							SHMFATAL("View configured as source for an attachment whose view does not support this operation.");
							continue;
						}
	
						if (!view->regenerate_attachment_target(view, p, attachment)) 
						{
							SHMERROR("View failed to regenerate attachment target for attachment type.");
						}
					}
				}

				Renderer::render_target_create(
					target->attachments.capacity,
					target->attachments.data, 
					pass, 
					target->attachments[0].texture->width, 
					target->attachments[0].texture->height,
					&pass->render_targets[rt]);

			}

		}

	}

	static void material_get_instance_render_data(Material* material, FrameData* frame_data, uint32 shader_id, RenderViewInstanceData* out_instance_data)
	{
		out_instance_data->shader_id = shader_id;
		out_instance_data->instance_properties = material->properties;
		out_instance_data->texture_maps_count = material->maps.capacity;
		out_instance_data->texture_maps = (TextureMap**)frame_data->frame_allocator.allocate(sizeof(TextureMap*) * out_instance_data->texture_maps_count);
		for (uint32 i = 0; i < out_instance_data->texture_maps_count; i++)
			out_instance_data->texture_maps[i] = &material->maps[i];

		out_instance_data->shader_instance_id = material->shader_instance_id;
	}

	uint32 mesh_draw(RenderView* view, Mesh* mesh, uint32 shader_id, LightingInfo lighting, FrameData* frame_data, const Math::Frustum* frustum)
	{
		return meshes_draw(view, mesh, 1, shader_id, lighting, frame_data, frustum);
	}

	uint32 meshes_draw(RenderView* view, Mesh* meshes, uint32 mesh_count, uint32 shader_id, LightingInfo lighting, FrameData* frame_data, const Math::Frustum* frustum)
	{
		RenderViewPacketData packet_data = {};
		packet_data.geometries_pushed_count = 0;
		packet_data.instances_pushed_count = 0;
		packet_data.objects_pushed_count = 0;

		for (uint32 i = 0; i < mesh_count; i++)
		{
			Mesh* m = &meshes[i];
			if (m->generation == Constants::max_u8)
				continue;

			RenderViewObjectData* object_data = &view->objects[view->objects.emplace()];
			object_data->model = Math::transform_get_world(m->transform);
			object_data->unique_id = m->unique_id;
			object_data->lighting = lighting;
			packet_data.objects_pushed_count++;

			for (uint32 j = 0; j < m->geometries.count; j++)
			{
				MeshGeometry* g = &m->geometries[j];

				bool32 in_frustum = true;
				if (frustum)
				{
					Math::Vec3f extents_max = Math::vec_mul_mat(g->g_data->extents.max, object_data->model);
					Math::Vec3f center = Math::vec_mul_mat(g->g_data->center, object_data->model);
					Math::Vec3f half_extents = { Math::abs(extents_max.x - center.x), Math::abs(extents_max.y - center.y), Math::abs(extents_max.z - center.z) };

					in_frustum = Math::frustum_intersects_aabb(*frustum, center, half_extents);
				}

				if (in_frustum)
				{
					RenderViewGeometryData* geo_render_data = &view->geometries[view->geometries.emplace()];
					geo_render_data->object_index = view->objects.count - 1;
					geo_render_data->shader_instance_id = g->material->shader_instance_id;
					geo_render_data->shader_id = shader_id;
					geo_render_data->geometry_data = g->g_data;
					geo_render_data->has_transparency = (g->material->maps[0].texture->flags & TextureFlags::HAS_TRANSPARENCY);
					packet_data.geometries_pushed_count++;

					RenderViewInstanceData* inst_render_data = &view->instances[view->instances.emplace()];
					material_get_instance_render_data(g->material, frame_data, shader_id, inst_render_data);
					packet_data.instances_pushed_count++;
				}
			}
		}

		RenderViewSystem::build_packet(view, frame_data, &packet_data);

		return packet_data.geometries_pushed_count;
	}

	static void skybox_get_instance_render_data(Skybox* skybox, FrameData* frame_data, uint32 shader_id, RenderViewInstanceData* out_instance_data)
	{
		out_instance_data->shader_id = shader_id;
		out_instance_data->instance_properties = 0;
		out_instance_data->texture_maps_count = 1;
		out_instance_data->texture_maps = (TextureMap**)frame_data->frame_allocator.allocate(sizeof(TextureMap*) * out_instance_data->texture_maps_count);
		out_instance_data->texture_maps[0] = &skybox->cubemap;

		out_instance_data->shader_instance_id = skybox->shader_instance_id;
	}

	bool32 skybox_draw(RenderView* view, Skybox* skybox, uint32 shader_id, FrameData* frame_data)
	{
		RenderViewPacketData packet_data = {};
		packet_data.geometries_pushed_count = 0;
		packet_data.instances_pushed_count = 0;
		packet_data.objects_pushed_count = 0;

		RenderViewGeometryData* render_data = &view->geometries[view->geometries.emplace()];
		render_data->object_index = Constants::max_u32;
		render_data->shader_id = shader_id;
		render_data->shader_instance_id = skybox->shader_instance_id;
		render_data->geometry_data = skybox->geometry;
		render_data->has_transparency = 0;
		packet_data.geometries_pushed_count++;

		RenderViewInstanceData* inst_render_data = &view->instances[view->instances.emplace()];
		skybox_get_instance_render_data(skybox, frame_data, shader_id, inst_render_data);
		packet_data.instances_pushed_count++;

		return RenderViewSystem::build_packet(view, frame_data, &packet_data);
	}

	static void terrain_get_instance_render_data(Terrain* terrain, FrameData* frame_data, uint32 shader_id, RenderViewInstanceData* out_instance_data)
	{
		out_instance_data->shader_id = shader_id;
		out_instance_data->instance_properties = &terrain->material_properties;
		out_instance_data->texture_maps_count = terrain->materials.count * 3;
		out_instance_data->texture_maps = (TextureMap**)frame_data->frame_allocator.allocate(sizeof(TextureMap*) * out_instance_data->texture_maps_count);
		for (uint32 mat_i = 0; mat_i < terrain->materials.count; mat_i++)
		{
			out_instance_data->texture_maps[mat_i * 3] = &terrain->materials[mat_i].mat->maps[0];
			out_instance_data->texture_maps[mat_i * 3 + 1] = &terrain->materials[mat_i].mat->maps[1];
			out_instance_data->texture_maps[mat_i * 3 + 2] = &terrain->materials[mat_i].mat->maps[2];
		}
		out_instance_data->shader_instance_id = terrain->shader_instance_id;
	}

	uint32 terrain_draw(RenderView* view, Terrain* terrain, uint32 shader_id, LightingInfo lighting, FrameData* frame_data)
	{
		return terrains_draw(view, terrain, 1, shader_id, lighting, frame_data);
	}

	uint32 terrains_draw(RenderView* view, Terrain* terrains, uint32 terrains_count, uint32 shader_id, LightingInfo lighting, FrameData* frame_data)
	{
		RenderViewPacketData packet_data = {};
		packet_data.geometries_pushed_count = 0;
		packet_data.instances_pushed_count = 0;
		packet_data.objects_pushed_count = 0;

		for (uint32 i = 0; i < terrains_count; i++)
		{
			Terrain* t = &terrains[i];

			RenderViewObjectData* object_data = &view->objects[view->objects.emplace()];
			object_data->model = Math::transform_get_world(t->xform);
			object_data->unique_id = t->unique_id;
			object_data->lighting = lighting;
			packet_data.objects_pushed_count++;

			RenderViewGeometryData* geo_render_data = &view->geometries[view->geometries.emplace()];
			geo_render_data->object_index = view->objects.count - 1;
			geo_render_data->shader_instance_id = t->shader_instance_id;
			geo_render_data->shader_id = shader_id;
			geo_render_data->geometry_data = &t->geometry;
			geo_render_data->has_transparency = 0;
			packet_data.geometries_pushed_count++;

			RenderViewInstanceData* inst_render_data = &view->instances[view->instances.emplace()];
			terrain_get_instance_render_data(t, frame_data, shader_id, inst_render_data);
			packet_data.instances_pushed_count++;
		}

		RenderViewSystem::build_packet(view, frame_data, &packet_data);

		return packet_data.geometries_pushed_count;
	}

	static void ui_text_get_instance_render_data(UIText* text, FrameData* frame_data, uint32 shader_id, RenderViewInstanceData* out_instance_data)
	{
		out_instance_data->shader_id = shader_id;
		static Math::Vec4f white_color = { 1.0f, 1.0f, 1.0f, 1.0f };
		out_instance_data->instance_properties = &white_color;
		out_instance_data->texture_maps_count = 1;
		out_instance_data->texture_maps = (TextureMap**)frame_data->frame_allocator.allocate(sizeof(TextureMap*) * out_instance_data->texture_maps_count);
		out_instance_data->texture_maps[0] = &text->font_atlas->map;
		out_instance_data->shader_instance_id = text->shader_instance_id;
	}

	bool32 ui_text_draw(RenderView* view, UIText* text, uint32 shader_id, FrameData* frame_data)
	{
		RenderViewPacketData packet_data = {};
		packet_data.geometries_pushed_count = 0;
		packet_data.instances_pushed_count = 0;
		packet_data.objects_pushed_count = 0;

		RenderViewObjectData* object_data = &view->objects[view->objects.emplace()];
		object_data->model = Math::transform_get_world(text->transform);
		object_data->unique_id = text->unique_id;
		object_data->lighting = {};
		packet_data.objects_pushed_count++;

		RenderViewGeometryData* geo_render_data = &view->geometries[view->geometries.emplace()];
		geo_render_data->object_index = view->objects.count - 1;
		geo_render_data->shader_instance_id = text->shader_instance_id;
		geo_render_data->shader_id = shader_id;
		geo_render_data->geometry_data = &text->geometry;
		geo_render_data->has_transparency = 0;
		packet_data.geometries_pushed_count++;

		RenderViewInstanceData* inst_render_data = &view->instances[view->instances.emplace()];
		ui_text_get_instance_render_data(text, frame_data, shader_id, inst_render_data);
		packet_data.instances_pushed_count++;

		return RenderViewSystem::build_packet(view, frame_data, &packet_data);
	}

	uint32 box3D_draw(RenderView* view, Box3D* box, uint32 shader_id, FrameData* frame_data)
	{
		return boxes3D_draw(view, box, 1, shader_id, frame_data);
	}

	uint32 boxes3D_draw(RenderView* view, Box3D* boxes, uint32 boxes_count, uint32 shader_id, FrameData* frame_data)
	{
		RenderViewPacketData packet_data = {};
		packet_data.geometries_pushed_count = 0;
		packet_data.instances_pushed_count = 0;
		packet_data.objects_pushed_count = 0;

		for (uint32 i = 0; i < boxes_count; i++)
		{
			Box3D* box = &boxes[i];

			RenderViewObjectData* object_data = &view->objects[view->objects.emplace()];
			object_data->model = Math::transform_get_world(box->xform);
			object_data->unique_id = box->unique_id;
			object_data->lighting = {};
			packet_data.objects_pushed_count++;

			RenderViewGeometryData* geo_render_data = &view->geometries[view->geometries.emplace()];
			geo_render_data->object_index = view->objects.count - 1;
			geo_render_data->shader_instance_id = Constants::max_u32;
			geo_render_data->shader_id = shader_id;
			geo_render_data->geometry_data = &box->geometry;
			geo_render_data->has_transparency = 0;
			packet_data.geometries_pushed_count++;
		}

		RenderViewSystem::build_packet(view, frame_data, &packet_data);

		return packet_data.geometries_pushed_count;
	}

	uint32 line3D_draw(RenderView* view, Line3D* line, uint32 shader_id, FrameData* frame_data)
	{
		return lines3D_draw(view, line, 1, shader_id, frame_data);
	}

	uint32 lines3D_draw(RenderView* view, Line3D* lines, uint32 lines_count, uint32 shader_id, FrameData* frame_data)
	{
		RenderViewPacketData packet_data = {};
		packet_data.geometries_pushed_count = 0;
		packet_data.instances_pushed_count = 0;
		packet_data.objects_pushed_count = 0;

		for (uint32 i = 0; i < lines_count; i++)
		{
			Line3D* line = &lines[i];

			RenderViewObjectData* object_data = &view->objects[view->objects.emplace()];
			object_data->model = Math::transform_get_world(line->xform);
			object_data->unique_id = line->unique_id;
			object_data->lighting = {};
			packet_data.objects_pushed_count++;

			RenderViewGeometryData* geo_render_data = &view->geometries[view->geometries.emplace()];
			geo_render_data->object_index = view->objects.count - 1;
			geo_render_data->shader_instance_id = Constants::max_u32;
			geo_render_data->shader_id = shader_id;
			geo_render_data->geometry_data = &line->geometry;
			geo_render_data->has_transparency = 0;
			packet_data.geometries_pushed_count++;
		}

		RenderViewSystem::build_packet(view, frame_data, &packet_data);

		return packet_data.geometries_pushed_count;
	}

	uint32 gizmo3D_draw(RenderView* view, Gizmo3D* gizmo, uint32 shader_id, FrameData* frame_data, Camera* camera)
	{
		RenderViewPacketData packet_data = {};
		packet_data.geometries_pushed_count = 0;
		packet_data.instances_pushed_count = 0;
		packet_data.objects_pushed_count = 0;

		RenderViewObjectData* object_data = &view->objects[view->objects.emplace()];
		object_data->model = Math::transform_get_world(gizmo->xform);
		Math::Vec3f camera_pos = camera->get_position();
		Math::Vec3f gizmo_pos = gizmo->xform.position;
		// TODO: Should get this from the camera/viewport.
		float32 fov = Math::deg_to_rad(45.0f);
		float32 dist = Math::vec_distance(camera_pos, gizmo_pos);
		float32 fixed_size = 0.1f;  // TODO: Make this a configurable option for gizmo size.
		float32 scale_scalar = ((2.0f * Math::tan(fov * 0.5f)) * dist) * fixed_size;
		Math::Mat4 scale = Math::mat_scale({scale_scalar, scale_scalar, scale_scalar});
		object_data->model = Math::mat_mul(object_data->model, scale);

		object_data->unique_id = gizmo->unique_id;
		object_data->lighting = {};
		packet_data.objects_pushed_count++;

		RenderViewGeometryData* geo_render_data = &view->geometries[view->geometries.emplace()];
		geo_render_data->object_index = view->objects.count - 1;
		geo_render_data->shader_instance_id = Constants::max_u32;
		geo_render_data->shader_id = shader_id;
		geo_render_data->geometry_data = &gizmo->geometry;
		geo_render_data->has_transparency = 0;
		packet_data.geometries_pushed_count++;

		return RenderViewSystem::build_packet(view, frame_data, &packet_data);
	}
}