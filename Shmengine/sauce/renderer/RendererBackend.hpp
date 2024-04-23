#pragma once

#include "RendererTypes.hpp"

namespace Renderer
{
	bool32 backend_create(BackendType type, Platform::PlatformState* plat_state, Backend* out_backend);
	void backend_destroy(Backend* backend);
}
