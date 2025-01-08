#pragma once

#include "Defines.hpp"
#include "renderer/RendererTypes.hpp"
#include "systems/RenderViewSystem.hpp"

namespace Renderer
{

	bool32 render_view_pick_on_create(RenderView* self);
	void render_view_pick_on_destroy(RenderView* self);
	void render_view_pick_on_resize(RenderView* self, uint32 width, uint32 height);
	bool32 render_view_pick_on_build_packet(RenderView* self, Memory::LinearAllocator* frame_allocator, const RenderViewPacketData* packet_data);
	void render_view_pick_on_end_frame(RenderView* self);
	bool32 render_view_pick_on_render(RenderView* self, Memory::LinearAllocator* frame_allocator, uint32 frame_number, uint64 render_target_index);
	bool32 render_view_pick_regenerate_attachment_target(const RenderView* self, uint32 pass_index, RenderTargetAttachment* attachment);

}
