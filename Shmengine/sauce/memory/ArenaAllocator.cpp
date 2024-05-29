#include "ArenaAllocator.hpp"
#include "core/Memory.hpp"
#include "core/Assert.hpp"

namespace Memory
{
	static void init_mem_chunks(ArenaAllocator* arena);
	static ArenaPageChunk* insert_reservation_at(ArenaAllocator* arena, uint32 index, uint32 reservation_page_count);
	static bool8 remove_reservation_at(ArenaAllocator* arena, uint32 index);

	void arena_create(uint64 size, ArenaPageType page_type, ArenaAllocator* out_arena)
	{
		uint32 page_size = 0;
		uint32 page_count = 0;

		switch (page_type)
		{
		case ArenaPageType::small_pages:
		{
			page_size = MEMARENA_SMALL_PAGE_SIZE;
			break;
		}
		case ArenaPageType::medium_pages:
		{
			page_size = MEMARENA_MEDIUM_PAGE_SIZE;
			break;
		}
		case ArenaPageType::large_pages:
		{
			page_size = MEMARENA_LARGE_PAGE_SIZE;
			break;
		}
		}

		page_count = (uint32)((size / page_size) + 1);

		uint64 arena_size =	(page_count * sizeof(ArenaPageChunk)) +	(page_count * page_size);
		void* arena_data = (void*)allocate(arena_size, true, AllocationTag::RAW);
		zero_memory((void*)arena_data, arena_size);

		uint64 data_byte_offset =  (page_count * sizeof(ArenaPageChunk));

		out_arena->page_size = page_size;
		out_arena->page_count = page_count;
		out_arena->page_type = page_type;
		out_arena->arena_size = arena_size;
		out_arena->mem_chunks = (ArenaPageChunk*)arena_data;
		out_arena->data = PTR_BYTES_OFFSET(arena_data, data_byte_offset);

		init_mem_chunks(out_arena);
	}

	void arena_destroy(ArenaAllocator* arena)
	{
		free_memory((void*)arena, true, AllocationTag::RAW);
	}

	void* arena_allocate(ArenaAllocator* arena, uint64 size)
	{
		int32 allocation_index = -1;
		for (uint32 i = 0; i < arena->mem_chunk_count; i++)
		{
			if (!arena->mem_chunks[i].reserved && (arena->mem_chunks[i].page_count * arena->page_size) >= size)
			{
				allocation_index = (int32)i;
				break;
			}		
		}

		if (allocation_index >= 0)
		{
			uint32 pages_needed = (uint32)((size / arena->page_size) + 1);
			ArenaPageChunk* chunk = insert_reservation_at(arena, allocation_index, pages_needed);
			
			uint8* data_ptr = (uint8*)arena->data;
			data_ptr += (chunk->page_index * arena->page_size);

			// NOTE: Zeroing out allocated memory for now. Perhaps remove that later.
			zero_memory(data_ptr, chunk->page_count * arena->page_size);
			return data_ptr;
		}

		return 0;
	}

	void arena_free(ArenaAllocator* arena, void* data)
	{

		uint8* data_ptr = (uint8*)arena->data;

		int32 freeing_index = -1;
		for (uint32 i = 0; i < arena->mem_chunk_count; i++)
		{
			if (data_ptr == data)
			{
				freeing_index = (int32)i;
				break;
			}

			data_ptr += (arena->mem_chunks[i].page_count * arena->page_size);
			SHMASSERT_MSG(data_ptr <= data, "Error: Freeing data block does not align with chunks of mem arena!");
		}

		if (freeing_index >= 0)
			remove_reservation_at(arena, freeing_index);
		else
			SHMASSERT_MSG(data_ptr <= data, "Error: Freeing data block does not align with chunks of mem arena!");

	}

