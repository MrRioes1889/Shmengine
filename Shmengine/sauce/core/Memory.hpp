#pragma once

#include "Defines.hpp"

enum class AllocationTag
{
	UNKNOWN,
	MAIN,
	TRANSIENT,
	PERMANENT,
	RAW,

	TAG_COUNT
};

namespace Memory
{
	bool32 init_memory(void* linear_allocator, void*& out_state);

	SHMAPI void* allocate(uint64 size, bool32 aligned, AllocationTag tag);
	SHMAPI void* reallocate(uint64 size, void* block, bool32 aligned, AllocationTag tag);
	SHMAPI void free_memory(void* block, bool32 aligned, AllocationTag tag);

	SHMAPI void* zero_memory(void* block, uint64 size);
	SHMAPI void* copy_memory(const void* source, void* dest, uint64 size);
	SHMAPI void* set_memory(void* dest, int32 value, uint64 size);

}