#include "Memory.hpp"
#include "platform/Platform.hpp"
#include "containers/Buffer.hpp"
#include "memory/DynamicAllocator.hpp"
#include "memory/LinearAllocator.hpp"

namespace Memory
{

    struct SystemState
    {
        //ArenaAllocator main_memory;
        /*ArenaAllocator transient_memory;
        ArenaAllocator temp_memory;*/

        Buffer main_memory;
        DynamicAllocator main_allocator;

        Buffer transient_memory;
        DynamicAllocator transient_allocator;
        
    };
    
    static bool32 system_initialized = false;
    static SystemState* system_state;

    static void init_buffer_and_allocator_pair(Buffer* buffer, DynamicAllocator* allocator, uint64 size, AllocatorPageSize page_size, AllocationTag tag, uint32 node_count_limit = 0)
    {
        uint64 main_nodes_size = 0;
        if (node_count_limit)
            main_nodes_size = DynamicAllocator::get_required_nodes_array_memory_size_by_node_count(node_count_limit, page_size);
        else
            main_nodes_size = DynamicAllocator::get_required_nodes_array_memory_size_by_data_size(size, page_size);

        buffer->init(size + main_nodes_size, tag);
        void* main_nodes = (((uint8*)buffer->data) + size);
        allocator->init(size, buffer->data, page_size, main_nodes, node_count_limit);
    }

    bool32 system_init(PFN_allocator_allocate_callback allocator_callback, void*& out_state)
    {

        out_state = allocator_callback(sizeof(SystemState));
        system_state = (SystemState*)out_state;

        init_buffer_and_allocator_pair(&system_state->main_memory, &system_state->main_allocator, Mebibytes(256), AllocatorPageSize::SMALL, AllocationTag::RAW, 10000);
        init_buffer_and_allocator_pair(&system_state->transient_memory, &system_state->transient_allocator, Mebibytes(256), AllocatorPageSize::SMALL, AllocationTag::RAW, 10000);

        system_initialized = true;
        return system_initialized;

    }

    void system_shutdown()
    {
        system_state->main_memory.free_data();
        system_state->transient_memory.free_data();
        system_state = 0;
    }

    static void* raw_allocate(uint64 size, bool32 aligned);
    static void* raw_reallocate(uint64 size, void* block, bool32 aligned);
    static void raw_free(void* block, bool32 aligned);

    void* allocate(uint64 size, bool32 aligned, AllocationTag tag)
    {
        if (size == 0)
            return 0;

        if (!system_state)
        {
            if (system_initialized)
                return 0;
            else
                return raw_allocate(size, aligned);
        }

        switch (tag)
        {
        case AllocationTag::RAW:
            return raw_allocate(size, aligned);
        case AllocationTag::TRANSIENT:
            return system_state->transient_allocator.allocate(size);
        default:
            return system_state->main_allocator.allocate(size);
        }
        
    }

    void* reallocate(uint64 size, void* block, bool32 aligned, AllocationTag tag)
    {
        if (!system_state)
        {
            if (system_initialized)
                return 0;
            else
                return raw_reallocate(size, block, aligned);
        }

        switch (tag)
        {
        case AllocationTag::RAW:
            return raw_reallocate(size, block, aligned);
        case AllocationTag::TRANSIENT:
            return system_state->transient_allocator.reallocate(size, block);
        default:
            return system_state->main_allocator.reallocate(size, block);
        }
   
    }

    void free_memory(void* block, bool32 aligned, AllocationTag tag)
    {
        if (!system_state)
        {
            if (system_initialized)
                return;
            else
                return raw_free(block, aligned);
        }

        switch (tag)
        {
        case AllocationTag::RAW:
            return raw_free(block, aligned);
        case AllocationTag::TRANSIENT:
            return system_state->transient_allocator.free(block);
        default:
            return system_state->main_allocator.free(block);
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