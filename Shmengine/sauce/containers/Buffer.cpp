#include "Buffer.hpp"
#include "core/Assert.hpp"

Buffer::Buffer(uint32 reserve_size, AllocationTag tag, void* memory)
{
	data = 0;
	init(size, tag, memory);
}

Buffer::~Buffer()
{
	if (owns_memory && data)
		Memory::free_memory(data, true, (AllocationTag)allocation_tag);
}

void Buffer::init(uint64 reserve_size, AllocationTag tag, void* memory)
{
	SHMASSERT(!data);

	owns_memory = (memory == 0);
	allocation_tag = (uint16)tag;
	size = reserve_size;
	if (owns_memory)
		data = Memory::allocate(size, true, (AllocationTag)allocation_tag);
	else
	{
		data = memory;
		clear();
	}		
}

void Buffer::free_data()
{
	if (owns_memory && data)
		Memory::free_memory(data, true, (AllocationTag)allocation_tag);

	data = 0;
	size = 0;
}

void Buffer::resize(uint64 new_size, void* memory)
{
	if (owns_memory && data)
		data = Memory::reallocate(new_size, memory, true, (AllocationTag)allocation_tag);
	else
		data = memory;

	size = new_size;
}

void Buffer::clear()
{
	Memory::zero_memory(data, size);
}
