#pragma once

#include "Defines.hpp"
#include "core/Memory.hpp"

struct Buffer
{
	Buffer(const Buffer& other) = delete;
	Buffer(Buffer&& other) = delete;

	Buffer() : size(0), data(0), allocation_tag(AllocationTag::UNKNOWN) {};
	Buffer(uint32 reserve_size, AllocationTag tag = AllocationTag::UNKNOWN);
	~Buffer();

	// NOTE: Call for already instantiated arrays
	void init(uint32 reserve_size, AllocationTag tag = AllocationTag::UNKNOWN);
	void free_data();

	void clear();

	void* data;
	uint32 size;
	AllocationTag allocation_tag;
};