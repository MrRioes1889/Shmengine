#include "Freelist.hpp"

#include "core/Memory.hpp"
#include "core/Assert.hpp"
#include "utility/Math.hpp"
#include "utility/Utility.hpp"

static bool8 insert_reservation_at(Freelist* freelist, uint64 index, uint32 reservation_page_count, uint16 node_page_offset);
static bool8 remove_reservation_at(Freelist* freelist, uint64 index);

static SHMINLINE int64 find_first_free_node(Freelist* freelist, uint32 pages_needed, uint32* out_page_index);
static SHMINLINE int64 find_first_free_node_aligned(Freelist* freelist, uint32 pages_needed, uint16 page_alignment, uint16* out_page_alignment_offset, uint32* out_page_index);
static SHMINLINE int64 find_allocated_node(Freelist* freelist, uint64 expected_offset);

Freelist::Freelist() :
page_size(AllocatorPageSize::MINIMAL),
max_nodes_count(0),
nodes_count(0),
pages_count(0),
nodes(0)
{
}

Freelist::Freelist(uint64 buffer_size, void* nodes_ptr, AllocatorPageSize freelist_page_size, uint32 max_nodes_count_limit)
{
	init(buffer_size, nodes_ptr, freelist_page_size, max_nodes_count_limit);
}

Freelist::~Freelist()
{
}

void Freelist::init(uint64 buffer_size, void* nodes_ptr, AllocatorPageSize freelist_page_size, uint32 max_nodes_count_limit)
{
	page_size = freelist_page_size;
	pages_count = (uint32)(buffer_size / (uint32)freelist_page_size);
	if (max_nodes_count_limit && max_nodes_count_limit < pages_count)
		max_nodes_count = max_nodes_count_limit;
	else
		max_nodes_count = pages_count;

	nodes_count = 1;
	nodes = (Node*)nodes_ptr;

	clear_nodes();

}

void Freelist::resize(uint64 data_buffer_size, void* nodes_ptr, uint32 new_max_nodes_count)
{
	uint32 new_pages_count = (uint32)(data_buffer_size / (uint32)page_size);
	SHMASSERT(new_max_nodes_count >= max_nodes_count && new_pages_count >= pages_count);
	max_nodes_count = new_max_nodes_count;
	nodes = (Node*)nodes_ptr;
	uint32 pages_count_diff = new_pages_count - pages_count;
	pages_count = new_pages_count;

	if (!nodes[nodes_count - 1].reserved)
	{
		nodes[nodes_count - 1].page_count += pages_count_diff;
	}
	else
	{
		SHMASSERT(nodes_count < max_nodes_count);
		nodes_count++;
		nodes[nodes_count - 1].page_count = pages_count_diff;
		nodes[nodes_count - 1].reserved = false;
	}
}

void Freelist::clear_nodes()
{
	Memory::zero_memory(nodes, nodes_count * sizeof(Node));
	nodes[0].reserved = false;
	nodes[0].page_count = pages_count;
	nodes_count = 1;
}

void Freelist::destroy()
{
	nodes = 0;
	nodes_count = 0;
	max_nodes_count = 0;
	pages_count = 0;
}

bool8 Freelist::allocate(uint64 size, AllocationReference* alloc)
{
	return allocate_aligned(size, (uint16)page_size, alloc);
}

bool8 Freelist::allocate_aligned(uint64 size, uint16 alignment, AllocationReference* alloc)
{
	*alloc = {.byte_offset = Constants::max_u64, .byte_size = 0};

	if (!size)
		return false;

	if ((uint16)page_size % alignment == 0)
		alignment = 1;

	if (alignment > 1 && (alignment % (uint16)page_size) != 0)
		return false;

	uint32 nodes_added = 1 + (alignment > 1 ? 1 : 0);
	if ((nodes_count + nodes_added) > max_nodes_count)
		return false;

	uint16 page_alignment = alignment / (uint16)page_size;

	uint32 pages_needed = (uint32)((size / (uint64)page_size));
	if (size % (uint64)page_size != 0)
		pages_needed++;

	uint32 page_index = 0;
	int64 node_index = 0;
	uint16 node_page_offset = 0;

	if (alignment > 1)
		node_index = find_first_free_node_aligned(this, pages_needed, page_alignment, &node_page_offset, &page_index);
	else
		node_index = find_first_free_node(this, pages_needed, &page_index);

	if (node_index < 0)
		return false;

	insert_reservation_at(this, (uint64)node_index, pages_needed, node_page_offset);

	alloc->byte_size = pages_needed * (uint32)page_size;
	alloc->byte_offset = (uint64)page_index * (uint32)page_size;
	return true;
}

bool8 Freelist::free(uint64 offset, uint64* pages_freed)
{
	int64 node_index = find_allocated_node(this, offset);
	if (node_index < 0)
		return false;

	if (pages_freed)
		*pages_freed = nodes[node_index].page_count * (uint32)page_size;

	remove_reservation_at(this, (uint64)node_index);

	return true;
}

