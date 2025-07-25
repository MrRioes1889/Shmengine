#pragma once

#include "RendererTypes.hpp"

namespace Renderer
{
	SHMAPI bool8 flags_enabled(RendererConfigFlags::Value flags);
	SHMAPI void set_flags(RendererConfigFlags::Value flags, bool8 enabled);
}