#include "RenderViewCanvas.hpp"

bool32 render_view_canvas_on_register(RenderView* self)
{
	return true;
}

void render_view_canvas_on_unregister(RenderView* self)
{
	return;
}

void render_view_canvas_on_resize(RenderView* self, uint32 width, uint32 height)
{
	return;
}

bool32 render_view_canvas_on_build_packet(RenderView* self, FrameData* frame_data, const RenderViewPacketData* packet_data)
{
	return true;
}

void render_view_canvas_on_end_frame(RenderView* self)
{
	return;
}

bool32 render_view_canvas_on_render(RenderView* self, FrameData* frame_data, uint32 frame_number, uint64 render_target_index)
{
	return true;
}
