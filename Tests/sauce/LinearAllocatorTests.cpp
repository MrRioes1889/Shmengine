#include "LinearAllocatorTests.hpp"
#include "TestManager.hpp"
#include "Expect.hpp"

#include <Defines.hpp>

#include <memory/LinearAllocator.hpp>

namespace Memory
{

    uint8 linear_allocator_should_create_and_destroy() {
        LinearAllocator alloc;
        linear_allocator_create(sizeof(uint64), &alloc, 0);

        expect_should_not_be(0, alloc.memory);
        expect_should_be(sizeof(uint64), alloc.size);
        expect_should_be(0, alloc.allocated);

        linear_allocator_destroy(&alloc);

        expect_should_be(0, alloc.memory);
        expect_should_be(0, alloc.size);
        expect_should_be(0, alloc.allocated);

        return true;
    }

    uint8 linear_allocator_single_allocation_all_space() {
        LinearAllocator alloc;
        linear_allocator_create(sizeof(uint64), &alloc, 0);

        // Single allocation.
        void* block = linear_allocator_allocate(&alloc, sizeof(uint64));

        // Validate it
        expect_should_not_be(0, block);
        expect_should_be(sizeof(uint64), alloc.allocated);

        linear_allocator_destroy(&alloc);

        return true;
    }

    uint8 linear_allocator_multi_allocation_all_space() {
        uint64 max_allocs = 1024;
        LinearAllocator alloc;
        linear_allocator_create(sizeof(uint64) * max_allocs, &alloc, 0);

        // Multiple allocations - full.
        void* block;
        for (uint64 i = 0; i < max_allocs; ++i) {
            block = linear_allocator_allocate(&alloc, sizeof(uint64));
            // Validate it
            expect_should_not_be(0, block);
            expect_should_be(sizeof(uint64) * (i + 1), alloc.allocated);
        }

        linear_allocator_destroy(&alloc);

        return true;
    }

    uint8 linear_allocator_multi_allocation_over_allocate() {
        uint64 max_allocs = 3;
        LinearAllocator alloc;
        linear_allocator_create(sizeof(uint64) * max_allocs, &alloc, 0);

        // Multiple allocations - full.
        void* block;
        for (uint64 i = 0; i < max_allocs; ++i) {
            block = linear_allocator_allocate(&alloc, sizeof(uint64));
            // Validate it
            expect_should_not_be(0, block);
            expect_should_be(sizeof(uint64) * (i + 1), alloc.allocated);
        }

        SHMDEBUG("Note: The following error is intentionally caused by this test.");

        // Ask for one more allocation. Should error and return 0.
        block = linear_allocator_allocate(&alloc, sizeof(uint64));
        // Validate it - allocated should be unchanged.
        expect_should_be(0, block);
        expect_should_be(sizeof(uint64) * (max_allocs), alloc.allocated);

        linear_allocator_destroy(&alloc);

        return true;
    }

    //uint8 linear_allocator_multi_allocation_all_space_then_free() {
    //    uint64 max_allocs = 1024;
    //    LinearAllocator alloc;
    //    linear_allocator_create(sizeof(uint64) * max_allocs, &alloc, 0);

    //    // Multiple allocations - full.
    //    void* block;
    //    for (uint64 i = 0; i < max_allocs; ++i) {
    //        block = linear_allocator_allocate(&alloc, sizeof(uint64));
    //        // Validate it
    //        expect_should_not_be(0, block);
    //        expect_should_be(sizeof(uint64) * (i + 1), alloc.allocated);
    //    }

    //    // Validate that pointer is reset.
    //    linear_allocator_free_all(&alloc);
    //    expect_should_be(0, alloc.allocated);

    //    linear_allocator_destroy(&alloc);

    //    return true;
    //}

    void linear_allocator_register_tests() {
        test_manager_register_test(linear_allocator_should_create_and_destroy, "Linear allocator should create and destroy");
        test_manager_register_test(linear_allocator_single_allocation_all_space, "Linear allocator single alloc for all space");
        test_manager_register_test(linear_allocator_multi_allocation_all_space, "Linear allocator multi alloc for all space");
        test_manager_register_test(linear_allocator_multi_allocation_over_allocate, "Linear allocator try over allocate");
        //test_manager_register_test(linear_allocator_multi_allocation_all_space_then_free, "Linear allocator allocated should be 0 after free_all");
    }

}