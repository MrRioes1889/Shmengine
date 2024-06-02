#include "Memory.hpp"
#include "platform/Platform.hpp"
#include "memory/ArenaAllocator.hpp"
#include "memory/LinearAllocator.hpp"

namespace Memory
{

    struct SystemState
    {
        ArenaAllocator main_memory;
        ArenaAllocator transient_memory;
        ArenaAllocator temp_memory;

        bool32 initialized = false;
    };
    
    static SystemState* system_state;

    bool32 init_memory(void* linear_allocator, void*& out_state)
    {
        Memory::LinearAllocator* allocator = (Memory::LinearAllocator*)linear_allocator;
        out_state = Memory::linear_allocator_allocate(allocator, sizeof(SystemState));
        system_state = (SystemState*)out_state;
        *system_state = {};

        arena_create(Mebibytes(256), ArenaPageType::medium_pages, &system_state->main_memory);
        arena_create(Mebibytes(256), ArenaPageType::medium_pages, &system_state->transient_memory);
        arena_create(Mebibytes(256), ArenaPageType::medium_pages, &system_state->temp_memory);

        system_state->initialized = true;
        return system_state->initialized;
    }

    static void* raw_allocate(uint64 size, bool32 aligned);
    static void* raw_reallocate(uint64 size, void* block, bool32 aligned);
    static void raw_free(void* block, bool32 aligned);

    void* allocate(uint64 size, bool32 aligned, AllocationTag tag)
    {
        if (size == 0)
            return 0;

        if (!system_state || !system_state->initialized)
            return raw_allocate(size, aligned);

        switch (tag)
        {
        case AllocationTag::RAW:
            return raw_allocate(size, aligned);
        case AllocationTag::TRANSIENT:
            return arena_allocate(&system_state->transient_memory, size);
        case AllocationTag::TBD:
            return arena_allocate(&system_state->temp_memory, size);
        default:
            return arena_allocate(&system_state->main_memory, size);
        }
        
    }

    void* reallocate(uint64 size, void* block, bool32 aligned, AllocationTag tag)
    {
        if (!system_state || !system_state->initialized)
            return raw_reallocate(size, block, aligned);  

        switch (tag)
        {
        case AllocationTag::RAW:
            return raw_reallocate(size, block, aligned);
        case AllocationTag::TRANSIENT:
            return arena_reallocate(&system_state->transient_memory, size, block);
        default:
            return arena_reallocate(&system_state->main_memory, size, block);
        }
   
    }

    void free_memory(void* block, bool32 aligned, AllocationTag tag)
    {
        if (!system_state || !system_state->initialized)
            return raw_free(block, aligned);

        switch (tag)
        {
        case AllocationTag::RAW:
            return raw_free(block, aligned);
        case AllocationTag::TRANSIENT:
            return arena_free(&system_state->transient_memory, block);
        default:
            return arena_free(&system_state->main_memory, block);
        }
        
    }

    void* zero_memory(void* block, uint64 size)
    {
        return Platform::zero_memory(block, size);
    }

    void* copy_memory(const void* source, void* dest, uint64 size)
    {
        return Platform::copy_memory(source, dest, size);
    }

    void* set_memory(void* dest, int32 value, uint64 size)
    {
        return Platform::set_memory(dest, value, size);
    }

    static void* raw_allocate(uint64 size, bool32 aligned)
    {
        return Platform::allocate(size, aligned);
    }

    static void* raw_reallocate(uint64 size, void* block, bool32 aligned)
    {
        void* new_block = Platform::allocate(size, aligned);
        copy_memory(block, new_block, size);
        free_memory(block, aligned, AllocationTag::RAW);
        return new_block;
    }

    static void raw_free(void* block, bool32 aligned)
    {
        Platform::free_memory(block, aligned);
    }

}