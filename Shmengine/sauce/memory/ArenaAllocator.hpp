#pragma once

#include "Defines.hpp"

#define MEMARENA_SMALL_PAGE_SIZE 64
#define MEMARENA_MEDIUM_PAGE_SIZE 128
#define MEMARENA_LARGE_PAGE_SIZE Kibibytes(1)

namespace Memory
{
	enum class ArenaPageType
	{
		small_pages = 0x00,
		medium_pages = 0x01,
		large_pages = 0x02		
	};

	struct ArenaPageChunk
	{
		uint32 page_index;
		uint32 page_count;
		bool8 reserved;
	};

	struct ArenaAllocator
	{
		ArenaPageType page_type;
		uint32 page_size;
		uint32 page_count;
		uint64 arena_size;
		uint32 mem_chunk_count;
		ArenaPageChunk* mem_chunks;
		void* data;
	};

	//ArenaAllocator* arena_create(uint64 size, ArenaPageType page_type);
	void arena_create(uint64 size, ArenaPageType page_type, ArenaAllocator* out_arena);
	void arena_destroy(ArenaAllocator* arena);

	void* arena_allocate(ArenaAllocator* arena, uint64 size);
	void arena_free(ArenaAllocator* arena, void* data);
	void* arena_reallocate(ArenaAllocator* arena, uint64 requested_size, void* data);

}
