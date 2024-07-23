#pragma once

#include "Defines.hpp"

enum class AllocationTag
{
	UNKNOWN,
	PLATFORM,
	MAIN_MEMORY,
	ALLOCATORS,
	ARRAY,
	LINEAR_ALLOCATOR,
	DARRAY,
	DICT,
	RING_QUEUE,
	BST,
	STRING,
	APPLICATION,
	JOB,
	TEXTURE,
	MATERIAL_INSTANCE,
	RENDERER,
	GAME,
	TRANSFORM,
	ENTITY,
	ENTITY_NODE,
	SCENE,
	RESOURCE,
	VULKAN,
	// "External" vulkan allocations, for reporting purposes only.
	VULKAN_EXT,
	D3D_12,
	OPENGL,
	// Representation of GPU-local/vram
	GPU_LOCAL,

	TAG_COUNT
};

namespace Memory
{

	struct SystemConfig
	{
		uint64 total_allocation_size;
	};

	bool32 system_init(SystemConfig config);
	void system_shutdown();

	SHMAPI void* allocate(uint64 size, AllocationTag tag, uint16 alignment = 1);
	SHMAPI void* reallocate(uint64 size, void* block, uint16 alignment = 1);
	SHMAPI void free_memory(void* block, bool32 aligned = true);

	void* allocate_string(uint64 size, AllocationTag tag, uint16 alignment = 1);
	void* reallocate_string(uint64 size, void* block, uint16 alignment = 1);
	void free_memory_string(void* block, bool32 aligned = true);

	void* allocate_platform(uint64 size, AllocationTag tag, uint16 alignment = 1);
	void* reallocate_platform(uint64 size, void* block, uint16 alignment = 1);
	void free_memory_platform(void* block, bool32 aligned = true);

	SHMAPI void* zero_memory(void* block, uint64 size);
	SHMAPI void* copy_memory(const void* source, void* dest, uint64 size);
	SHMAPI void* set_memory(void* dest, int32 value, uint64 size);

	SHMAPI uint32 get_current_allocation_count();

}