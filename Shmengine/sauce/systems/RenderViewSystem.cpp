#include "RenderViewSystem.hpp"

#include "core/Logging.hpp"
#include "utility/CString.hpp"
#include "containers/Hashtable.hpp"
#include "renderer/RendererFrontend.hpp"
#include "renderer/views/RenderViewWORLD.hpp"
#include "renderer/views/RenderViewUI.hpp"

namespace RenderViewSystem
{

	struct SystemState
	{
		Config config;

		Renderer::RenderView* registered_views;
		Hashtable<uint32> lookup_table;

	};

	static SystemState* system_state = 0;

	bool32 system_init(PFN_allocator_allocate_callback allocator_callback, void*& out_state, Config config)
	{
		out_state = allocator_callback(sizeof(SystemState));
		system_state = (SystemState*)out_state;

		system_state->config = config;

		uint64 view_array_size = sizeof(Renderer::RenderView) * config.max_view_count;
		system_state->registered_views = (Renderer::RenderView*)allocator_callback(view_array_size);

		uint64 hashtable_data_size = sizeof(uint32) * config.max_view_count;
		void* hashtable_data = allocator_callback(hashtable_data_size);
		system_state->lookup_table.init(config.max_view_count, HashtableFlag::EXTERNAL_MEMORY, AllocationTag::UNKNOWN, hashtable_data);

		system_state->lookup_table.floodfill(INVALID_ID);

		for (uint32 i = 0; i < config.max_view_count; ++i) {
			system_state->registered_views[i].id = INVALID_ID;
		}

		return true;
	}

	void system_shutdown()
	{
		system_state = 0;
	}

	bool32 create(const Renderer::RenderViewConfig& config)
	{

		if (!config.pass_configs.count) {
			SHMERROR("RenderViewSystem::create - Config must have at least one renderpass.");
			return false;
		}

		uint32& ref_id = system_state->lookup_table.get_ref(config.name);
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
		view.renderpasses.init(config.pass_configs.count, 0, AllocationTag::MAIN);

		for (uint32 i = 0; i < view.renderpasses.count; i++)
		{
			view.renderpasses[i] = Renderer::renderpass_get(config.pass_configs[i].name);
			if (!view.renderpasses[i])
			{
				SHMERRORV("RenderViewSystem::create - Failed to get renderpass called '%s'.", config.pass_configs[i].name);
				view.renderpasses.free_data();
				ref_id = INVALID_ID;
				view.id = INVALID_ID;
				return false;
			}
		}

		if (config.type == Renderer::RenderViewType::WORLD) {		
			view.on_build_packet = Renderer::render_view_world_on_build_packet;  // For building the packet
			view.on_render = Renderer::render_view_world_on_render;              // For rendering the packet
			view.on_create = Renderer::render_view_world_on_create;
			view.on_destroy = Renderer::render_view_world_on_destroy;
			view.on_resize = Renderer::render_view_world_on_resize;
		}
		else if (config.type == Renderer::RenderViewType::UI) {
			view.on_build_packet = Renderer::render_view_ui_on_build_packet;  // For building the packet
			view.on_render = Renderer::render_view_ui_on_render;              // For rendering the packet
			view.on_create = Renderer::render_view_ui_on_create;
			view.on_destroy = Renderer::render_view_ui_on_destroy;
			view.on_resize = Renderer::render_view_ui_on_resize;
		}

		if (!view.on_create(&view))
		{
			SHMERROR("RenderViewSystem::create - Failed to create view.");
			view.renderpasses.free_data();
			ref_id = INVALID_ID;
			view.id = INVALID_ID;
			return false;
		}

		return true;

	}

	Renderer::RenderView* get(const char* name)
	{
		uint32 id = system_state->lookup_table.get_value(name);
		if (id == INVALID_ID)
			return 0;

		return &system_state->registered_views[id];
	}

	bool32 build_packet(const Renderer::RenderView* view, void* data, Renderer::RenderViewPacket* out_packet)
	{
		return view->on_build_packet(view, data, out_packet);
	}

	void on_window_resize(uint32 width, uint32 height)
	{
		for (uint32 i = 0; i < system_state->config.max_view_count; ++i) {
			if (system_state->registered_views[i].id != INVALID_ID) {
				system_state->registered_views[i].on_resize(&system_state->registered_views[i], width, height);
			}
		}
	}

	bool32 on_render(const Renderer::RenderView* view, const Renderer::RenderViewPacket& packet, uint64 frame_number, uint64 render_target_index)
	{
		return view->on_render(view, packet, frame_number, render_target_index);
	}
}