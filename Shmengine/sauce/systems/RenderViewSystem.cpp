#include "RenderViewSystem.hpp"

#include "core/Logging.hpp"
#include "utility/CString.hpp"
#include "containers/Hashtable.hpp"
#include "renderer/RendererFrontend.hpp"
#include "renderer/views/RenderViewWorld.hpp"
#include "renderer/views/RenderViewUI.hpp"
#include "renderer/views/RenderViewSkybox.hpp"
#include "renderer/views/RenderViewPick.hpp"
#include "systems/TextureSystem.hpp"
#include "optick.h"

namespace RenderViewSystem
{

	struct SystemState
	{
		SystemConfig config;

		Renderer::RenderView* registered_views;
		Hashtable<uint32> lookup_table;

	};

	static SystemState* system_state = 0;
	static void destroy(Renderer::RenderView* view);

	bool32 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config)
	{

		SystemConfig* sys_config = (SystemConfig*)config;
		system_state = (SystemState*)allocator_callback(allocator, sizeof(SystemState));

		system_state->config = *sys_config;

		uint64 view_array_size = sizeof(Renderer::RenderView) * sys_config->max_view_count;
		system_state->registered_views = (Renderer::RenderView*)allocator_callback(allocator, view_array_size);

		uint64 hashtable_data_size = sizeof(uint32) * sys_config->max_view_count;
		void* hashtable_data = allocator_callback(allocator, hashtable_data_size);
		system_state->lookup_table.init(sys_config->max_view_count, HashtableFlag::EXTERNAL_MEMORY, AllocationTag::UNKNOWN, hashtable_data);

		system_state->lookup_table.floodfill(INVALID_ID);

		for (uint32 i = 0; i < sys_config->max_view_count; ++i) {
			system_state->registered_views[i].id = INVALID_ID;
		}

		return true;
	}

	void system_shutdown(void* state)
	{
		system_state->lookup_table.floodfill(INVALID_ID);
		for (uint32 i = 0; i < system_state->config.max_view_count; i++)
		{
			if (system_state->registered_views[i].id != INVALID_ID)
			{
				destroy(&system_state->registered_views[i]);
				system_state->registered_views[i].id = INVALID_ID;
			}			
		}

		system_state = 0;
	}

	bool32 create(const Renderer::RenderViewConfig& config)
	{

		if (!config.pass_configs.capacity) {
			SHMERROR("RenderViewSystem::create - Config must have at least one renderpass.");
			return false;
		}

		uint32 ref_id = system_state->lookup_table.get_value(config.name);
		if (ref_id != INVALID_ID) {
			SHMERRORV("RenderViewSystem::create - A view named '%s' already exists or caused a hash table collision. A new one will not be created.", config.name);
			return false;
		}

		for (uint32 i = 0; i < system_state->config.max_view_count; ++i) {
			if (system_state->registered_views[i].id == INVALID_ID) {
				ref_id = i;
				break;
			}
		}

		if (ref_id == INVALID_ID) {
			SHMERROR("RenderViewSystem::create - No available space for a new view. Change system config to account for more.");
			return false;
		}

		Renderer::RenderView& view = system_state->registered_views[ref_id];
		view.id = ref_id;
		view.type = config.type;
		view.name = config.name;
		view.custom_shader_name = config.custom_shader_name;
		view.renderpasses.init(config.pass_configs.capacity, 0, AllocationTag::RENDERER);

		for (uint32 i = 0; i < view.renderpasses.capacity; i++)
		{
			if (!Renderer::renderpass_create(&config.pass_configs[i], &view.renderpasses[i]))
			{
				SHMERROR("Failed to create renderpass.");
				view.renderpasses.free_data();
				view.id = INVALID_ID;
				return false;
			}
		}
	
		if (config.type == Renderer::RenderViewType::WORLD) 
		{		
			view.on_build_packet = Renderer::render_view_world_on_build_packet;
			view.on_destroy_packet = Renderer::render_view_world_on_destroy_packet;
			view.on_render = Renderer::render_view_world_on_render;
			view.on_create = Renderer::render_view_world_on_create;
			view.on_destroy = Renderer::render_view_world_on_destroy;
			view.on_resize = Renderer::render_view_world_on_resize;	
			view.regenerate_attachment_target = 0;
		}
		else if (config.type == Renderer::RenderViewType::UI) 
		{
			view.on_build_packet = Renderer::render_view_ui_on_build_packet;
			view.on_destroy_packet = Renderer::render_view_ui_on_destroy_packet;
			view.on_render = Renderer::render_view_ui_on_render; 
			view.on_create = Renderer::render_view_ui_on_create;
			view.on_destroy = Renderer::render_view_ui_on_destroy;
			view.on_resize = Renderer::render_view_ui_on_resize;
			view.regenerate_attachment_target = 0;
		}
		else if (config.type == Renderer::RenderViewType::SKYBOX) 
		{
			view.on_build_packet = Renderer::render_view_skybox_on_build_packet;
			view.on_destroy_packet = Renderer::render_view_skybox_on_destroy_packet;
			view.on_render = Renderer::render_view_skybox_on_render;  
			view.on_create = Renderer::render_view_skybox_on_create;
			view.on_destroy = Renderer::render_view_skybox_on_destroy;
			view.on_resize = Renderer::render_view_skybox_on_resize;
			view.regenerate_attachment_target = 0;
		}
		else if (config.type == Renderer::RenderViewType::PICK)
		{
			view.on_build_packet = Renderer::render_view_pick_on_build_packet;
			view.on_destroy_packet = Renderer::render_view_pick_on_destroy_packet;
			view.on_render = Renderer::render_view_pick_on_render;
			view.on_create = Renderer::render_view_pick_on_create;
			view.on_destroy = Renderer::render_view_pick_on_destroy;
			view.on_resize = Renderer::render_view_pick_on_resize;
			view.regenerate_attachment_target = Renderer::render_view_pick_regenerate_attachment_target;
		}

		if (!view.on_create(&view))
		{
			SHMERROR("RenderViewSystem::create - Failed to create view.");
			view.renderpasses.free_data();
			view.id = INVALID_ID;
			return false;
		}

		system_state->lookup_table.set_value(config.name, ref_id);

		regenerate_render_targets(&view);

		return true;

	}

	static void destroy(Renderer::RenderView* view)
	{
		view->on_destroy(view);
		for (uint32 i = 0; i < view->renderpasses.capacity; i++)
			Renderer::renderpass_destroy(&view->renderpasses[i]);
		view->renderpasses.free_data();
	}

	Renderer::RenderView* get(const char* name)
	{
		uint32 id = system_state->lookup_table.get_value(name);
		if (id == INVALID_ID)
			return 0;

		return &system_state->registered_views[id];
	}

	bool32 build_packet(Renderer::RenderView* view, Memory::LinearAllocator* frame_allocator, void* data, Renderer::RenderViewPacket* out_packet)
	{
		OPTICK_EVENT();
		return view->on_build_packet(view, frame_allocator, data, out_packet);
	}

	void on_window_resize(uint32 width, uint32 height)
	{
		for (uint32 i = 0; i < system_state->config.max_view_count; ++i) {
			if (system_state->registered_views[i].id != INVALID_ID) {
				system_state->registered_views[i].on_resize(&system_state->registered_views[i], width, height);
			}
		}
	}

	bool32 on_render(Renderer::RenderView* view, const Renderer::RenderViewPacket& packet, uint64 frame_number, uint64 render_target_index)
	{
		OPTICK_EVENT();
		return view->on_render(view, packet, frame_number, render_target_index);
	}

	void regenerate_render_targets(Renderer::RenderView* view)
	{
		for (uint32 p = 0; p < view->renderpasses.capacity; p++)
		{
			Renderer::Renderpass* pass = &view->renderpasses[p];

			for (uint32 rt = 0; rt < pass->render_targets.capacity; rt++)
			{
				Renderer::RenderTarget* target = &pass->render_targets[rt];

				Renderer::render_target_destroy(target, false);

				for (uint32 att = 0; att < target->attachments.capacity; att++) 
				{
					Renderer::RenderTargetAttachment* attachment = &target->attachments[att];
					if (attachment->source == Renderer::RenderTargetAttachmentSource::DEFAULT) {
						if (attachment->type == Renderer::RenderTargetAttachmentType::COLOR) {
							attachment->texture = Renderer::get_window_attachment(rt);
						}
						else if (attachment->type == Renderer::RenderTargetAttachmentType::DEPTH) {
							attachment->texture = Renderer::get_depth_attachment(rt);
						}
						else {
							SHMFATAL("Unsupported attachment type.");
							continue;
						}
					}
					else if (attachment->source == Renderer::RenderTargetAttachmentSource::VIEW) {
						if (!view->regenerate_attachment_target) {
							SHMFATAL("View configured as source for an attachment whose view does not support this operation.");
							continue;
						}
						else {
							if (!view->regenerate_attachment_target(view, p, attachment)) {
								SHMERROR("View failed to regenerate attachment target for attachment type.");
							}
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
}