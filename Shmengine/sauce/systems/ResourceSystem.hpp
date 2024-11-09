#pragma once

#include "resources/ResourceTypes.hpp"
#include "core/Subsystems.hpp"

namespace ResourceSystem
{

	struct SystemConfig
	{
		char asset_base_path[MAX_FILEPATH_LENGTH];
	};

	bool32 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config);
	void system_shutdown(void* state);

	SHMAPI const char* get_base_path();

}