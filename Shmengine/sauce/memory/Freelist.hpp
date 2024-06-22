#pragma once

#pragma once

#include "Defines.hpp"

enum class AllocatorPageSize
{
	MINIMAL = sizeof(void*),
	SMALL = 0x20,
	MEDIUM = 0x40,
	LARGE = 0x100
};

struct Freelist
{

	struct Node
	{
		bool32 reserved;
		uint32 page_count;
	};

	static SHMINLINE uint64 get_required_nodes_array_memory_size_by_node_count(uint32 node_count_limit, AllocatorPageSize page_size)
	{
		return node_count_limit * sizeof(Node);
	}

	static SHMINLINE uint64 get_required_nodes_array_memory_size_by_data_size(uint64 data_size, AllocatorPageSize page_size)
	{
		return data_size / (uint32)page_size * sizeof(Node);
	}

	Freelist();
	Freelist(uint64 buffer_size, uint64 in_nodes_size, void* nodes_ptr, AllocatorPageSize freelist_page_size, uint32 max_nodes_count_limit = 0);
	~Freelist();

	void init(uint64 buffer_size, uint64 in_nodes_size, void* nodes_ptr, AllocatorPageSize freelist_page_size, uint32 max_nodes_count_limit = 0);
	void resize(void* nodes_ptr, uint64 in_nodes_size);
	void clear_nodes();
	void destroy();

	bool32 allocate(uint64 size, uint64* out_offset);
	bool32 free(uint64 offset);

	int64 get_reserved_size(uint64 offset);

	uint64 nodes_size = 0;
	AllocatorPageSize page_size = AllocatorPageSize::MINIMAL;
	uint32 max_nodes_count = 0;
	uint32 nodes_count = 0;
	uint32 pages_count = 0;
	Node* nodes = 0;

};
