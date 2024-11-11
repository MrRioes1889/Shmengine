#pragma once

#include "Defines.hpp"

typedef void* (*FP_allocator_allocate)(void* allocator, uint64 size);

typedef bool32 (*FP_system_init)(FP_allocator_allocate allocator_allocate, void* allocator, void* config);
typedef void   (*FP_system_shutdown)(void* state);
typedef bool32 (*FP_system_update)(void* state, float64 delta_time);

struct ApplicationConfig;

namespace SubsystemManager
{
	
	bool32 init(ApplicationConfig* app_config);
	bool32 post_boot_init(ApplicationConfig* app_config);
	void shutdown();

	bool32 update(float64 delta_time);

};

	




