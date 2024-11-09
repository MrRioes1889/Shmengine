#pragma once

#include "systems/ResourceSystem.hpp"

namespace ResourceSystem
{
	SHMAPI bool32 generic_loader_load(const char* name, void* params, Buffer* out_buffer);
	SHMAPI void generic_loader_unload(Buffer* buffer);
}