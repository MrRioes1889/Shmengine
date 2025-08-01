#include "Buffer.hpp"
#include "core/Assert.hpp"

Buffer::Buffer(uint32 reserve_size, uint32 creation_flags, AllocationTag tag, void* memory)
{
	data = 0;
	init(reserve_size, creation_flags, tag, memory);
}

Buffer::~Buffer()
{
	if (data && !(flags & BufferFlags::ExternalMemory))
		Memory::free_memory(data);
}

void Buffer::init(uint64 reserve_size, uint32 creation_flags, AllocationTag tag, void* memory)
{
	SHMASSERT(!data);

	allocation_tag = (uint16)tag;
	size = reserve_size;
	flags = (uint16)creation_flags;

	if (memory)
	{
		flags |= SarrayFlags::ExternalMemory;
		data = memory;
	}
	else
	{
		flags &= ~SarrayFlags::ExternalMemory;
		data = Memory::allocate(size, (AllocationTag)allocation_tag);
	}
}

void Buffer::free_data()
{
	if (data && !(flags & BufferFlags::ExternalMemory))
	{
		if (flags & BufferFlags::PlatformAllocation)
			Memory::free_memory_platform(data);
		else
			Memory::free_memory(data);
	}

	data = 0;
	size = 0;
}

void Buffer::resize(uint64 new_size, void* memory)
{
	if (data && !(flags & BufferFlags::ExternalMemory))
		data = Memory::reallocate(new_size, data);
	else
		data = memory;

	size = new_size;
}

void Buffer::clear()
{
	Memory::zero_memory(data, size);
}
