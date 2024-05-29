#include "TestManager.hpp"

#include <containers/Darray.hpp>
#include <core/logging.hpp>
#include <utility/string/String.hpp>
#include <core/Clock.hpp>

typedef struct test_entry {
    PFN_test func;
    const char* desc;
} test_entry;

static test_entry* tests;

void test_manager_init() {
    tests = darray_create(test_entry);
}

void test_manager_register_test(uint8(*PFN_test)(), const char* desc) {
    test_entry e;
    e.func = PFN_test;
    e.desc = desc;
    darray_push(tests, e);
}

void test_manager_run_tests() {
    uint32 passed = 0;
    uint32 failed = 0;
    uint32 skipped = 0;

    uint32 count = darray_count(tests);

    Clock total_time;
    clock_start(&total_time);

    for (uint32 i = 0; i < count; ++i) {
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
        String::print_s(status, 20, failed ? (char*)"*** %u FAILED ***" : (char*)"SUCCESS", failed);
        clock_update(&total_time);
        SHMINFOV("Executed %u of %u (skipped %u) %s (%f6 sec / %f6 sec total", i + 1, count, skipped, status, test_time.elapsed, total_time.elapsed);
    }

    clock_stop(&total_time);

    SHMINFOV("Results: %u passed, %u failed, %u skipped.", passed, failed, skipped);
}