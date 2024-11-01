#include "LinearAllocator.hpp"

#include "core/Memory.hpp"
#include "core/Assert.hpp"
#include "core/Logging.hpp"

namespace Memory
{

	void LinearAllocator::init(uint64 memory_size, void* memory_ptr)
	{
		size = memory_size;
		allocated = 0;
		memory = memory_ptr;
		owns_memory = memory == 0;

		if (owns_memory)
			memory = allocate_platform(size, AllocationTag::LINEAR_ALLOCATOR);
	}

	void LinearAllocator::destroy()
	{
		if (owns_memory)
			free_memory_platform(memory);

		size = 0;
		allocated = 0;
		memory = 0;
		owns_memory = 0;
	}

	void* LinearAllocator::allocate(uint64 alloc_size)
	{	
		SHMASSERT_MSG(allocated + alloc_size <= size, "Linear allocator ran out of memory!");

		if (alloc_size == 0)
			return 0;

		void* mem = PTR_BYTES_OFFSET(memory, allocated);
		allocated += alloc_size;
		return mem;
	}

	void LinearAllocator::free_all_data()
	{
		allocated = 0;
	}

}

