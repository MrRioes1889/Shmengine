#pragma once

#include "Defines.hpp"
#include "Freelist.hpp"

struct DynamicAllocator
{

	DynamicAllocator();
	DynamicAllocator(uint64 buffer_size, void* buffer_ptr, uint64 nodes_size, void* nodes_ptr, AllocatorPageSize freelist_page_size, uint32 max_nodes_count_limit = 0);
	~DynamicAllocator();

	void init(uint64 buffer_size, void* buffer_ptr, uint64 nodes_size, void* nodes_ptr, AllocatorPageSize freelist_page_siz, uint32 max_nodes_count_limit = 0);

	void* allocate(uint64 size, uint64* bytes_allocated = 0);
	bool32 free(void* data_ptr, uint64* bytes_freed = 0);
	void* reallocate(uint64 requested_size, void* data_ptr, uint64* bytes_freed = 0, uint64* bytes_allocated = 0);

	Freelist freelist;
	void* data = 0;

};
