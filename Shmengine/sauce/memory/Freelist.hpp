#pragma once

#include "Defines.hpp"

enum class AllocatorPageSize
{
	MINIMAL = sizeof(void*),
	TINY = 0x10,
	SMALL = 0x20,
	MEDIUM = 0x40,
	LARGE = 0x100
};

struct AllocationReference
{
	uint64 byte_offset;
	uint64 byte_size;
};

struct AllocationReference32
{
	uint32 byte_offset;
	uint32 byte_size;
	AllocationReference32() : byte_offset(0), byte_size(0) {}
	AllocationReference32(AllocationReference ref) : byte_offset((uint32)ref.byte_offset), byte_size((uint32)ref.byte_size) {}
};

struct SHMAPI Freelist
{
	struct Node
	{
		bool32 reserved;
		uint32 page_count;
	};

	static SHMINLINE uint32 get_max_node_count_by_data_size(uint64 data_size, AllocatorPageSize page_size)
	{
		return (uint32)(data_size / (uint32)page_size);
	}

	static SHMINLINE uint64 get_required_nodes_array_memory_size_by_node_count(uint32 node_count_limit)
	{
		return node_count_limit * sizeof(Node);
	}

	Freelist();
	Freelist(uint64 buffer_size, void* nodes_ptr, AllocatorPageSize freelist_page_size, uint32 max_nodes_count_limit);
	~Freelist();

	void init(uint64 buffer_size, void* nodes_ptr, AllocatorPageSize freelist_page_size, uint32 max_nodes_count_limit);
	void resize(uint64 data_buffer_size, void* nodes_ptr, uint32 new_max_nodes_count);
	void clear_nodes();
	void destroy();

	bool8 allocate(uint64 size, AllocationReference* alloc);
	bool8 allocate_aligned(uint64 size, uint16 alignment, AllocationReference* alloc);
	bool32 free(uint64 offset, uint64* bytes_freed = 0);

	int64 get_reserved_size(uint64 offset);

	AllocatorPageSize page_size;
	uint32 max_nodes_count;
	uint32 nodes_count;
	uint32 pages_count;
	Node* nodes;

};
