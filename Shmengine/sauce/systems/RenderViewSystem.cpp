#include "RenderViewSystem.hpp"

#include "core/Logging.hpp"
#include "core/FrameData.hpp"
#include "core/Event.hpp"
#include "core/Engine.hpp"
#include "platform/Platform.hpp"
#include "renderer/Camera.hpp"
#include "utility/CString.hpp"
#include "containers/Hashtable.hpp"
#include "renderer/RendererFrontend.hpp"
#include "systems/TextureSystem.hpp"
#include "optick.h"

#include "renderer/views/RenderViewSkybox.hpp"
#include "renderer/views/RenderViewWorld.hpp"
#include "renderer/views/RenderViewWorldEditor.hpp"
#include "renderer/views/RenderViewUI.hpp"
#include "renderer/views/RenderViewPick.hpp"

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

		uint32 views_count;
		Sarray<RenderView> views;
		HashtableRH<RenderViewId> view_lookup;

		RenderViewId default_skybox_view_id;
		RenderViewId default_world_view_id;
		RenderViewId default_world_editor_view_id;
		RenderViewId default_ui_view_id;
		RenderViewId default_pick_view_id;

		Camera default_world_camera;
	};

	static bool32 create_default_render_views();
	static bool32 on_event(uint16 code, void* sender, void* listener_inst, EventData data);

	static SystemState* system_state = 0;

	bool32 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config)
	{
		SystemConfig* sys_config = (SystemConfig*)config;
		system_state = (SystemState*)allocator_callback(allocator, sizeof(SystemState));

		system_state->config = *sys_config;

		uint64 view_array_size = system_state->views.get_external_size_requirement(sys_config->max_view_count);
		void* view_array_data = allocator_callback(allocator, view_array_size);
		system_state->views.init(sys_config->max_view_count, 0, AllocationTag::ARRAY, view_array_data);

		uint64 hashtable_data_size = system_state->view_lookup.get_external_size_requirement(sys_config->max_view_count);
		void* hashtable_data = allocator_callback(allocator, hashtable_data_size);
		system_state->view_lookup.init(sys_config->max_view_count, HashtableRHFlag::ExternalMemory, AllocationTag::UNKNOWN, hashtable_data);

		for (uint32 i = 0; i < system_state->views.capacity; i++)
			system_state->views[i].id.invalidate();

		system_state->default_pick_view_id.invalidate();
		system_state->default_skybox_view_id.invalidate();
		system_state->default_ui_view_id.invalidate();
		system_state->default_world_editor_view_id.invalidate();
		system_state->default_world_view_id.invalidate();

		system_state->default_world_camera = Camera();

		system_state->views_count = 0;
		Event::event_register(SystemEventCode::DEFAULT_RENDERTARGET_REFRESH_REQUIRED, 0, on_event);

		create_default_render_views();

		return true;
	}

	void system_shutdown(void* state)
	{
		for (uint16 i = 0; i < system_state->views.capacity; i++)
		{
			if (system_state->views[i].id.is_valid())
				destroy_view(i);
		}

		Event::event_unregister(SystemEventCode::DEFAULT_RENDERTARGET_REFRESH_REQUIRED, 0, on_event);
		system_state->view_lookup.free_data();
		system_state->views.free_data();
		system_state = 0;
	}

	bool32 create_view(const RenderViewConfig* config)
	{
		if (system_state->view_lookup.get(config->name)) 
		{
			SHMERRORV("RenderViewSystem::create - A view named '%s' already exists or caused a hash table collision. A new one will not be created.", config->name);
			return false;
		}

		RenderViewId ref_id = RenderViewId::invalid_value;
		for (uint16 i = 0; i < (uint16)system_state->views.capacity; i++) {
			if (!system_state->views[i].id.is_valid()) {
				ref_id = i;
				break;
			}
		}

		if (!ref_id.is_valid()) {
			SHMERROR("RenderViewSystem::create - No available space for a new view. Change system config to account for more.");
			return false;
		}

		RenderView* view = &system_state->views[ref_id];
		view->id = ref_id;

		view->name = config->name;
		view->custom_shader_name = config->custom_shader_name;
		view->width = config->width;
		view->height = config->height;
		view->enabled = true;

		view->on_build_packet = config->on_build_packet;
		view->on_end_frame = config->on_end_frame;
		view->on_create = config->on_create;
		view->on_render = config->on_render;
		view->on_resize = config->on_resize;
		view->on_destroy = config->on_destroy;
		view->on_regenerate_attachment_target = config->on_regenerate_attachment_target;

		view->renderpasses.init(config->renderpass_count, 0, AllocationTag::RENDERER);
		for (uint32 pass_i = 0; pass_i < view->renderpasses.capacity; pass_i++)
		{
			if (!Renderer::renderpass_create(&config->renderpass_configs[pass_i], &view->renderpasses[pass_i]))
			{
				destroy_view(view->id);
				SHMERROR("Failed to create renderpass!");
				return false;
			}
		}

		view->geometries.init(1, 0, AllocationTag::RENDERER);
		view->instances.init(1, 0, AllocationTag::RENDERER);
		view->objects.init(1, 0, AllocationTag::RENDERER);

		if (!view->on_create(view))
		{
			destroy_view(view->id);
			SHMERROR("Failed to perform view's on_register function.");
			return false;
		}

		system_state->view_lookup.set_value(view->name.c_str(), ref_id);
		system_state->views_count++;
		//regenerate_render_targets(ref_id);

		return true;
	}

	void destroy_view(RenderViewId view_id)
	{
		RenderView* view = &system_state->views[view_id];
		if (!view->id.is_valid())
			return;
		
		system_state->view_lookup.remove_entry(view->name.c_str());
		view->on_destroy(view);

		for (uint32 pass_i = 0; pass_i < view->renderpasses.capacity; pass_i++)
			Renderer::renderpass_destroy(&view->renderpasses[pass_i]);

		view->objects.free_data();
		view->instances.free_data();
		view->geometries.free_data();
		view->internal_data.free_data();
		view->renderpasses.free_data();
		view->name.free_data();

		view->id.invalidate();
		system_state->views_count--;
	}

	RenderView* get(const char* name)
	{
		RenderViewId* view_id = system_state->view_lookup.get(name);
		if (!view_id || !system_state->views[*view_id].id.is_valid())
			return 0;

		return &system_state->views[*view_id];
	}

	RenderViewId get_id(const char* name)
	{
		RenderViewId* view_id = system_state->view_lookup.get(name);
		if (!view_id || !system_state->views[*view_id].id.is_valid())
			return RenderViewId::invalid_value;

		return *view_id;
	}

	Camera* get_bound_world_camera()
	{
		return &system_state->default_world_camera;
	}

	bool32 build_packet(RenderViewId view_id, FrameData* frame_data, const RenderViewPacketData* packet_data)
	{
		OPTICK_EVENT();
		RenderView* view = &system_state->views[view_id];
		if (!view->id.is_valid())
			return false;

		return view->on_build_packet(view, frame_data, packet_data);
	}

	void on_window_resize(uint32 width, uint32 height)
	{
		for (uint32 i = 0; i < system_state->views.capacity; ++i) 
		{
			if (!system_state->views[i].id.is_valid())
				break;

			system_state->views[i].on_resize(&system_state->views[i], width, height);
		}
	}

	bool32 on_render(FrameData* frame_data, uint32 frame_number, uint64 render_target_index)
	{
		OPTICK_EVENT();
		for (uint32 i = 0; i < system_state->views.capacity; ++i) 
		{
			if (!system_state->views[i].id.is_valid())
				break;

			system_state->views[i].on_render(&system_state->views[i], frame_data, frame_number, render_target_index);
		}

		return true;
	}

	void on_end_frame()
	{
		for (uint32 i = 0; i < system_state->views.capacity; ++i) 
		{
			RenderView* view = &system_state->views[i];
			if (!view->id.is_valid())
				break;

			view->geometries.clear();
			view->instances.clear();
			view->objects.clear();
			view->on_end_frame(view);
		}
	}

	void regenerate_render_targets(RenderViewId view_id)
	{
		RenderView* view = &system_state->views[view_id];
		if (!view->id.is_valid())
			return;

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
						if (!view->on_regenerate_attachment_target) 
						{
							SHMFATAL("View configured as source for an attachment whose view does not support this operation.");
							continue;
						}
	
						if (!view->on_regenerate_attachment_target(view, p, attachment)) 
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

	static void material_get_instance_render_data(Material* material, FrameData* frame_data, ShaderId shader_id, RenderViewInstanceData* out_instance_data)
	{
		out_instance_data->shader_id = shader_id;
		out_instance_data->instance_properties = material->properties;
		out_instance_data->texture_maps_count = material->maps.capacity;
		out_instance_data->texture_maps = (TextureMap**)frame_data->frame_allocator.allocate(sizeof(TextureMap*) * out_instance_data->texture_maps_count);
		for (uint32 i = 0; i < out_instance_data->texture_maps_count; i++)
			out_instance_data->texture_maps[i] = &material->maps[i];

		out_instance_data->shader_instance_id = material->shader_instance_id;
	}

	uint32 mesh_draw(Mesh* mesh, LightingInfo lighting, FrameData* frame_data, const Math::Frustum* frustum, RenderViewId view_id, ShaderId shader_id)
	{
		return meshes_draw(mesh, 1, lighting, frame_data, frustum, view_id, shader_id);
	}

	uint32 meshes_draw(Mesh* meshes, uint32 mesh_count, LightingInfo lighting, FrameData* frame_data, const Math::Frustum* frustum, RenderViewId view_id, ShaderId shader_id)
	{
		if (!view_id.is_valid())
			view_id = system_state->default_world_view_id;
		if (!shader_id.is_valid())
			shader_id = ShaderSystem::get_material_phong_shader_id();

		RenderView* view = &system_state->views[view_id];
		if (!view->id.is_valid())
			return false;

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

		RenderViewSystem::build_packet(view_id, frame_data, &packet_data);

		return packet_data.geometries_pushed_count;
	}

	static void skybox_get_instance_render_data(Skybox* skybox, FrameData* frame_data, ShaderId shader_id, RenderViewInstanceData* out_instance_data)
	{
		out_instance_data->shader_id = shader_id;
		out_instance_data->instance_properties = 0;
		out_instance_data->texture_maps_count = 1;
		out_instance_data->texture_maps = (TextureMap**)frame_data->frame_allocator.allocate(sizeof(TextureMap*) * out_instance_data->texture_maps_count);
		out_instance_data->texture_maps[0] = &skybox->cubemap;

		out_instance_data->shader_instance_id = skybox->shader_instance_id;
	}

	bool32 skybox_draw(Skybox* skybox, FrameData* frame_data, RenderViewId view_id, ShaderId shader_id)
	{
		if (!view_id.is_valid())
			view_id = system_state->default_skybox_view_id;
		if (!shader_id.is_valid())
			shader_id = ShaderSystem::get_skybox_shader_id();

		RenderView* view = &system_state->views[view_id];
		if (!view->id.is_valid())
			return false;

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

		return RenderViewSystem::build_packet(view_id, frame_data, &packet_data);
	}

	static void terrain_get_instance_render_data(Terrain* terrain, FrameData* frame_data, ShaderId shader_id, RenderViewInstanceData* out_instance_data)
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

	uint32 terrain_draw(Terrain* terrain, LightingInfo lighting, FrameData* frame_data, RenderViewId view_id, ShaderId shader_id)
	{
		return terrains_draw(terrain, 1, lighting, frame_data, view_id, shader_id);
	}

	uint32 terrains_draw(Terrain* terrains, uint32 terrains_count, LightingInfo lighting, FrameData* frame_data, RenderViewId view_id, ShaderId shader_id)
	{
		if (!view_id.is_valid())
			view_id = system_state->default_world_view_id;
		if (!shader_id.is_valid())
			shader_id = ShaderSystem::get_terrain_shader_id();

		RenderView* view = &system_state->views[view_id];
		if (!view->id.is_valid())
			return false;

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

		RenderViewSystem::build_packet(view_id, frame_data, &packet_data);

		return packet_data.geometries_pushed_count;
	}

	static void ui_text_get_instance_render_data(UIText* text, FrameData* frame_data, ShaderId shader_id, RenderViewInstanceData* out_instance_data)
	{
		out_instance_data->shader_id = shader_id;
		static Math::Vec4f white_color = { 1.0f, 1.0f, 1.0f, 1.0f };
		out_instance_data->instance_properties = &white_color;
		out_instance_data->texture_maps_count = 1;
		out_instance_data->texture_maps = (TextureMap**)frame_data->frame_allocator.allocate(sizeof(TextureMap*) * out_instance_data->texture_maps_count);
		out_instance_data->texture_maps[0] = &text->font_atlas->map;
		out_instance_data->shader_instance_id = text->shader_instance_id;
	}

	bool32 ui_text_draw(UIText* text, FrameData* frame_data, RenderViewId view_id, ShaderId shader_id)
	{
		if (!view_id.is_valid())
			view_id = system_state->default_ui_view_id;	
		if (!shader_id.is_valid())
			shader_id = ShaderSystem::get_ui_shader_id();

		RenderView* view = &system_state->views[view_id];
		if (!view->id.is_valid())
			return false;

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

		return RenderViewSystem::build_packet(view_id, frame_data, &packet_data);
	}

	uint32 box3D_draw(Box3D* box, FrameData* frame_data, RenderViewId view_id, ShaderId shader_id)
	{
		return boxes3D_draw(box, 1, frame_data, view_id, shader_id);
	}

	uint32 boxes3D_draw(Box3D* boxes, uint32 boxes_count, FrameData* frame_data, RenderViewId view_id, ShaderId shader_id)
	{
		if (!view_id.is_valid())
			view_id = system_state->default_world_view_id;	
		if (!shader_id.is_valid())
			shader_id = ShaderSystem::get_color3D_shader_id();

		RenderView* view = &system_state->views[view_id];
		if (!view->id.is_valid())
			return false;

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

		RenderViewSystem::build_packet(view_id, frame_data, &packet_data);

		return packet_data.geometries_pushed_count;
	}

	uint32 line3D_draw(Line3D* line, FrameData* frame_data, RenderViewId view_id, ShaderId shader_id)
	{
		return lines3D_draw(line, 1, frame_data, view_id, shader_id);
	}

	uint32 lines3D_draw(Line3D* lines, uint32 lines_count, FrameData* frame_data, RenderViewId view_id, ShaderId shader_id)
	{
		if (!view_id.is_valid())
			view_id = system_state->default_world_view_id;		
		if (!shader_id.is_valid())
			shader_id = ShaderSystem::get_color3D_shader_id();

		RenderView* view = &system_state->views[view_id];
		if (!view->id.is_valid())
			return false;

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

		RenderViewSystem::build_packet(view_id, frame_data, &packet_data);

		return packet_data.geometries_pushed_count;
	}

	uint32 gizmo3D_draw(Gizmo3D* gizmo, FrameData* frame_data, Camera* camera, RenderViewId view_id, ShaderId shader_id)
	{
		if (!view_id.is_valid())
			view_id = system_state->default_world_editor_view_id;
		if (!shader_id.is_valid())
			shader_id = ShaderSystem::get_color3D_shader_id();

		RenderView* view = &system_state->views[view_id];
		if (!view->id.is_valid())
			return false;

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

		return RenderViewSystem::build_packet(view->id, frame_data, &packet_data);
	}

	static bool32 create_default_render_views()
	{
		const Platform::Window* main_window = Engine::get_main_window();

		{
			RenderViewConfig skybox_view_config = {};
			skybox_view_config.width = 0;
			skybox_view_config.height = 0;
			skybox_view_config.name = "Builtin.Skybox";

			skybox_view_config.on_build_packet = render_view_skybox_on_build_packet;
			skybox_view_config.on_end_frame = render_view_skybox_on_end_frame;
			skybox_view_config.on_render = render_view_skybox_on_render;
			skybox_view_config.on_create = render_view_skybox_on_create;
			skybox_view_config.on_destroy = render_view_skybox_on_destroy;
			skybox_view_config.on_resize = render_view_skybox_on_resize;
			skybox_view_config.on_regenerate_attachment_target = 0;

			const uint32 skybox_pass_count = 1;
			Renderer::RenderPassConfig skybox_pass_configs[skybox_pass_count];

			Renderer::RenderPassConfig* skybox_pass_config = &skybox_pass_configs[0];
			skybox_pass_config->name = "Builtin.Skybox";
			skybox_pass_config->dim = { main_window->client_width, main_window->client_height };
			skybox_pass_config->offset = { 0, 0 };
			skybox_pass_config->clear_color = { 0.0f, 0.0f, 0.2f, 1.0f };
			skybox_pass_config->clear_flags = Renderer::RenderpassClearFlags::COLOR_BUFFER;
			skybox_pass_config->depth = 1.0f;
			skybox_pass_config->stencil = 0;

			const uint32 skybox_target_att_count = 1;
			Renderer::RenderTargetAttachmentConfig skybox_att_configs[skybox_target_att_count];
			skybox_att_configs[0].type = Renderer::RenderTargetAttachmentType::COLOR;
			skybox_att_configs[0].source = Renderer::RenderTargetAttachmentSource::DEFAULT;
			skybox_att_configs[0].load_op = Renderer::RenderTargetAttachmentLoadOp::DONT_CARE;
			skybox_att_configs[0].store_op = Renderer::RenderTargetAttachmentStoreOp::STORE;
			skybox_att_configs[0].present_after = false;

			skybox_pass_config->target_config.attachment_count = skybox_target_att_count;
			skybox_pass_config->target_config.attachment_configs = skybox_att_configs;
			skybox_pass_config->render_target_count = Renderer::get_window_attachment_count();

			skybox_view_config.renderpass_count = skybox_pass_count;
			skybox_view_config.renderpass_configs = skybox_pass_configs;

			if (create_view(&skybox_view_config))
				system_state->default_skybox_view_id = get_id(skybox_view_config.name);
		}

		{
			RenderViewConfig world_view_config = {};
			world_view_config.width = 0;
			world_view_config.height = 0;
			world_view_config.name = "Builtin.World";

			world_view_config.on_build_packet = render_view_world_on_build_packet;
			world_view_config.on_end_frame = render_view_world_on_end_frame;
			world_view_config.on_render = render_view_world_on_render;
			world_view_config.on_create = render_view_world_on_create;
			world_view_config.on_destroy = render_view_world_on_destroy;
			world_view_config.on_resize = render_view_world_on_resize;
			world_view_config.on_regenerate_attachment_target = 0;

			const uint32 world_pass_count = 1;
			Renderer::RenderPassConfig world_pass_configs[world_pass_count];

			Renderer::RenderPassConfig* world_pass_config = &world_pass_configs[0];

			world_pass_config->name = "Builtin.World";
			world_pass_config->dim = { main_window->client_width, main_window->client_height };
			world_pass_config->offset = { 0, 0 };
			world_pass_config->clear_color = { 0.0f, 0.0f, 0.2f, 1.0f };
			world_pass_config->clear_flags = Renderer::RenderpassClearFlags::DEPTH_BUFFER | Renderer::RenderpassClearFlags::STENCIL_BUFFER;
			world_pass_config->depth = 1.0f;
			world_pass_config->stencil = 0;

			const uint32 world_target_att_count = 2;
			Renderer::RenderTargetAttachmentConfig world_att_configs[world_target_att_count];
			world_att_configs[0].type = Renderer::RenderTargetAttachmentType::COLOR;
			world_att_configs[0].source = Renderer::RenderTargetAttachmentSource::DEFAULT;
			world_att_configs[0].load_op = Renderer::RenderTargetAttachmentLoadOp::LOAD;
			world_att_configs[0].store_op = Renderer::RenderTargetAttachmentStoreOp::STORE;
			world_att_configs[0].present_after = false;

			world_att_configs[1].type = Renderer::RenderTargetAttachmentType::DEPTH;
			world_att_configs[1].source = Renderer::RenderTargetAttachmentSource::DEFAULT;
			world_att_configs[1].load_op = Renderer::RenderTargetAttachmentLoadOp::DONT_CARE;
			world_att_configs[1].store_op = Renderer::RenderTargetAttachmentStoreOp::STORE;
			world_att_configs[1].present_after = false;

			world_pass_config->target_config.attachment_count = world_target_att_count;
			world_pass_config->target_config.attachment_configs = world_att_configs;
			world_pass_config->render_target_count = Renderer::get_window_attachment_count();

			world_view_config.renderpass_count = world_pass_count;
			world_view_config.renderpass_configs = world_pass_configs;

			if (create_view(&world_view_config))
				system_state->default_world_view_id = get_id(world_view_config.name);
		}

		{
			RenderViewConfig world_editor_view_config = {};
			world_editor_view_config.width = 0;
			world_editor_view_config.height = 0;
			world_editor_view_config.name = "Builtin.WorldEditor";

			world_editor_view_config.on_build_packet = render_view_world_editor_on_build_packet;
			world_editor_view_config.on_end_frame = render_view_world_editor_on_end_frame;
			world_editor_view_config.on_render = render_view_world_editor_on_render;
			world_editor_view_config.on_create = render_view_world_editor_on_create;
			world_editor_view_config.on_destroy = render_view_world_editor_on_destroy;
			world_editor_view_config.on_resize = render_view_world_editor_on_resize;
			world_editor_view_config.on_regenerate_attachment_target = 0;

			const uint32 world_editor_pass_count = 1;
			Renderer::RenderPassConfig world_editor_pass_configs[world_editor_pass_count];

			Renderer::RenderPassConfig* world_editor_pass_config = &world_editor_pass_configs[0];

			world_editor_pass_config->name = "Builtin.WorldEditor";
			world_editor_pass_config->dim = { main_window->client_width, main_window->client_height };
			world_editor_pass_config->offset = { 0, 0 };
			world_editor_pass_config->clear_color = { 0.0f, 0.0f, 0.2f, 1.0f };
			world_editor_pass_config->clear_flags = Renderer::RenderpassClearFlags::NONE;
			world_editor_pass_config->depth = 1.0f;
			world_editor_pass_config->stencil = 0;

			const uint32 world_editor_target_att_count = 2;
			Renderer::RenderTargetAttachmentConfig world_editor_att_configs[world_editor_target_att_count];
			world_editor_att_configs[0].type = Renderer::RenderTargetAttachmentType::COLOR;
			world_editor_att_configs[0].source = Renderer::RenderTargetAttachmentSource::DEFAULT;
			world_editor_att_configs[0].load_op = Renderer::RenderTargetAttachmentLoadOp::LOAD;
			world_editor_att_configs[0].store_op = Renderer::RenderTargetAttachmentStoreOp::STORE;
			world_editor_att_configs[0].present_after = false;

			world_editor_att_configs[1].type = Renderer::RenderTargetAttachmentType::DEPTH;
			world_editor_att_configs[1].source = Renderer::RenderTargetAttachmentSource::DEFAULT;
			world_editor_att_configs[1].load_op = Renderer::RenderTargetAttachmentLoadOp::LOAD;
			world_editor_att_configs[1].store_op = Renderer::RenderTargetAttachmentStoreOp::STORE;
			world_editor_att_configs[1].present_after = false;

			world_editor_pass_config->target_config.attachment_count = world_editor_target_att_count;
			world_editor_pass_config->target_config.attachment_configs = world_editor_att_configs;
			world_editor_pass_config->render_target_count = Renderer::get_window_attachment_count();

			world_editor_view_config.renderpass_count = world_editor_pass_count;
			world_editor_view_config.renderpass_configs = world_editor_pass_configs;

			if (create_view(&world_editor_view_config))
				system_state->default_world_editor_view_id = get_id(world_editor_view_config.name);
		}

		{
			RenderViewConfig ui_view_config = {};
			ui_view_config.width = 0;
			ui_view_config.height = 0;
			ui_view_config.name = "Builtin.UI";

			ui_view_config.on_build_packet = render_view_ui_on_build_packet;
			ui_view_config.on_end_frame = render_view_ui_on_end_frame;
			ui_view_config.on_render = render_view_ui_on_render;
			ui_view_config.on_create = render_view_ui_on_create;
			ui_view_config.on_destroy = render_view_ui_on_destroy;
			ui_view_config.on_resize = render_view_ui_on_resize;
			ui_view_config.on_regenerate_attachment_target = 0;

			const uint32 ui_pass_count = 1;
			Renderer::RenderPassConfig ui_pass_configs[ui_pass_count];

			Renderer::RenderPassConfig* ui_pass_config = &ui_pass_configs[0];
			ui_pass_config->name = "Builtin.UI";
			ui_pass_config->dim = { main_window->client_width, main_window->client_height };
			ui_pass_config->offset = { 0, 0 };
			ui_pass_config->clear_color = { 0.0f, 0.0f, 0.2f, 1.0f };
			ui_pass_config->clear_flags = Renderer::RenderpassClearFlags::NONE;
			ui_pass_config->depth = 1.0f;
			ui_pass_config->stencil = 0;

			const uint32 ui_target_att_count = 1;
			Renderer::RenderTargetAttachmentConfig ui_att_configs[ui_target_att_count];
			ui_att_configs[0].type = Renderer::RenderTargetAttachmentType::COLOR;
			ui_att_configs[0].source = Renderer::RenderTargetAttachmentSource::DEFAULT;
			ui_att_configs[0].load_op = Renderer::RenderTargetAttachmentLoadOp::LOAD;
			ui_att_configs[0].store_op = Renderer::RenderTargetAttachmentStoreOp::STORE;
			ui_att_configs[0].present_after = true;

			ui_pass_config->target_config.attachment_count = ui_target_att_count;
			ui_pass_config->target_config.attachment_configs = ui_att_configs;
			ui_pass_config->render_target_count = Renderer::get_window_attachment_count();

			ui_view_config.renderpass_count = ui_pass_count;
			ui_view_config.renderpass_configs = ui_pass_configs;

			if (create_view(&ui_view_config))
				system_state->default_ui_view_id = get_id(ui_view_config.name);
		}

		{
			RenderViewConfig pick_view_config = {};
			pick_view_config.width = 0;
			pick_view_config.height = 0;
			pick_view_config.name = "Builtin.PickV";

			pick_view_config.on_build_packet = render_view_pick_on_build_packet;
			pick_view_config.on_end_frame = render_view_pick_on_end_frame;
			pick_view_config.on_render = render_view_pick_on_render;
			pick_view_config.on_create = render_view_pick_on_create;
			pick_view_config.on_destroy = render_view_pick_on_destroy;
			pick_view_config.on_resize = render_view_pick_on_resize;
			pick_view_config.on_regenerate_attachment_target = render_view_pick_regenerate_attachment_target;

			const uint32 pick_pass_count = 2;
			Renderer::RenderPassConfig pick_pass_configs[pick_pass_count];

			Renderer::RenderPassConfig* world_pick_pass_config = &pick_pass_configs[0];
			Renderer::RenderPassConfig* ui_pick_pass_config = &pick_pass_configs[1];

			world_pick_pass_config->name = "Builtin.WorldPick";
			world_pick_pass_config->dim = { main_window->client_width, main_window->client_height };
			world_pick_pass_config->offset = { 0, 0 };
			world_pick_pass_config->clear_color = { 1.0f, 1.0f, 1.0f, 1.0f };
			world_pick_pass_config->clear_flags = Renderer::RenderpassClearFlags::COLOR_BUFFER | Renderer::RenderpassClearFlags::DEPTH_BUFFER;
			world_pick_pass_config->depth = 1.0f;
			world_pick_pass_config->stencil = 0;

			const uint32 world_pick_target_att_count = 2;
			Renderer::RenderTargetAttachmentConfig world_pick_att_configs[world_pick_target_att_count];
			world_pick_att_configs[0].type = Renderer::RenderTargetAttachmentType::COLOR;
			world_pick_att_configs[0].source = Renderer::RenderTargetAttachmentSource::VIEW;
			world_pick_att_configs[0].load_op = Renderer::RenderTargetAttachmentLoadOp::DONT_CARE;
			world_pick_att_configs[0].store_op = Renderer::RenderTargetAttachmentStoreOp::STORE;
			world_pick_att_configs[0].present_after = false;

			world_pick_att_configs[1].type = Renderer::RenderTargetAttachmentType::DEPTH;
			world_pick_att_configs[1].source = Renderer::RenderTargetAttachmentSource::VIEW;
			world_pick_att_configs[1].load_op = Renderer::RenderTargetAttachmentLoadOp::DONT_CARE;
			world_pick_att_configs[1].store_op = Renderer::RenderTargetAttachmentStoreOp::STORE;
			world_pick_att_configs[1].present_after = false;

			world_pick_pass_config->target_config.attachment_count = world_pick_target_att_count;
			world_pick_pass_config->target_config.attachment_configs = world_pick_att_configs;
			world_pick_pass_config->render_target_count = 1;

			ui_pick_pass_config->name = "Builtin.UIPick";
			ui_pick_pass_config->dim = { main_window->client_width, main_window->client_height };
			ui_pick_pass_config->offset = { 0, 0 };
			ui_pick_pass_config->clear_color = { 1.0f, 1.0f, 1.0f, 1.0f };
			ui_pick_pass_config->clear_flags = Renderer::RenderpassClearFlags::NONE;
			ui_pick_pass_config->depth = 1.0f;
			ui_pick_pass_config->stencil = 0;

			const uint32 ui_pick_target_att_count = 1;
			Renderer::RenderTargetAttachmentConfig ui_pick_att_configs[ui_pick_target_att_count];
			ui_pick_att_configs[0].type = Renderer::RenderTargetAttachmentType::COLOR;
			ui_pick_att_configs[0].source = Renderer::RenderTargetAttachmentSource::VIEW;
			ui_pick_att_configs[0].load_op = Renderer::RenderTargetAttachmentLoadOp::LOAD;
			ui_pick_att_configs[0].store_op = Renderer::RenderTargetAttachmentStoreOp::STORE;
			ui_pick_att_configs[0].present_after = false;

			ui_pick_pass_config->target_config.attachment_count = ui_pick_target_att_count;
			ui_pick_pass_config->target_config.attachment_configs = ui_pick_att_configs;
			ui_pick_pass_config->render_target_count = 1;

			pick_view_config.renderpass_count = pick_pass_count;
			pick_view_config.renderpass_configs = pick_pass_configs;

			if (create_view(&pick_view_config))
				system_state->default_pick_view_id = get_id(pick_view_config.name);
		}

		return true;
	}

	static bool32 on_event(uint16 code, void* sender, void* listener_inst, EventData data)
	{
		switch (code)
		{
		case SystemEventCode::DEFAULT_RENDERTARGET_REFRESH_REQUIRED:
		{
			for (uint32 i = 0; i < system_state->views.capacity; ++i) 
			{
				if (!system_state->views[i].id.is_valid())
					break;

				regenerate_render_targets(system_state->views[i].id);
			}
		}
		}

		return false;
	}
}