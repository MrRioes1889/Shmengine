#pragma once

#include "RendererTypes.hpp"

namespace Renderer
{
	struct MeshDataStatic;

	bool32 init(const char* application_name, Platform::PlatformState* plat_state);
	void shutdown();

	void on_resized(uint32 width, uint32 height);

	bool32 draw_frame(RenderData* data);
}


