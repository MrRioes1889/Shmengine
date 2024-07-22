#include "Memory.hpp"
#include "platform/Platform.hpp"
#include "containers/Buffer.hpp"
#include "memory/DynamicAllocator.hpp"
#include "memory/LinearAllocator.hpp"
#include "core/Logging.hpp"
#include "core/Mutex.hpp"

namespace Memory
{

    struct SystemState
    {

        SystemConfig config;

        uint64 allocated_size;

        Buffer main_memory;
        DynamicAllocator main_allocator;

        Buffer transient_memory;
        DynamicAllocator transient_allocator;

        Buffer string_memory;
        DynamicAllocator string_allocator;

        Threading::Mutex allocation_mutex;
        
    };
    
    static bool32 system_initialized = false;
    static SystemState* system_state;

    static void* platform_allocate(uint64 size, bool32 aligned);
    static void* platform_reallocate(uint64 size, void* block, bool32 aligned);
    static void platform_free(void* block, bool32 aligned);

    static void init_buffer_and_allocator_pair(Buffer* buffer, DynamicAllocator* allocator, uint64 size, AllocatorPageSize page_size, AllocationTag tag, uint32 node_count_limit = 0)
    {
        uint64 nodes_size = 0;
        if (node_count_limit)
            nodes_size = Freelist::get_required_nodes_array_memory_size_by_node_count(node_count_limit, page_size);
        else
            nodes_size = Freelist::get_required_nodes_array_memory_size_by_data_size(size, page_size);

        buffer->init(size + nodes_size, 0, tag);
        void* main_nodes = (((uint8*)buffer->data) + size);
        allocator->init(size, buffer->data, nodes_size, main_nodes, page_size, node_count_limit);
    }

    bool32 system_init(SystemConfig config)
    {

        system_state = (SystemState*)platform_allocate(sizeof(SystemState), true);
        zero_memory(system_state, sizeof(SystemState));
        system_state->config = config;

        init_buffer_and_allocator_pair(&system_state->main_memory, &system_state->main_allocator, config.total_allocation_size, AllocatorPageSize::SMALL, AllocationTag::PLAT, 10000);

        init_buffer_and_allocator_pair(&system_state->transient_memory, &system_state->transient_allocator, config.total_allocation_size / 16, AllocatorPageSize::SMALL, AllocationTag::MAIN, 10000);
        init_buffer_and_allocator_pair(&system_state->string_memory, &system_state->string_allocator, Mebibytes(64), AllocatorPageSize::MINIMAL, AllocationTag::MAIN, 100000);

        system_state->allocated_size = 0;

        if (!Threading::mutex_create(&system_state->allocation_mutex))
        {
            SHMFATAL("Failed creating general allocation mutex!");
            return false;
        }

        system_initialized = true;
        return system_initialized;

    }

    void system_shutdown()
    {
        system_state->transient_memory.free_data();
        system_state->main_memory.free_data();
        Threading::mutex_destroy(&system_state->allocation_mutex); 
        system_state = 0;
    }

    void* allocate(uint64 size, bool32 aligned, AllocationTag tag)
    {
        if (size == 0)
            return 0;

        void* ret = 0;

        if (!system_state)
        {
            if (system_initialized)
                return 0;
            else
                return platform_allocate(size, aligned);
        }

        if (!Threading::mutex_lock(&system_state->allocation_mutex))
        {
            SHMFATAL("Failed obtaining lock for general allocation mutex!");
            return 0;
        }

        switch (tag)
        {
        case AllocationTag::PLAT:
        {
            ret = platform_allocate(size, aligned);
            break;
        }
        case AllocationTag::TRANSIENT:
        {
            ret = system_state->transient_allocator.allocate(size);
            break;
        }
        case AllocationTag::STRING:
        {
            /*uint64 bytes_allocated = 0;
            ret = system_state->string_allocator.allocate(size, &bytes_allocated);
            uint64 offset = (uint64)ret - (uint64)system_state->string_allocator.data;
            SHMDEBUGV("string allocated at offset: %lu; bytes allocated: %lu", offset, bytes_allocated);*/
            ret = system_state->string_allocator.allocate(size);
            break;
        }
        default:
        {
            ret = system_state->main_allocator.allocate(size);
            break;
        }
        }

        Threading::mutex_unlock(&system_state->allocation_mutex);       

        zero_memory(ret, size);
        return ret;

    }

    void* reallocate(uint64 size, void* block, bool32 aligned, AllocationTag tag)
    {
        if (!system_state)
        {
            if (system_initialized)
                return 0;
            else
                return platform_reallocate(size, block, aligned);
        }

        void* ret = 0;

        if (!Threading::mutex_lock(&system_state->allocation_mutex))
        {
            SHMFATAL("Failed obtaining lock for general allocation mutex!");
            return 0;
        }

        switch (tag)
        {
        case AllocationTag::PLAT:
        {
            ret = platform_reallocate(size, block, aligned);
            break;
        }           
        case AllocationTag::TRANSIENT:
        {
            ret = system_state->transient_allocator.reallocate(size, block);
            break;
        }         
        case AllocationTag::STRING:
        {
            /*uint64 bytes_freed = 0;
            uint64 bytes_allocated = 0;
            ret = system_state->string_allocator.reallocate(size, block, &bytes_freed, &bytes_allocated);
            uint64 from_offset = (uint64)block - (uint64)system_state->string_allocator.data;
            uint64 to_offset = (uint64)ret - (uint64)system_state->string_allocator.data;
            SHMDEBUGV("String reallocated from: %lu, to: %lu; Bytes freed: %lu; Bytes allocated; %lu", from_offset, to_offset, bytes_freed, bytes_allocated);*/
            ret = system_state->string_allocator.reallocate(size, block);
            break;
        }         
        default:
        {
            ret = system_state->main_allocator.reallocate(size, block);
            break;
        }          
        }

        Threading::mutex_unlock(&system_state->allocation_mutex);

        return ret;
   
    }

    void free_memory(void* block, bool32 aligned, AllocationTag tag)
    {       

        if (!system_state)
        {
            if (system_initialized)
                return;
            else
                return platform_free(block, aligned);
        }

        if (!Threading::mutex_lock(&system_state->allocation_mutex))
        {
            SHMFATAL("Failed obtaining lock for general allocation mutex!");
            return;
        }

        switch (tag)
        {
        case AllocationTag::PLAT:
        {
            platform_free(block, aligned);
            break;
        }         
        case AllocationTag::TRANSIENT:
        {
            system_state->transient_allocator.free(block);
            break;
        }        
        case AllocationTag::STRING:
        {
            /*uint64 bytes_freed = 0;
            system_state->string_allocator.free(block, &bytes_freed);
            uint64 offset = (uint64)block - (uint64)system_state->string_allocator.data;
            SHMDEBUGV("String freed at offset: %lu; Bytes freed: %lu", offset, bytes_freed);*/
            system_state->string_allocator.free(block);
            break;
        }          
        default:
        {
            system_state->main_allocator.free(block);
            break;
        }          
        }

        Threading::mutex_unlock(&system_state->allocation_mutex);
        
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

    static void* platform_allocate(uint64 size, bool32 aligned)
    {
        return Platform::allocate(size, aligned);
    }

    static void* platform_reallocate(uint64 size, void* block, bool32 aligned)
    {
        void* new_block = Platform::allocate(size, aligned);
        copy_memory(block, new_block, size);
        free_memory(block, aligned, AllocationTag::PLAT);
        return new_block;
    }

    static void platform_free(void* block, bool32 aligned)
    {
        Platform::free_memory(block, aligned);
    }

}