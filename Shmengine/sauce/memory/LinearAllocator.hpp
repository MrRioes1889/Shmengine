#pragma once

#include "Defines.hpp"

namespace Memory
{

	struct LinearAllocator
	{
		uint64 size;
		uint64 allocated;

		void* memory;
		bool32 owns_memory;
	};

	SHMAPI void linear_allocator_create(uint64 size, LinearAllocator* out_allocator, void* memory = 0);
	SHMAPI void linear_allocator_destroy(LinearAllocator* allocator);

	SHMAPI void* linear_allocator_allocate(LinearAllocator* allocator, uint64 size);
	SHMAPI void linear_allocator_free_all_data(LinearAllocator* allocator);

}


