#pragma once

#include "RendererTypes.hpp"

namespace Renderer
{
	bool32 backend_create(BackendType type, Backend* out_backend);
	void backend_destroy(Backend* backend);
}
