#include "MemArena.hpp"
#include "core/Memory.hpp"
#include "core/Assert.hpp"

namespace Memory
{
	static void init_mem_chunks(MemArena* arena)
	{
		arena->mem_chunks[0].page_index = 0;
		arena->mem_chunks[0].page_count = arena->page_count;
		arena->mem_chunks[0].reserved = false;
		arena->mem_chunk_count = 1;
	}

	static MemArenaPageChunk* insert_reservation_at(MemArena* arena, uint32 index, uint32 reservation_page_count)
	{
		MemArenaPageChunk* chunks = arena->mem_chunks;

		if (chunks[index].page_count == reservation_page_count)
		{
			chunks[index].reserved = true;
			return &chunks[index];
		}
		else
		{
			SHMASSERT(arena->mem_chunk_count < (arena->page_count - 1));

			copy_memory((void*)&chunks[index], (void*)(&chunks[index] + 1), sizeof(MemArenaPageChunk) * (arena->mem_chunk_count - index));

			chunks[index].reserved = true;
			chunks[index].page_count = reservation_page_count;

			chunks[index+1].reserved = false;
			chunks[index+1].page_count -= reservation_page_count;
			chunks[index+1].page_index += reservation_page_count;

			arena->mem_chunk_count++;

			return &chunks[index];
		}
	}

	static bool8 remove_reservation_at(MemArena* arena, uint32 index)
	{
		MemArenaPageChunk* chunks = arena->mem_chunks;

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
				copy_memory((void*)(&chunks[index] + 1), (void*)&chunks[index], sizeof(MemArenaPageChunk) * (arena->mem_chunk_count - index - 1));
				chunks[arena->mem_chunk_count - 1] = {};
				arena->mem_chunk_count--;
			}
			else
			{
				chunks[index + merge_offset].page_count += chunks[index + 1].page_count;
				copy_memory((void*)(&chunks[index] + 2), (void*)&chunks[index], sizeof(MemArenaPageChunk) * (arena->mem_chunk_count - index - 2));
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

	MemArena* mem_arena_create(uint64 size, MemArenaPageType page_type)
	{

		uint32 page_size = 0;
		uint32 page_count = 0;

		switch (page_type)
		{
		case MemArenaPageType::small_pages:
		{
			page_size = MEMARENA_SMALL_PAGE_SIZE;
			break;
		}
		case MemArenaPageType::medium_pages:
		{
			page_size = MEMARENA_MEDIUM_PAGE_SIZE;
			break;
		}
		case MemArenaPageType::large_pages:
		{
			page_size = MEMARENA_LARGE_PAGE_SIZE;				
			break;
		}
		}

		page_count = (uint32)((size / page_size) + 1);

		uint64 mem_arena_size =
			(sizeof(MemArena)) +
			(page_count * sizeof(MemArenaPageChunk)) +
			(page_count * page_size);
		MemArena* arena = (MemArena*)raw_allocate(mem_arena_size, true);
		zero_memory((void*)arena, mem_arena_size);

		uint64 mem_chunks_byte_offset = (sizeof(MemArena));
		uint64 data_byte_offset = mem_chunks_byte_offset + (page_count * sizeof(MemArenaPageChunk));

		arena->page_size = page_size;
		arena->page_count = page_count;
		arena->page_type = page_type;
		arena->mem_arena_size = mem_arena_size;
		arena->mem_chunks = (MemArenaPageChunk*)((uint8*)(arena)+mem_chunks_byte_offset);
		arena->data = (void*)((uint8*)(arena)+data_byte_offset);

		init_mem_chunks(arena);

		return arena;
	}

	void mem_arena_destroy(MemArena* arena)
	{
		raw_free((void*)arena, true);
	}

	void* mem_arena_allocate(MemArena* arena, uint64 size)
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
			MemArenaPageChunk* chunk = insert_reservation_at(arena, allocation_index, pages_needed);
			
			uint8* data_ptr = (uint8*)arena->data;
			data_ptr += (chunk->page_index * arena->page_size);

			// NOTE: Zeroing out allocated memory for now. Perhaps remove that later.
			zero_memory(data_ptr, chunk->page_count * arena->page_size);
			return data_ptr;
		}

		return 0;
	}

	void mem_arena_free(MemArena* arena, void* data)
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

	void* mem_arena_reallocate(MemArena* arena, uint64 requested_size, void* data)
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
				void* dest_mem = mem_arena_allocate(arena, requested_size);
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
}