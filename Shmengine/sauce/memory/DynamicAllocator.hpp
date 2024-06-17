#pragma once

#include "Defines.hpp"

enum class AllocatorPageSize
{
	MINIMAL = sizeof(void*),
	SMALL = 0x20,
	MEDIUM = 0x40,
	LARGE = 0x100
};

namespace Memory
{

	struct DynamicAllocator
	{		

		struct Node
		{
			uint32 value;
			static const uint32 reserved_mask = 0x80000000;
			static const uint32 page_count_mask = reserved_mask - 1;

			SHMINLINE bool32 reserved() { return value & reserved_mask; }
			SHMINLINE uint32 page_count() { return value & page_count_mask; }
		};

		DynamicAllocator() = delete;
		DynamicAllocator(const DynamicAllocator& other) = delete;
		DynamicAllocator(DynamicAllocator&& other) = delete;

		static SHMINLINE uint64 get_required_nodes_array_memory_size_by_node_count(uint32 node_count_limit, AllocatorPageSize page_size)
		{
			return node_count_limit * sizeof(Node);
		}

		static SHMINLINE uint64 get_required_nodes_array_memory_size_by_data_size(uint64 data_size, AllocatorPageSize page_size)
		{
			return data_size / (uint32)page_size * sizeof(Node);
		}

		DynamicAllocator(uint64 buffer_size, void* buffer_ptr, AllocatorPageSize freelist_page_size, void* nodes_ptr, uint32 max_nodes_count_limit = 0);
		~DynamicAllocator();

		void init(uint64 buffer_size, void* buffer_ptr, AllocatorPageSize freelist_page_size, void* nodes_ptr, uint32 max_nodes_count_limit = 0);
		void clear_nodes();

		void* allocate(uint64 size);
		void free(void* data_ptr);
		void* reallocate(uint64 requested_size, void* data_ptr);

		uint64 data_size = 0;
		AllocatorPageSize page_size = AllocatorPageSize::MINIMAL;
		uint32 pages_count = 0;
		uint32 max_nodes_count = 0;
		uint32 nodes_count = 0;
		Node* nodes = 0;
		void* data = 0;	

	};


}
