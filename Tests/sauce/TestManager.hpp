#pragma once

#include <Defines.hpp>

#define BYPASS 2

typedef uint8(*PFN_test)();

void test_manager_init();

void test_manager_register_test(PFN_test, const char* desc);

void test_manager_run_tests();
