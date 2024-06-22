#include "Freelist.hpp"

#include "core/Memory.hpp"
#include "core/Assert.hpp"
#include "utility/Math.hpp"

static bool32 insert_reservation_at(Freelist* freelist, uint64 index, uint32 reservation_page_count);
static bool32 remove_reservation_at(Freelist* freelist, uint64 index);

static SHMINLINE int64 find_first_free_node(Freelist* freelist, uint32 pages_needed, uint32* out_page_index);
static SHMINLINE int64 find_allocated_node(Freelist* freelist, uint64 expected_offset);

Freelist::Freelist()
{
}

Freelist::Freelist(uint64 buffer_size, uint64 in_nodes_size, void* nodes_ptr, AllocatorPageSize freelist_page_size, uint32 max_nodes_count_limit)
{
	init(buffer_size, nodes_size, nodes_ptr, freelist_page_size, max_nodes_count_limit);
}

Freelist::~Freelist()
{
}

void Freelist::init(uint64 buffer_size, uint64 in_nodes_size, void* nodes_ptr, AllocatorPageSize freelist_page_size, uint32 max_nodes_count_limit)
{

	nodes_size = in_nodes_size;
	page_size = freelist_page_size;
	pages_count = (uint32)(buffer_size / (uint32)freelist_page_size);
	if (max_nodes_count_limit && max_nodes_count_limit < pages_count)
		max_nodes_count = max_nodes_count_limit;
	else
		max_nodes_count = pages_count;

	nodes = (Freelist::Node*)nodes_ptr;

	clear_nodes();

}

void Freelist::resize(void* nodes_ptr, uint64 in_nodes_size)
{
	SHMASSERT(in_nodes_size < (max_nodes_count * sizeof(Freelist::Node)));
	nodes_size = in_nodes_size;
	nodes = (Freelist::Node*)nodes_ptr;
}

void Freelist::clear_nodes()
{
	Memory::zero_memory(nodes, max_nodes_count * sizeof(Freelist::Node));
	nodes[0].reserved = false;
	nodes[0].page_count = pages_count;
	nodes_count = 1;
}

void Freelist::destroy()
{
	nodes = 0;
	nodes_count = 0;
	nodes_size = 0;
	max_nodes_count = 0;
	pages_count = 0;
}

bool32 Freelist::allocate(uint64 size, uint64* out_offset)
{
	if (nodes_count >= max_nodes_count)
		return false;

	uint32 pages_needed = (uint32)((size / (uint64)page_size));
	if (size % (uint64)page_size != 0)
		pages_needed++;

	uint32 page_index = 0;
	int64 node_index = find_first_free_node(this, pages_needed, &page_index);
	if (node_index < 0)
		return false;
	
	insert_reservation_at(this, (uint64)node_index, pages_needed);

	*out_offset = (uint64)page_index * (uint32)page_size;
	return true;
}

bool32 Freelist::free(uint64 offset)
{
	int64 node_index = find_allocated_node(this, offset);
	if (node_index < 0)
		return false;

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

static bool32 insert_reservation_at(Freelist* freelist, uint64 index, uint32 reservation_page_count)
{

	Freelist::Node* node = &freelist->nodes[index];

	if (node->page_count == reservation_page_count)
	{
		node->reserved = true;
		return true;
	}
	else
	{

		Memory::copy_memory((void*)node, (void*)(node + 1), sizeof(Freelist::Node) * (freelist->nodes_count - index));

		(node + 1)->reserved = false;
		(node + 1)->page_count = node->page_count - reservation_page_count;

		node->page_count = reservation_page_count;
		node->reserved = true;	

		freelist->nodes_count++;

		return true;
	}
}

static bool32 remove_reservation_at(Freelist* freelist, uint64 index)
{
	Freelist::Node* nodes = freelist->nodes;

	int32 merge_offset = 0;
	bool32 both_chunks_free = false;

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
