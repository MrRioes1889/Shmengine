#include "Buffer.hpp"
#include "core/Assert.hpp"

Buffer::Buffer(uint32 reserve_size, uint32 creation_flags, AllocationTag tag, void* memory)
{
	data = 0;
	init(reserve_size, creation_flags, tag, memory);
}

Buffer::~Buffer()
{
	if (data && !(flags & BufferFlags::EXTERNAL_MEMORY))
		Memory::free_memory(data);
}

void Buffer::init(uint64 reserve_size, uint32 creation_flags, AllocationTag tag, void* memory)
{
	SHMASSERT(!data);

	flags = (uint16)creation_flags;
	allocation_tag = (uint16)tag;
	size = reserve_size;
	if (!memory)
	{
		data = Memory::allocate(size, (AllocationTag)allocation_tag);
	}
	else
	{
		data = memory;
	}		
}

void Buffer::free_data()
{
	if (data && !(flags & BufferFlags::EXTERNAL_MEMORY))
	{
		if (flags & BufferFlags::PLATFORM_ALLOCATION)
			Memory::free_memory_platform(data);
		else
			Memory::free_memory(data);
	}
		

	data = 0;
	size = 0;
}

void Buffer::resize(uint64 new_size, void* memory)
{
	if (data && !(flags & BufferFlags::EXTERNAL_MEMORY))
		data = Memory::reallocate(new_size, data);
	else
		data = memory;

	size = new_size;
}

void Buffer::clear()
{
	Memory::zero_memory(data, size);
}
