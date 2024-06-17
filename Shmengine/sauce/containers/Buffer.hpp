#pragma once

#include "Defines.hpp"
#include "core/Memory.hpp"

struct SHMAPI Buffer
{
	Buffer() : size(0), data(0), allocation_tag((uint16)AllocationTag::UNKNOWN), owns_memory(true) {};
	Buffer(uint32 reserve_size, AllocationTag tag = AllocationTag::UNKNOWN, void* memory = 0);
	~Buffer();

	Buffer(const Buffer& other)=default;
	Buffer& operator=(const Buffer& other)=default;

	SHMINLINE void steal(Buffer& other)
	{
		data = other.data;
		size = other.size;
		owns_memory = other.owns_memory;
		allocation_tag = other.allocation_tag;

		other.data = 0;
		other.size = 0;
	}

	Buffer(Buffer&& other) noexcept
	{
		steal(other);
	};
	Buffer& operator=(Buffer&& other) noexcept
	{
		steal(other);
		return *this;
	};

	// NOTE: Call for already instantiated arrays
	void init(uint64 reserve_size, AllocationTag tag = AllocationTag::UNKNOWN, void* memory = 0);
	void free_data();

	void resize(uint64 new_size, void* memory = 0);
	void clear();

	void* data = 0;
	uint64 size = 0;
	uint16 allocation_tag;
	bool8 owns_memory = false;
};