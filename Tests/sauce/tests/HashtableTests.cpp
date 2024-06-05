#include "HashtableTests.hpp"
#include "../TestManager.hpp"
#include "../Expect.hpp"

#include <Defines.hpp>
#include <containers/Hashtable.hpp>


uint8 hashtable_should_set_and_get_successfully() 
{
    Hashtable<uint64> table;

    uint32 element_count = 3;

    table.init(element_count, AllocationTag::RAW);

    expect_should_not_be(0, table.buffer.data);
    expect_should_be(3, table.element_count);

    uint64 testval1 = 23;
    table.set_value("test1", testval1);
    uint64 get_testval_1 = 0;
    get_testval_1 = table.get_value("test1");
    expect_should_be(testval1, get_testval_1);

    table.free_data();

    expect_should_be(0, table.buffer.data);
    expect_should_be(0, table.element_count);

    return true;
}

typedef struct ht_test_struct {
    bool8 b_value;
    float32 f_value;
    uint64 u_value;
} ht_test_struct;

uint8 hashtable_should_set_and_get_ptr_successfully() {
    Hashtable<ht_test_struct*> table;
    uint32 element_count = 3;

    table.init(element_count, AllocationTag::RAW);

    expect_should_not_be(0, table.buffer.data);
    expect_should_be(3, table.element_count);

    ht_test_struct t;
    ht_test_struct* testval1 = &t;
    testval1->b_value = true;
    testval1->u_value = 63;
    testval1->f_value = 3.1415f;
    table.set_value("test1", testval1);

    ht_test_struct* get_testval_1 = 0;
    get_testval_1 = table.get_value("test1");

    expect_should_be(testval1->b_value, get_testval_1->b_value);
    expect_should_be(testval1->u_value, get_testval_1->u_value);

    table.free_data();

    expect_should_be(0, table.buffer.data);
    expect_should_be(0, table.element_count);

    return true;
}

uint8 hashtable_should_set_and_get_nonexistant() {
    Hashtable<uint64> table;
    uint32 element_count = 3;

    table.init(element_count, AllocationTag::RAW);

    expect_should_not_be(0, table.buffer.data);
    expect_should_be(3, table.element_count);

    uint64 testval1 = 23;
    table.set_value("test1", testval1);
    uint64 get_testval_1 = 0;
    get_testval_1 = table.get_value("test2");
    expect_should_be(0, get_testval_1);

    table.free_data();

    expect_should_be(0, table.buffer.data);
    expect_should_be(0, table.element_count);

    return true;
}

uint8 hashtable_should_set_and_get_ptr_nonexistant() {
    Hashtable<ht_test_struct*> table;
    uint32 element_count = 3;

    table.init(element_count, AllocationTag::RAW);

    expect_should_not_be(0, table.buffer.data);
    expect_should_be(3, table.element_count);

    ht_test_struct t;
    ht_test_struct* testval1 = &t;
    testval1->b_value = true;
    testval1->u_value = 63;
    testval1->f_value = 3.1415f;
    table.set_value("test1", testval1);

    ht_test_struct* get_testval_1 = 0;
    get_testval_1 = table.get_value("test2");
    expect_should_be(0, get_testval_1);

    table.free_data();

    expect_should_be(0, table.buffer.data);
    expect_should_be(0, table.element_count);

    return true;
}

uint8 hashtable_should_set_and_unset_ptr() {
    Hashtable<ht_test_struct*> table;
    uint32 element_count = 3;

    table.init(element_count, AllocationTag::RAW);

    expect_should_not_be(0, table.buffer.data);
    expect_should_be(3, table.element_count);

    ht_test_struct t;
    ht_test_struct* testval1 = &t;
    testval1->b_value = true;
    testval1->u_value = 63;
    testval1->f_value = 3.1415f;
    // Set it
    table.set_value("test1", testval1);

    // Check that it exists and is correct.
    ht_test_struct* get_testval_1 = 0;
    get_testval_1 = table.get_value("test1");
    expect_should_be(testval1->b_value, get_testval_1->b_value);
    expect_should_be(testval1->u_value, get_testval_1->u_value);

    // Unset it
    table.set_value("test1", 0);

    // Should no longer be found.
    ht_test_struct* get_testval_2 = 0;
    get_testval_2 = table.get_value("test1");
    expect_should_be(0, get_testval_2);

    table.free_data();

    expect_should_be(0, table.buffer.data);
    expect_should_be(0, table.element_count);

    return true;
}

uint8 hashtable_should_set_get_and_update_ptr_successfully() {
    Hashtable<ht_test_struct*> table;
    uint32 element_count = 3;

    table.init(element_count, AllocationTag::RAW);

    expect_should_not_be(0, table.buffer.data);
    expect_should_be(3, table.element_count);

    ht_test_struct t;
    ht_test_struct* testval1 = &t;
    testval1->b_value = true;
    testval1->u_value = 63;
    testval1->f_value = 3.1415f;
    table.set_value("test1", testval1);

    ht_test_struct* get_testval_1 = 0;
    get_testval_1 = table.get_value("test1");
    expect_should_be(testval1->b_value, get_testval_1->b_value);
    expect_should_be(testval1->u_value, get_testval_1->u_value);

    // Update pointed-to values
    get_testval_1->b_value = false;
    get_testval_1->u_value = 99;
    get_testval_1->f_value = 6.69f;

    // Get the pointer again and confirm correct values
    ht_test_struct* get_testval_2 = 0;
    get_testval_2 = table.get_value("test1");
    expect_to_be_false(get_testval_2->b_value);
    expect_should_be(99, get_testval_2->u_value);
    expect_float_to_be(6.69f, get_testval_2->f_value);

    table.free_data();

    expect_should_be(0, table.buffer.data);
    expect_should_be(0, table.element_count);

    return true;
}

void hashtable_register_tests() 
{
    test_manager_register_test(hashtable_should_set_and_get_successfully, "Hashtable should set and get");
    test_manager_register_test(hashtable_should_set_and_get_ptr_successfully, "Hashtable should set and get pointer");
    test_manager_register_test(hashtable_should_set_and_get_nonexistant, "Hashtable should set and get non-existent entry as nothing.");
    test_manager_register_test(hashtable_should_set_and_get_ptr_nonexistant, "Hashtable should set and get non-existent pointer entry as nothing.");
    test_manager_register_test(hashtable_should_set_and_unset_ptr, "Hashtable should set and unset pointer entry as nothing.");
    test_manager_register_test(hashtable_should_set_get_and_update_ptr_successfully, "Hashtable Should get pointer, update, and get again successfully.");
}