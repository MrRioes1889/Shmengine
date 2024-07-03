#pragma once

#include "Defines.hpp"
#include "core/Memory.hpp"
#include "Sarray.hpp"

namespace BufferFlag
{
	enum Value
	{
		EXTERNAL_MEMORY = 1 << 0
	};
}

struct SHMAPI Buffer
{
	Buffer() : size(0), flags(0), data(0), allocation_tag((uint16)AllocationTag::UNKNOWN) {};
	Buffer(uint32 reserve_size, uint32 creation_flags, AllocationTag tag = AllocationTag::UNKNOWN, void* memory = 0);
	~Buffer();

	SHMINLINE Buffer(const Buffer& other);
	SHMINLINE Buffer& operator=(const Buffer& other);

	SHMINLINE void steal(Buffer& other)
	{
		data = other.data;
		size = other.size;
		flags = other.flags;
		allocation_tag = other.allocation_tag;

		other.data = 0;
		other.size = 0;
	}

	SHMINLINE Buffer(Buffer&& other) noexcept
	{
		steal(other);
	};
	SHMINLINE Buffer& operator=(Buffer&& other) noexcept
	{
		free_data();
		steal(other);
		return *this;
	};

	template<typename SubT>
	SHMINLINE SubT& get_as(uint32 index)
	{
		uint32 size_needed = sizeof(SubT) * (index + 1);
		SHMASSERT_MSG(size_needed <= size, "Index does not lie within bounds of Buffer.");
		return ((SubT*)data)[index];
	}

	template<typename SubT>
	SHMINLINE const SubT& get_as(uint32 index) const
	{
		uint32 size_needed = sizeof(SubT) * (index + 1);
		SHMASSERT_MSG(size_needed <= size, "Index does not lie within bounds of Buffer.");
		return ((SubT*)data)[index];
	}

	// NOTE: Call for already instantiated arrays
	void init(uint64 reserve_size, uint32 creation_flags, AllocationTag tag = AllocationTag::UNKNOWN, void* memory = 0);
	void free_data();

	void resize(uint64 new_size, void* memory = 0);
	void clear();

	void* data;
	uint64 size;
	uint16 allocation_tag;
	uint16 flags;
};


SHMINLINE Buffer::Buffer(const Buffer& other)
{
	init(other.size, other.flags, (AllocationTag)other.allocation_tag);
	Memory::copy_memory(other.data, data, size);
}

SHMINLINE Buffer& Buffer::operator=(const Buffer& other)
{
	free_data();
	init(other.size, other.flags, (AllocationTag)other.allocation_tag);
	Memory::copy_memory(other.data, data, size);
	return *this;
}