#include "TestManager.hpp"

#include <containers/Darray.hpp>
#include <core/logging.hpp>
#include <utility/String.hpp>
#include <core/Clock.hpp>

typedef struct TestEntry {
    PFN_test func;
    const char* desc;
} test_entry;

static Darray<TestEntry> tests = {};

void test_manager_init() {
    tests.init(1, 0, AllocationTag::PLAT);
}

void test_manager_register_test(uint8(*PFN_test)(), const char* desc) {
    TestEntry e;
    e.func = PFN_test;
    e.desc = desc;
    tests.push(e);
}

void test_manager_run_tests() {
    uint32 passed = 0;
    uint32 failed = 0;
    uint32 skipped = 0;

    Clock total_time;
    clock_start(&total_time);

    for (uint32 i = 0; i < tests.count; ++i) {
        Clock test_time;
        clock_start(&test_time);
        uint8 result = tests[i].func();
        clock_update(&test_time);

        if (result == 1) {
            ++passed;
        }
        else if (result == BYPASS) {
            SHMWARNV("[SKIPPED]: %s", tests[i].desc);
            ++skipped;
        }
        else {
            SHMERRORV("[FAILED]: %s", tests[i].desc);
            ++failed;
        }
        char status[20];
        CString::print_s(status, 20, failed ? (char*)"*** %u FAILED ***" : (char*)"SUCCESS", failed);
        clock_update(&total_time);
        SHMINFOV("Executed %u of %u (skipped %u) %s (%f6 sec / %f6 sec total", i + 1, tests.count, skipped, status, test_time.elapsed, total_time.elapsed);
    }

    clock_stop(&total_time);

    SHMINFOV("Results: %u passed, %u failed, %u skipped.", passed, failed, skipped);
}