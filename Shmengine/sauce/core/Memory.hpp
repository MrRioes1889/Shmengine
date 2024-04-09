#pragma once

#include "Defines.hpp"

namespace Memory
{
	bool8 init_memory();

	void* raw_allocate(uint64 size, bool32 aligned);
	void raw_free(void* mem, bool32 aligned);
	SHMAPI void* allocate(uint64 size, bool32 aligned);
	SHMAPI void* reallocate(uint64 size, void* block, bool32 aligned);
	SHMAPI void free_memory(void* block, bool32 aligned);

	SHMAPI void* zero_memory(void* block, uint64 size);
	SHMAPI void* copy_memory(void* dest, const void* source, uint64 size);
	SHMAPI void* set_memory(void* dest, int32 value, uint64 size);

}