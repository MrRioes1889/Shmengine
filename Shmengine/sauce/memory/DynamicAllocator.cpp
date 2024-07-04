#include "DynamicAllocator.hpp"

#include "core/Memory.hpp"
#include "core/Assert.hpp"

DynamicAllocator::DynamicAllocator()
{
}

DynamicAllocator::DynamicAllocator(uint64 buffer_size, void* buffer_ptr, uint64 nodes_size, void* nodes_ptr, AllocatorPageSize freelist_page_size, uint32 max_nodes_count_limit)
{
	init(buffer_size, buffer_ptr, nodes_size, nodes_ptr, freelist_page_size, max_nodes_count_limit);
}

DynamicAllocator::~DynamicAllocator()
{
}

void DynamicAllocator::init(uint64 buffer_size, void* buffer_ptr, uint64 nodes_size, void* nodes_ptr, AllocatorPageSize freelist_page_size, uint32 max_nodes_count_limit)
{
	if (max_nodes_count_limit)
		Freelist::get_required_nodes_array_memory_size_by_node_count(max_nodes_count_limit, freelist_page_size);
	else
		Freelist::get_required_nodes_array_memory_size_by_data_size(buffer_size, freelist_page_size);

	freelist.init(buffer_size, nodes_size, nodes_ptr, freelist_page_size, max_nodes_count_limit);

	data = buffer_ptr;
}

void* DynamicAllocator::allocate(uint64 size, uint64* bytes_allocated)
{
	uint64 data_offset;
	if (!freelist.allocate(size, &data_offset, bytes_allocated))
		return 0;

	return PTR_BYTES_OFFSET(data, data_offset);
}

bool32 DynamicAllocator::free(void* data_ptr, uint64* bytes_freed)
{
	SHMASSERT(data_ptr >= data);
	uint64 data_offset = (uint64)data_ptr - (uint64)data;
	return freelist.free(data_offset, bytes_freed);
}

void* DynamicAllocator::reallocate(uint64 requested_size, void* data_ptr, uint64* bytes_freed, uint64* bytes_allocated)
{
	uint64 old_data_offset = (uint64)data_ptr - (uint64)data;
	uint64 old_size = freelist.get_reserved_size(old_data_offset);
	if (old_size >= requested_size)
		return data_ptr;

	// NOTE: Freeing data first to allow the same data block to be part of the new allocation. Feels unsafe but should work fine.
	free(data_ptr, bytes_freed);
	void* new_data = allocate(requested_size, bytes_allocated);
	Memory::copy_memory(data_ptr, new_data, old_size);

	return new_data;
}
