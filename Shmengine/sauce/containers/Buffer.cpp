#include "Buffer.hpp"

Buffer::Buffer(uint32 reserve_size, AllocationTag tag)
{
	init(size, tag);
}

Buffer::~Buffer()
{
	if (data)
		Memory::free_memory(data, true, allocation_tag);
}

void Buffer::init(uint32 reserve_size, AllocationTag tag)
{
	if (data)
		free_data();

	allocation_tag = tag;
	size = reserve_size;
	data = Memory::allocate(size, true, allocation_tag);
	clear();
}

void Buffer::free_data()
{
	if (data)
		Memory::free_memory(data, true, allocation_tag);

	data = 0;
	size = 0;
}

void Buffer::clear()
{
	Memory::zero_memory(data, size);
}
