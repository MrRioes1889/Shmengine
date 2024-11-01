#pragma once

#include "Defines.hpp"
#include "memory/LinearAllocator.hpp"

typedef void* (*FP_allocator_allocate)(void* allocator, uint64 size);

typedef bool32 (*FP_system_init)(FP_allocator_allocate allocator_allocate, void* allocator, void* config);
typedef void   (*FP_system_shutdown)(void* state);
typedef bool32 (*FP_system_update)(void* state, float64 delta_time);

struct Subsystem
{
	uint64 state_size;
	void* state;

	FP_system_init init;
	FP_system_shutdown shutdown;
	FP_system_update update;
};

namespace SubsystemType
{
	enum
	{
		MEMORY,
		CONSOLE,
		LOGGING,
		INPUT,
		EVENT,
		PLATFORM,
		RESOURCE_SYSTEM,	
		RENDERER,	
		SHADER_SYSTEM,
		JOB_SYSTEM,		
		TEXTURE_SYSTEM,
		FONT_SYSTEM,
		CAMERA_SYSTEM,	
		RENDERVIEW_SYSTEM,
		MATERIAL_SYSTEM,
		GEOMETRY_SYSTEM,
		KNOWN_TYPES_COUNT,

		MAX_TYPES_COUNT = 128
	};
	typedef uint8 Value;
}

struct ApplicationConfig;

struct SubsystemManager
{
	Memory::LinearAllocator allocator;
	Subsystem subsystems[SubsystemType::MAX_TYPES_COUNT];

	bool32 init(ApplicationConfig* app_config);
	bool32 post_boot_init(ApplicationConfig* app_config);
	void shutdown();

	bool32 update(float64 delta_time);
	bool32 register_system(SubsystemType::Value type, FP_system_init init_callback, FP_system_shutdown shutdown_callback, FP_system_update update_callback, void* config);

private:
	bool32 register_known_systems_pre_boot(ApplicationConfig* app_config);
	bool32 register_known_systems_post_boot(ApplicationConfig* app_config);
	void shutdown_known_systems();

};

	




