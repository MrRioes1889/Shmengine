#pragma once

#include "Defines.hpp"

struct ApplicationConfig;
struct FrameData;

typedef void* (*FP_allocator_allocate)(void* allocator, uint64 size);

typedef bool32 (*FP_system_init)(FP_allocator_allocate allocator_allocate, void* allocator, void* config);
typedef void   (*FP_system_shutdown)(void* state);
typedef bool32 (*FP_system_update)(void* state, const FrameData* frame_data);

namespace SubsystemManager
{
	
	bool32 init_basic();
	bool32 init_advanced(const ApplicationConfig* app_config);

	void shutdown_basic();
	void shutdown_advanced();

	bool32 update(const FrameData* frame_data);

};

	