int64 Freelist::get_reserved_size(uint64 offset)
{
	int64 node_index = find_allocated_node(this, offset);
	if (node_index < 0)
		return -1;

	return (nodes[node_index].page_count * (int64)page_size);
}

static bool8 insert_reservation_at(Freelist* freelist, uint64 index, uint32 reservation_page_count, uint16 node_page_offset)
{

	Freelist::Node* node = &freelist->nodes[index];

	if (node->page_count == (reservation_page_count + node_page_offset))
	{
		node->reserved = true;
		return true;
	}

	if (!node_page_offset)
	{
		Memory::copy_memory((void*)node, (void*)(node + 1), sizeof(Freelist::Node) * (freelist->nodes_count - index));

		(node + 1)->reserved = false;
		(node + 1)->page_count = node->page_count - reservation_page_count;

		node->page_count = reservation_page_count;
		node->reserved = true;

		freelist->nodes_count++;
	}
	else
	{
		Memory::copy_memory((void*)node, (void*)(node + 2), sizeof(Freelist::Node) * (freelist->nodes_count - index));

		node->page_count = node_page_offset;
		node->reserved = false;

		(node + 1)->page_count = reservation_page_count;
		(node + 1)->reserved = true;
	
		(node + 2)->page_count = node->page_count - reservation_page_count;	
		(node + 2)->reserved = false;

		freelist->nodes_count += 2;
	}

	return true;
}

static bool8 remove_reservation_at(Freelist* freelist, uint64 index)
{
	Freelist::Node* nodes = freelist->nodes;

	int32 merge_offset = 0;
	bool8 both_chunks_free = false;

	if (index > 0)
	{
		if (!(nodes[index - 1].reserved))
			merge_offset = -1;
	}
	if (index < freelist->max_nodes_count - 1)
	{
		if (!(nodes[index + 1].reserved))
		{
			if (merge_offset == 0)
				merge_offset = 1;
			else
				both_chunks_free = true;
		}

	}

	if (merge_offset != 0)
	{

		nodes[index + merge_offset].page_count += nodes[index].page_count;

		if (!both_chunks_free)
		{
			Memory::copy_memory((void*)(&nodes[index] + 1), (void*)&nodes[index], sizeof(Freelist::Node) * (freelist->nodes_count - index - 1));
			nodes[freelist->nodes_count - 1] = {};
			freelist->nodes_count--;
		}
		else
		{
			nodes[index + merge_offset].page_count += nodes[index + 1].page_count;
			Memory::copy_memory((void*)(&nodes[index] + 2), (void*)&nodes[index], sizeof(Freelist::Node) * (freelist->nodes_count - index - 2));
			nodes[freelist->nodes_count - 1] = {};
			nodes[freelist->nodes_count - 2] = {};
			freelist->nodes_count -= 2;
		}
	}
	else
	{
		nodes[index].reserved = false;
	}

	return true;
}

static SHMINLINE int64 find_first_free_node(Freelist* freelist, uint32 pages_needed, uint32* out_page_index)
{
	int64 allocation_index = -1;
	uint32 page_index = 0;
	for (uint32 i = 0; i < freelist->nodes_count; i++)
	{
		if (!freelist->nodes[i].reserved && (freelist->nodes[i].page_count >= pages_needed))
		{
			allocation_index = (int64)i;
			break;
		}
		else
		{
			page_index += freelist->nodes[i].page_count;
		}
	}

	*out_page_index = page_index;
	return allocation_index;
}

static SHMINLINE int64 find_first_free_node_aligned(Freelist* freelist, uint32 pages_needed, uint16 page_alignment, uint16* out_page_alignment_offset, uint32* out_page_index)
{
	int64 allocation_index = -1;
	uint32 page_index = 0;
	for (uint32 i = 0; i < freelist->nodes_count; i++)
	{
		if (freelist->nodes[i].reserved || (freelist->nodes[i].page_count < pages_needed))
		{
			page_index += freelist->nodes[i].page_count;
			continue;
		}

		uint64 aligned_page_offset = get_aligned(page_index, page_alignment) - page_index;
		if (freelist->nodes[i].page_count < (aligned_page_offset + pages_needed))
		{
			page_index += freelist->nodes[i].page_count;
			continue;
		}
		
		allocation_index = (int64)i;
		*out_page_alignment_offset = (uint16)aligned_page_offset;
		break;

	}

	*out_page_index = page_index + *out_page_alignment_offset;
	return allocation_index;
}

static SHMINLINE int64 find_allocated_node(Freelist* freelist, uint64 expected_offset)
{

	int64 freeing_index = -1;
	uint64 offset = 0;
	for (uint32 i = 0; i < freelist->nodes_count; i++)
	{
		if (offset == expected_offset)
		{
			freeing_index = (int64)i;
			break;
		}

		offset += (freelist->nodes[i].page_count * (uint32)freelist->page_size);
		SHMASSERT_MSG(offset <= expected_offset, "Error: Freeing data block does not align with chunks of mem arena!");
	}

	return freeing_index;
}
