#include "DynamicAllocator.hpp"

#include "core/Memory.hpp"
#include "core/Assert.hpp"
#include "utility/Utility.hpp"

struct AllocHeader
{
	uint16 alignment_offset;
	uint8 allocation_tag;
};

DynamicAllocator::DynamicAllocator()
{
}

DynamicAllocator::DynamicAllocator(uint64 buffer_size, void* buffer_ptr, uint64 nodes_buffer_size, void* nodes_ptr, AllocatorPageSize freelist_page_size, uint32 max_nodes_count_limit)
{
	init(buffer_size, buffer_ptr, nodes_buffer_size, nodes_ptr, freelist_page_size, max_nodes_count_limit);
}

DynamicAllocator::~DynamicAllocator()
{
}

void DynamicAllocator::init(uint64 buffer_size, void* buffer_ptr, uint64 nodes_buffer_size, void* nodes_ptr, AllocatorPageSize freelist_page_size, uint32 max_nodes_count_limit)
{
	uint32 max_nodes_count = (uint32)(nodes_buffer_size / (uint32)freelist_page_size);
	if (max_nodes_count_limit && max_nodes_count_limit < max_nodes_count)
		max_nodes_count = max_nodes_count_limit;

	freelist.init(buffer_size, nodes_ptr, freelist_page_size, max_nodes_count);

	data = buffer_ptr;
}

void* DynamicAllocator::allocate(uint64 size, AllocationTag tag, uint16 alignment, uint64* bytes_allocated)
{
	size += (alignment > 1 ? alignment : 0) + sizeof(AllocHeader);
	uint64 data_offset;
	if (!freelist.allocate(size, &data_offset, bytes_allocated))
		return 0;

	uint64 total_data_offset = (uint64)data + data_offset + sizeof(AllocHeader);
	uint64 alignment_offset = get_aligned(total_data_offset, alignment) - total_data_offset;	
	uint8* ret = PTR_BYTES_OFFSET(data, data_offset + alignment_offset);
	((AllocHeader*)ret)->alignment_offset = (uint16)alignment_offset;
	((AllocHeader*)ret)->allocation_tag = (uint8)tag;
	ret += sizeof(AllocHeader);

	return ret;
}

bool32 DynamicAllocator::free(void* data_ptr, AllocationTag* out_tag, uint64* bytes_freed)
{

	SHMASSERT(data_ptr >= data);
	uint8* ptr = PTR_BYTES_OFFSET(data_ptr, -(int32)sizeof(AllocHeader));
	uint16 alignment_offset = ((AllocHeader*)ptr)->alignment_offset;
	*out_tag = (AllocationTag)((AllocHeader*)ptr)->allocation_tag;
	ptr = PTR_BYTES_OFFSET(ptr, -(int32)alignment_offset);
	uint64 data_offset = (uint64)ptr - (uint64)data;

	return freelist.free(data_offset, bytes_freed);
}

void* DynamicAllocator::reallocate(uint64 requested_size, void* data_ptr, AllocationTag* out_tag, uint16 alignment, uint64* bytes_freed, uint64* bytes_allocated)
{
	requested_size += (alignment > 1 ? alignment : 0) + sizeof(AllocHeader);
	uint8* ptr = PTR_BYTES_OFFSET(data_ptr, -(int32)sizeof(AllocHeader));	
	uint16 old_alignment_offset = ((AllocHeader*)ptr)->alignment_offset;
	*out_tag = (AllocationTag)((AllocHeader*)ptr)->allocation_tag;
	ptr = PTR_BYTES_OFFSET(ptr, -old_alignment_offset);

	uint64 old_data_offset = (uint64)ptr - (uint64)data;
	uint64 old_size = freelist.get_reserved_size(old_data_offset);
	if (old_size >= requested_size)
		return data_ptr;

	// NOTE: Freeing data first to allow the same data block to be part of the new allocation. Feels unsafe but should work fine.
	freelist.free(old_data_offset, bytes_freed);
	void* new_data = allocate(requested_size, *out_tag, alignment, bytes_allocated);
	void* old_data_ptr = PTR_BYTES_OFFSET(data, old_data_offset + old_alignment_offset + sizeof(AllocHeader));
	Memory::copy_memory(old_data_ptr, new_data, requested_size);

	return new_data;
}
