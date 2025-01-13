#include "RenderViewSystem.hpp"

#include "core/Logging.hpp"
#include "utility/CString.hpp"
#include "containers/Hashtable.hpp"
#include "renderer/RendererFrontend.hpp"
#include "systems/TextureSystem.hpp"
#include "optick.h"

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

		system_state->lookup_table.floodfill(INVALID_ID);

		for (uint32 i = 0; i < sys_config->max_view_count; ++i)
			system_state->registered_views[i] = 0;

		return true;
	}

	void system_shutdown(void* state)
	{
		system_state->lookup_table.floodfill(INVALID_ID);
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
		if (ref_id != INVALID_ID) {
			SHMERRORV("RenderViewSystem::create - A view named '%s' already exists or caused a hash table collision. A new one will not be created.", view->name);
			return false;
		}

		for (uint32 i = 0; i < system_state->config.max_view_count; ++i) {
			if (!system_state->registered_views[i]) {
				ref_id = i;
				break;
			}
		}

		if (ref_id == INVALID_ID) {
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

		view->geometries.free_data();
		view->internal_data.free_data();
		view->renderpasses.free_data();
	}

	RenderView* get(const char* name)
	{
		uint32 id = system_state->lookup_table.get_value(name);
		if (id == INVALID_ID)
			return 0;

		return system_state->registered_views[id];
	}

	bool32 build_packet(RenderView* view, Memory::LinearAllocator* frame_allocator, const RenderViewPacketData* packet_data)
	{
		OPTICK_EVENT();
		return view->on_build_packet(view, frame_allocator, packet_data);
	}

	void on_window_resize(uint32 width, uint32 height)
	{
		for (uint32 i = 0; i < system_state->config.max_view_count; ++i) {
			if (system_state->registered_views[i]) {
				system_state->registered_views[i]->on_resize(system_state->registered_views[i], width, height);
			}
		}
	}

	bool32 on_render(RenderView* view, Memory::LinearAllocator* frame_allocator, uint32 frame_number, uint64 render_target_index)
	{
		OPTICK_EVENT();
		return view->on_render(view, frame_allocator, frame_number, render_target_index);
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
					else if (attachment->source == Renderer::RenderTargetAttachmentSource::VIEW) 
					{
						if (!view->regenerate_attachment_target) {
							SHMFATAL("View configured as source for an attachment whose view does not support this operation.");
							continue;
						}
	
						if (!view->regenerate_attachment_target(view, p, attachment)) {
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
}