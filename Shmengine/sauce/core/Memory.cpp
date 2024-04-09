#include "Memory.hpp"
#include "platform/Platform.hpp"
#include "containers/MemArena.hpp"

namespace Memory
{

    static MemArena* main_memory;

    bool8 init_memory()
    {
        main_memory = mem_arena_create(Gibibytes(1), MemArenaPageType::medium_pages);
        return true;
    }

    void* raw_allocate(uint64 size, bool32 aligned)
    {
        return Platform::allocate(size, aligned);
    }

    void raw_free(void* block, bool32 aligned)
    {
        Platform::free_memory(block, aligned);
    }

    void* allocate(uint64 size, bool32 aligned)
    {
        return mem_arena_allocate(main_memory, size);
    }

    void* reallocate(uint64 size, void* block, bool32 aligned)
    {
        return mem_arena_reallocate(main_memory, size, block);
    }

    void free_memory(void* block, bool32 aligned)
    {
        return mem_arena_free(main_memory, block);
    }

    void* zero_memory(void* block, uint64 size)
    {
        return Platform::zero_memory(block, size);
    }

    void* copy_memory(void* dest, const void* source, uint64 size)
    {
        return Platform::copy_memory(dest, source, size);
    }

    void* set_memory(void* dest, int32 value, uint64 size)
    {
        return Platform::set_memory(dest, value, size);
    }

}