	void* arena_reallocate(ArenaAllocator* arena, uint64 requested_size, void* data)
	{

		uint8* data_ptr = (uint8*)arena->data;

		int32 chunk_index = -1;
		for (uint32 i = 0; i < arena->mem_chunk_count; i++)
		{
			if (data_ptr == data)
			{
				chunk_index = (int32)i;
				break;
			}

			data_ptr += (arena->mem_chunks[i].page_count * arena->page_size);
			SHMASSERT_MSG(data_ptr <= data, "Error: Freeing data block does not align with chunks of mem arena!");
		}

		if (chunk_index >= 0)
		{
			if (arena->mem_chunks[chunk_index].page_count * arena->page_size >= requested_size)
			{
				return data;
			}
			else
			{
				void* dest_mem = arena_allocate(arena, requested_size);
				copy_memory(data, dest_mem, arena->mem_chunks[chunk_index].page_count * arena->page_size);
				remove_reservation_at(arena, chunk_index);
				return dest_mem;
			}	
		}		
		else
		{
			SHMASSERT_MSG(data_ptr <= data, "Error: Freeing data block does not align with chunks of mem arena!");		
		}
		return nullptr;
		
	}

	static void init_mem_chunks(ArenaAllocator* arena)
	{
		arena->mem_chunks[0].page_index = 0;
		arena->mem_chunks[0].page_count = arena->page_count;
		arena->mem_chunks[0].reserved = false;
		arena->mem_chunk_count = 1;
	}

	static ArenaPageChunk* insert_reservation_at(ArenaAllocator* arena, uint32 index, uint32 reservation_page_count)
	{
		ArenaPageChunk* chunks = arena->mem_chunks;

		if (chunks[index].page_count == reservation_page_count)
		{
			chunks[index].reserved = true;
			return &chunks[index];
		}
		else
		{
			SHMASSERT(arena->mem_chunk_count < (arena->page_count - 1));

			copy_memory((void*)&chunks[index], (void*)(&chunks[index] + 1), sizeof(ArenaPageChunk) * (arena->mem_chunk_count - index));

			chunks[index].reserved = true;
			chunks[index].page_count = reservation_page_count;

			chunks[index + 1].reserved = false;
			chunks[index + 1].page_count -= reservation_page_count;
			chunks[index + 1].page_index += reservation_page_count;

			arena->mem_chunk_count++;

			return &chunks[index];
		}
	}

	static bool8 remove_reservation_at(ArenaAllocator* arena, uint32 index)
	{
		ArenaPageChunk* chunks = arena->mem_chunks;

		int32 merge_offset = 0;
		bool32 both_chunks_free = false;
		uint32 freed_page_count = chunks[index].page_count;
		uint32 freed_page_index = chunks[index].page_index;
		if (index > 0)
		{
			if (!chunks[index - 1].reserved)
				merge_offset = -1;
		}
		if (index < arena->mem_chunk_count - 1)
		{
			if (!chunks[index + 1].reserved)
			{
				if (merge_offset == 0)
					merge_offset = 1;
				else
					both_chunks_free = true;
			}

		}

		if (merge_offset != 0)
		{

			chunks[index + merge_offset].page_count += chunks[index].page_count;

			if (merge_offset == 1)
				chunks[index + merge_offset].page_index = freed_page_index;

			if (!both_chunks_free)
			{
				copy_memory((void*)(&chunks[index] + 1), (void*)&chunks[index], sizeof(ArenaPageChunk) * (arena->mem_chunk_count - index - 1));
				chunks[arena->mem_chunk_count - 1] = {};
				arena->mem_chunk_count--;
			}
			else
			{
				chunks[index + merge_offset].page_count += chunks[index + 1].page_count;
				copy_memory((void*)(&chunks[index] + 2), (void*)&chunks[index], sizeof(ArenaPageChunk) * (arena->mem_chunk_count - index - 2));
				chunks[arena->mem_chunk_count - 1] = {};
				chunks[arena->mem_chunk_count - 2] = {};
				arena->mem_chunk_count -= 2;
			}
			return true;
		}
		else
		{
			chunks[index].reserved = false;
			return true;
		}
	}

}