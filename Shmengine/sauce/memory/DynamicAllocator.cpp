#include "DynamicAllocator.hpp"

#include "core/Memory.hpp"
#include "core/Assert.hpp"

namespace Memory
{

	static void insert_reservation_at(DynamicAllocator* freelist, uint32 index, uint32 reservation_page_count);
	static void remove_reservation_at(DynamicAllocator* freelist, uint32 index);

	static SHMINLINE int32 find_first_free_node(DynamicAllocator* freelist, uint64 size, uint32* out_page_index);
	static SHMINLINE int32 find_allocated_node(DynamicAllocator* freelist, void* data_ptr);

	DynamicAllocator::DynamicAllocator(uint64 buffer_size, void* buffer_ptr, AllocatorPageSize freelist_page_size, void* nodes_ptr, uint32 max_nodes_count_limit)
	{
		init(buffer_size, buffer_ptr, freelist_page_size, nodes_ptr, max_nodes_count_limit);
	}

	DynamicAllocator::~DynamicAllocator()
	{
	}

	void DynamicAllocator::init(uint64 buffer_size, void* buffer_ptr, AllocatorPageSize freelist_page_size, void* nodes_ptr, uint32 max_nodes_count_limit)
	{	

		data_size = buffer_size;
		page_size = freelist_page_size;
		pages_count = (uint32)(buffer_size / (uint32)freelist_page_size);
		if (max_nodes_count_limit && max_nodes_count_limit < pages_count)
			max_nodes_count = max_nodes_count_limit;
		else
			max_nodes_count = pages_count;

		SHMASSERT_MSG(max_nodes_count <= DynamicAllocator::Node::page_count_mask, "Node count for freelist exceeds maximum of 2^31 - 1!");

		data = buffer_ptr;
		nodes = (DynamicAllocator::Node*)nodes_ptr;

		clear_nodes();

	}

	void DynamicAllocator::clear_nodes()
	{
		zero_memory(nodes, max_nodes_count * sizeof(DynamicAllocator::Node));
		nodes[0].value = pages_count;
		nodes_count = 1;
	}

	void* DynamicAllocator::allocate(uint64 size)
	{
		if (nodes_count >= max_nodes_count)
			return 0;

		uint32 page_index = 0;
		int32 node_index = find_first_free_node(this, size, &page_index);
		if (node_index < 0)
			return 0;

		uint32 pages_needed = (uint32)((size / (uint32)page_size) + 1);
		insert_reservation_at(this, node_index, pages_needed);

		uint8* data_ptr = (uint8*)data;
		data_ptr += (page_index * (uint32)page_size);

		// NOTE: Zeroing out allocated memory for now. Perhaps remove that later.
		zero_memory(data_ptr, nodes[node_index].page_count() * (uint32)page_size);
		return data_ptr;
	}

	void DynamicAllocator::free(void* data_ptr)
	{
		uint32 node_index = find_allocated_node(this, data_ptr);
		if (node_index < 0)
			return;

		remove_reservation_at(this, node_index);
	}

	void* DynamicAllocator::reallocate(uint64 requested_size, void* data_ptr)
	{
		uint32 node_index = find_allocated_node(this, data_ptr);
		if (node_index < 0)
			return 0;
		else
		{
			if ((nodes[node_index].page_count() * (uint32)page_size) >= requested_size)
			{
				return data;
			}
			else
			{
				void* dest_mem = allocate(requested_size);
				copy_memory(data, dest_mem, nodes[node_index].page_count() * (uint32)page_size);
				remove_reservation_at(this, node_index);
				return dest_mem;
			}
		}
	}

	static void insert_reservation_at(DynamicAllocator* freelist, uint32 index, uint32 reservation_page_count)
	{

		DynamicAllocator::Node* node = &freelist->nodes[index];
		uint32 page_count = (node->value & DynamicAllocator::Node::page_count_mask);
		bool32 reserved = (node->value & DynamicAllocator::Node::reserved_mask);

		if (page_count == reservation_page_count)
		{
			node->value |= DynamicAllocator::Node::reserved_mask;
			return;
		}
		else
		{

			copy_memory((void*)node, (void*)(node + 1), sizeof(DynamicAllocator::Node) * (freelist->nodes_count - index));

			node->value = reservation_page_count;
			node->value |= DynamicAllocator::Node::reserved_mask;	

			(node + 1)->value &= DynamicAllocator::Node::page_count_mask;
			(node + 1)->value -= reservation_page_count;

			freelist->nodes_count++;

			return;
		}
	}

	static void remove_reservation_at(DynamicAllocator* freelist, uint32 index)
	{
		DynamicAllocator::Node* nodes = freelist->nodes;

		int32 merge_offset = 0;
		bool32 both_chunks_free = false;
		uint32 freed_page_count = (nodes[index].value & DynamicAllocator::Node::page_count_mask);
		//uint32 freed_page_index = chunks[index].page_index;
		if (index > 0)
		{
			if (!(nodes[index - 1].reserved()))
				merge_offset = -1;
		}
		if (index < freelist->max_nodes_count - 1)
		{
			if (!(nodes[index + 1].reserved()))
			{
				if (merge_offset == 0)
					merge_offset = 1;
				else
					both_chunks_free = true;
			}

		}

		if (merge_offset != 0)
		{

			nodes[index + merge_offset].value += nodes[index].page_count();

			if (!both_chunks_free)
			{
				copy_memory((void*)(&nodes[index] + 1), (void*)&nodes[index], sizeof(DynamicAllocator::Node) * (freelist->nodes_count - index - 1));
				nodes[freelist->nodes_count - 1] = {};
				freelist->nodes_count--;
			}
			else
			{
				nodes[index + merge_offset].value += nodes[index + 1].page_count();
				copy_memory((void*)(&nodes[index] + 2), (void*)&nodes[index], sizeof(DynamicAllocator::Node) * (freelist->nodes_count - index - 2));
				nodes[freelist->nodes_count - 1] = {};
				nodes[freelist->nodes_count - 2] = {};
				freelist->nodes_count -= 2;
			}
		}
		else
		{
			nodes[index].value &= DynamicAllocator::Node::page_count_mask;
		}
	}

	static SHMINLINE int32 find_first_free_node(DynamicAllocator* freelist, uint64 size, uint32* out_page_index)
	{
		int32 allocation_index = -1;
		uint32 page_index = 0;
		for (uint32 i = 0; i < freelist->nodes_count; i++)
		{
			if (!freelist->nodes[i].reserved() && (freelist->nodes[i].page_count() * (uint32)freelist->page_size) >= size)
			{
				allocation_index = (int32)i;
				break;
			}
			else
			{
				page_index += freelist->nodes[i].page_count();
			}
		}

		*out_page_index = page_index;
		return allocation_index;
	}

	static SHMINLINE int32 find_allocated_node(DynamicAllocator* freelist, void* data_ptr)
	{
		SHMASSERT(data_ptr >= freelist->data);
		uint64 expected_offset = (uint64)data_ptr - (uint64)freelist->data;

		int32 freeing_index = -1;
		uint64 offset = 0;
		for (uint32 i = 0; i < freelist->nodes_count; i++)
		{
			if (offset == expected_offset)
			{
				freeing_index = (int32)i;
				break;
			}

			offset += (freelist->nodes[i].page_count() * (uint32)freelist->page_size);
			SHMASSERT_MSG(offset <= expected_offset, "Error: Freeing data block does not align with chunks of mem arena!");
		}

		return freeing_index;
	}

}