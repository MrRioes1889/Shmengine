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
        uint64 external_allocation_size;
        uint32 allocation_count;
        uint32 external_allocation_count;

        Buffer main_memory;
        DynamicAllocator main_allocator;

        Buffer string_memory;
        DynamicAllocator string_allocator;

        Threading::Mutex allocation_mutex;
        
    };
    
    static bool32 system_initialized = false;
    static SystemState* system_state;

    static void* platform_allocate(uint64 size, uint16 alignment);
    static void* platform_reallocate(uint64 size, void* block, uint16 alignment);
    static void platform_free(void* block, bool32 aligned);

    static void* _allocate(DynamicAllocator* allocator, uint64 size, AllocationTag tag, uint16 alignment = 1);
    static void* _reallocate(DynamicAllocator* allocator, uint64 size, void* block, uint16 alignment = 1);
    static void _free_memory(DynamicAllocator* allocator, void* block, bool32 aligned = true);

    static void init_buffer_and_allocator_pair(Buffer* buffer, DynamicAllocator* out_allocator, DynamicAllocator* target_allocator, uint64 size, AllocatorPageSize page_size, AllocationTag tag, uint32 node_count_limit = 0, uint16 alignment = 1)
    {
        uint64 nodes_size = 0;
        if (node_count_limit)
            nodes_size = Freelist::get_required_nodes_array_memory_size_by_node_count(node_count_limit);
        else
            nodes_size = Freelist::get_max_node_count_by_data_size(size, page_size) * sizeof(Freelist::Node);

        BufferFlags::Value flags = 0;
        if (!target_allocator)
            flags = BufferFlags::PlatformAllocation;

        void* data = _allocate(target_allocator, size + nodes_size, tag, alignment);
        buffer->init(size + nodes_size, flags, tag, data);
        void* main_nodes = (((uint8*)buffer->data) + size);
        out_allocator->init(size, buffer->data, nodes_size, main_nodes, page_size, node_count_limit);
    }

    bool32 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config)
    {

        system_state = (SystemState*)platform_allocate(sizeof(SystemState), true);
        zero_memory(system_state, sizeof(SystemState));
        system_state->config = *((SystemConfig*)config);

        if (!Threading::mutex_create(&system_state->allocation_mutex))
        {
            SHMFATAL("Failed creating general allocation mutex!");
            return false;
        }

        init_buffer_and_allocator_pair(&system_state->main_memory, &system_state->main_allocator, 0, system_state->config.total_allocation_size, AllocatorPageSize::TINY, AllocationTag::MainMemory, 10000, 64);
        init_buffer_and_allocator_pair(&system_state->string_memory, &system_state->string_allocator, &system_state->main_allocator, mebibytes(64), AllocatorPageSize::SMALL, AllocationTag::Allocators, 100000, 64);

        system_state->allocated_size = 0;
        system_state->allocation_count = 0;

        system_state->external_allocation_size = 0;
        system_state->external_allocation_count = 0;       

        system_initialized = true;
        return system_initialized;

    }

    void system_shutdown(void* state)
    {
        system_state->string_memory.free_data();
        system_state->main_memory.free_data();
        Threading::mutex_destroy(&system_state->allocation_mutex); 
        system_state = 0;
    }

    void* allocate(uint64 size, AllocationTag tag, uint16 alignment)
    {
        return _allocate(&system_state->main_allocator, size, tag, alignment);
    }
    void* reallocate(uint64 size, void* block, uint16 alignment)
    {
        return _reallocate(&system_state->main_allocator, size, block, alignment);
    }
    void free_memory(void* block)
    {
        _free_memory(&system_state->main_allocator, block, true);
    }

    void* allocate_string(uint64 size, AllocationTag tag, uint16 alignment)
    {
        return _allocate(&system_state->string_allocator, size, tag, alignment);
    }
    void* reallocate_string(uint64 size, void* block, uint16 alignment)
    {
        return _reallocate(&system_state->string_allocator, size, block, alignment);
    }
    void free_memory_string(void* block)
    {
        _free_memory(&system_state->string_allocator, block, true);
    }

    void* allocate_platform(uint64 size, AllocationTag tag, uint16 alignment)
    {
        return _allocate(0, size, tag, alignment);
    }
    void* reallocate_platform(uint64 size, void* block, uint16 alignment)
    {
        return _reallocate(0, size, block, alignment);
    }
    void free_memory_platform(void* block, bool32 aligned)
    {
        _free_memory(0, block, aligned);
    }

    void track_external_allocation(uint64 size, AllocationTag tag)
    {
        Threading::mutex_lock(&system_state->allocation_mutex);
        system_state->external_allocation_size += size;
        //system_state->stats.tagged_allocations[tag] += size;
        system_state->external_allocation_count++;
        Threading::mutex_unlock(&system_state->allocation_mutex);
    }

    void track_external_free(uint64 size, AllocationTag tag)
    {
        Threading::mutex_lock(&system_state->allocation_mutex);
        system_state->external_allocation_size -= size;
        //system_state->stats.tagged_allocations[tag] -= size;
        system_state->external_allocation_count--;
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

    uint32 get_current_allocation_count()
    {
        return system_state->allocation_count;
    }

    static void* _allocate(DynamicAllocator* allocator, uint64 size, AllocationTag tag, uint16 alignment)
    {
        if (size == 0) //|| !Math::is_power_of_2(alignment))
            return 0;

        void* ret = 0;

        if (!system_state && allocator)
            return 0;   
    
        /*uint64 bytes_allocated = 0;
        ret = system_state->string_allocator.allocate(size, &bytes_allocated);
        uint64 offset = (uint64)ret - (uint64)system_state->string_allocator.data;
        SHMDEBUGV("string allocated at offset: %lu; bytes allocated: %lu", offset, bytes_allocated);*/
        if (!allocator)
        {
            ret = platform_allocate(size, alignment);
            tag = AllocationTag::Platform;
        }         
        else
        {
            if (!Threading::mutex_lock(&system_state->allocation_mutex))
            {
                SHMFATAL("Failed obtaining lock for general allocation mutex!");
                return 0;
            }

            ret = allocator->allocate(size, tag, alignment);

            system_state->allocation_count++;

            Threading::mutex_unlock(&system_state->allocation_mutex);
        }     

        zero_memory(ret, size);
        return ret;

    }

    static void* _reallocate(DynamicAllocator* allocator, uint64 size, void* block, uint16 alignment)
    {

        if (!system_state && allocator)
            return 0;

        void* ret = 0;    

        /*uint64 bytes_freed = 0;
        uint64 bytes_allocated = 0;
        ret = system_state->string_allocator.reallocate(size, block, &bytes_freed, &bytes_allocated);
        uint64 from_offset = (uint64)block - (uint64)system_state->string_allocator.data;
        uint64 to_offset = (uint64)ret - (uint64)system_state->string_allocator.data;
        SHMDEBUGV("String reallocated from: %lu, to: %lu; Bytes freed: %lu; Bytes allocated; %lu", from_offset, to_offset, bytes_freed, bytes_allocated);*/
        AllocationTag tag;
        if (!system_state || !allocator)
        {
            ret = platform_reallocate(size, block, alignment);
            tag = AllocationTag::Platform;
        }        
        else
        {
            if (!Threading::mutex_lock(&system_state->allocation_mutex))
            {
                SHMFATAL("Failed obtaining lock for general allocation mutex!");
                return 0;
            }

            ret = allocator->reallocate(size, block, &tag, alignment);

            Threading::mutex_unlock(&system_state->allocation_mutex);
        }
            
        return ret;
   
    }

    static void _free_memory(DynamicAllocator* allocator, void* block, bool32 aligned)
    {       

        if (!system_state && system_initialized && allocator)
            return;

        /*uint64 bytes_freed = 0;
        system_state->string_allocator.free(block, &bytes_freed);
        uint64 offset = (uint64)block - (uint64)system_state->string_allocator.data;
        SHMDEBUGV("String freed at offset: %lu; Bytes freed: %lu", offset, bytes_freed);*/
        AllocationTag tag;
        if (!system_state || !allocator)
        {
            platform_free(block, aligned);
            tag = AllocationTag::Platform;
        }          
        else
        {
            if (!Threading::mutex_lock(&system_state->allocation_mutex))
            {
                SHMFATAL("Failed obtaining lock for general allocation mutex!");
                return;
            }

            allocator->free(block, &tag);

            system_state->allocation_count--;

            Threading::mutex_unlock(&system_state->allocation_mutex);
        }   
        
    }

    static void* platform_allocate(uint64 size, uint16 alignment)
    {
        return Platform::allocate(size, alignment);
    }

    static void* platform_reallocate(uint64 size, void* block, uint16 alignment)
    {
        void* new_block = Platform::allocate(size, alignment);
        zero_memory(new_block, size);
        copy_memory(block, new_block, size);
        Platform::free_memory(block, alignment > 1);
        return new_block;
    }

    static void platform_free(void* block, bool32 aligned)
    {
        Platform::free_memory(block, aligned);
    }

}