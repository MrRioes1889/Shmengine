#pragma once

#include "Defines.hpp"

#define MEMARENA_SMALL_PAGE_SIZE 64
#define MEMARENA_MEDIUM_PAGE_SIZE 128
#define MEMARENA_LARGE_PAGE_SIZE Kibibytes(1)

namespace Memory
{
	enum class MemArenaPageType
	{
		small_pages = 0x00,
		medium_pages = 0x01,
		large_pages = 0x02		
	};

	struct MemArenaPageChunk
	{
		uint32 page_index;
		uint32 page_count;
		bool8 reserved;
	};

	struct MemArena
	{
		MemArenaPageType page_type;
		uint32 page_size;
		uint32 page_count;
		uint64 mem_arena_size;
		uint32 mem_chunk_count;
		MemArenaPageChunk* mem_chunks;
		void* data;
	};

	MemArena* mem_arena_create(uint64 size, MemArenaPageType page_type);
	void mem_arena_destroy(MemArena* arena);

	void* mem_arena_allocate(MemArena* arena, uint64 size);
	void mem_arena_free(MemArena* arena, void* data);
	void* mem_arena_reallocate(MemArena* arena, uint64 requested_size, void* data);

}
