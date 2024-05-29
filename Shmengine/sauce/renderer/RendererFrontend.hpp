#pragma once

#include "RendererTypes.hpp"

namespace Renderer
{

	bool32 init(void* linear_allocator, void*& out_state, const char* application_name);
	void shutdown();

	void on_resized(uint32 width, uint32 height);

	bool32 draw_frame(RenderData* data);

}


