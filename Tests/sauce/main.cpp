#include "TestManager.hpp"

#include "LinearAllocatorTests.hpp"

#include <memory/Memory.hpp>
#include <core/Logging.hpp>

int main() {
    // Always initalize the test manager first.
    test_manager_init();

    // TODO: add test registrations here.
    Memory::linear_allocator_register_tests();

    SHMDEBUG("Starting tests...");

    // Execute tests
    test_manager_run_tests();

    return 0;
}