#pragma once

#include "Defines.hpp"

namespace Memory
{

	struct LinearAllocator
	{
		uint64 size;
		uint64 allocated;

		void* memory;
		bool8 owns_memory;

		SHMAPI void init(uint64 memory_size, void* memory_ptr = 0);
		SHMAPI void destroy();

		SHMAPI void* allocate(uint64 alloc_size);
		SHMAPI void free_all_data();
	};

	

}


