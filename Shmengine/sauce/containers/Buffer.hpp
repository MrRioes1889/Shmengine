#pragma once

#include "Defines.hpp"
#include "core/Memory.hpp"

struct Buffer
{
	Buffer(const Buffer& other) = delete;
	Buffer(Buffer&& other) = delete;

	Buffer() : size(0), data(0), allocation_tag((uint16)AllocationTag::UNKNOWN), owns_memory(true) {};
	Buffer(uint32 reserve_size, AllocationTag tag = AllocationTag::UNKNOWN, void* memory = 0);
	~Buffer();

	// NOTE: Call for already instantiated arrays
	void init(uint32 reserve_size, AllocationTag tag = AllocationTag::UNKNOWN, void* memory = 0);
	void free_data();

	SHMINLINE void move(Buffer& other)
	{
		data = other.data;
		size = other.size;
		allocation_tag = other.allocation_tag;

		other.data = 0;
		other.size = 0;
	}

	void clear();

	void* data;
	uint32 size;
	uint16 allocation_tag;
	bool8 owns_memory;
};