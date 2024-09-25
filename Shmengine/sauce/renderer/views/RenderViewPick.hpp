#pragma once

#include "Defines.hpp"
#include "renderer/RendererTypes.hpp"

namespace Renderer
{

	bool32 render_view_pick_on_create(RenderView* self);
	void render_view_pick_on_destroy(RenderView* self);
	void render_view_pick_on_resize(RenderView* self, uint32 width, uint32 height);
	bool32 render_view_pick_on_build_packet(RenderView* self, Memory::LinearAllocator* frame_allocator, void* data, RenderViewPacket* out_packet);
	void render_view_pick_on_destroy_packet(const RenderView* self, RenderViewPacket* packet);
	bool32 render_view_pick_on_render(RenderView* self, const RenderViewPacket& packet, uint64 frame_number, uint64 render_target_index);
	bool32 render_view_pick_regenerate_attachment_target(const RenderView* self, uint32 pass_index, RenderTargetAttachment* attachment);

}
