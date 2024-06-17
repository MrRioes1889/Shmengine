#include "Buffer.hpp"

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
	if (data)
		free_data();

	owns_memory = (memory == 0);
	allocation_tag = (uint16)tag;
	size = reserve_size;
	if (owns_memory)
		data = Memory::allocate(size, true, (AllocationTag)allocation_tag);
	else
		data = memory;
	clear();
}

void Buffer::free_data()
{
	if (owns_memory && data)
		Memory::free_memory(data, true, (AllocationTag)allocation_tag);

	data = 0;
	size = 0;
}

void Buffer::clear()
{
	Memory::zero_memory(data, size);
}
