#pragma once

#include "RendererTypes.hpp"

namespace Renderer
{
	SHMAPI bool32 flags_enabled(RendererConfigFlags::Value flags);
	SHMAPI void set_flags(RendererConfigFlags::Value flags, bool32 enabled);
}