#pragma once

#include "Defines.hpp"
#include "utility/MathTypes.hpp"
#include "renderer/RendererTypes.hpp"

namespace RenderViewSystem
{

	struct Config
	{
		uint32 max_view_count;

		inline static const char* default_name = "default";
	};

	bool32 system_init(PFN_allocator_allocate_callback allocator_callback, void*& out_state, Config config);
	void system_shutdown();

	bool32 create(const Renderer::RenderViewConfig& config);

	Renderer::RenderView* get(const char* name);

	bool32 build_packet(const Renderer::RenderView* view, void* data, Renderer::RenderViewPacket* out_packet);

	void on_window_resize(uint32 width, uint32 height);
	bool32 on_render(const Renderer::RenderView* view, const Renderer::RenderViewPacket& packet, uint64 frame_number, uint64 render_target_index);

}