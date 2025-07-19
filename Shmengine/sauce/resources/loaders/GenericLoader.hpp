#pragma once

#include "Defines.hpp"

struct Buffer;

namespace ResourceSystem
{
	SHMAPI bool32 generic_loader_load(const char* name, Buffer* out_buffer);
	SHMAPI void generic_loader_unload(Buffer* buffer);
}