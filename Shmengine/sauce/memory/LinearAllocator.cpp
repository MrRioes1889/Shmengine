#include "LinearAllocator.hpp"

#include "core/Memory.hpp"
#include "core/Assert.hpp"
#include "core/Logging.hpp"

namespace Memory
{

	void linear_allocator_create(uint64 size, LinearAllocator* out_allocator, void* memory)
	{
		out_allocator->size = size;
		out_allocator->allocated = 0;
		out_allocator->memory = memory;
		out_allocator->owns_memory = memory == 0;

		if (out_allocator->owns_memory)
			out_allocator->memory = allocate_platform(size, AllocationTag::LINEAR_ALLOCATOR);
	}

	void linear_allocator_destroy(LinearAllocator* allocator)
	{
		if (allocator->owns_memory)
			free_memory_platform(allocator->memory);

		*allocator = {};
	}

	void* linear_allocator_allocate(LinearAllocator* allocator, uint64 size)
	{	
		//SHMASSERT_MSG(allocator->allocated + size <= allocator->size, "Linear allocator ran out of memory!");
		if (allocator->allocated + size > allocator->size)
		{
			SHMERROR("Linear allocator ran out of memory!");
			return 0;
		}

		void* mem = PTR_BYTES_OFFSET(allocator->memory, allocator->allocated);
		allocator->allocated += size;
		return mem;
	}

}

