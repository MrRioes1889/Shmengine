#pragma once

#include "Defines.hpp"
#include "renderer/RendererTypes.hpp"

namespace Renderer
{

	bool32 render_view_ui_on_create(RenderView* self);
	void render_view_ui_on_destroy(RenderView* self);
	void render_view_ui_on_resize(RenderView* self, uint32 width, uint32 height);
	bool32 render_view_ui_on_build_packet(const RenderView* self, void* data, RenderViewPacket* out_packet);
	bool32 render_view_ui_on_render(const RenderView* self, const RenderViewPacket& packet, uint64 frame_number, uint64 render_target_index);

}
