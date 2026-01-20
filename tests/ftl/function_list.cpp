/// @file function_list.cpp
/// @brief Comprehensive tests for function_list class

#include "fl/stl/function.h"
#include "fl/unused.h"
#include "fl/stl/stdint.h"
#include "fl/stl/new.h"
#include "doctest.h"
#include "fl/stl/allocator.h"
#include "fl/stl/move.h"
#include "fl/stl/vector.h"

using namespace fl;

TEST_CASE("function_list<void()> - no arguments") {
    function_list<void()> callbacks;
    int call_count = 0;
    
    callbacks.add([&call_count]() { call_count++; });
    REQUIRE(call_count == 0);
    
    callbacks.invoke();
    REQUIRE(call_count == 1);
    
    callbacks.invoke();  // Test operator()
    REQUIRE(call_count == 2);
}

TEST_CASE("function_list<void(float)> - single argument") {
    function_list<void(float)> callbacks;
    float received_value = 0.0f;
    
    callbacks.add([&received_value](float v) { received_value = v; });
    callbacks.invoke(42.5f);
    REQUIRE(received_value == 42.5f);
    
    callbacks.invoke(99.9f);  // Test operator()
    REQUIRE(received_value == 99.9f);
}

TEST_CASE("function_list<void(uint8_t, float, float)> - multiple arguments") {
    function_list<void(uint8_t, float, float)> callbacks;
    uint8_t received_u8 = 0;
    float received_f1 = 0.0f;
    float received_f2 = 0.0f;
    
    callbacks.add([&](uint8_t u8, float f1, float f2) {
        received_u8 = u8;
        received_f1 = f1;
        received_f2 = f2;
    });
    
    callbacks.invoke(123, 1.5f, 2.5f);
    REQUIRE(received_u8 == 123);
    REQUIRE(received_f1 == 1.5f);
    REQUIRE(received_f2 == 2.5f);
}

TEST_CASE("function_list<void()> - function signature syntax with no args") {
    function_list<void()> callbacks;
    int call_count = 0;
    
    callbacks.add([&call_count]() { call_count++; });
    callbacks.invoke();
    REQUIRE(call_count == 1);
    
    callbacks.invoke();
    REQUIRE(call_count == 2);
}

TEST_CASE("function_list<void(float)> - function signature syntax with single arg") {
    function_list<void(float)> callbacks;
    float received_value = 0.0f;
    
    callbacks.add([&received_value](float v) { received_value = v; });
    callbacks.invoke(3.14f);
    REQUIRE(received_value == 3.14f);
}

TEST_CASE("function_list<void(uint8_t, float, float)> - function signature syntax with multiple args") {
    function_list<void(uint8_t, float, float)> callbacks;
    uint8_t received_u8 = 0;
    float received_f1 = 0.0f;
    float received_f2 = 0.0f;
    
    callbacks.add([&](uint8_t u8, float f1, float f2) {
        received_u8 = u8;
        received_f1 = f1;
        received_f2 = f2;
    });
    
    callbacks.invoke(200, 5.0f, 10.0f);
    REQUIRE(received_u8 == 200);
    REQUIRE(received_f1 == 5.0f);
    REQUIRE(received_f2 == 10.0f);
}

TEST_CASE("function_list - add() returns unique IDs") {
    function_list<void()> callbacks;
    
    int id1 = callbacks.add([]() {});
    int id2 = callbacks.add([]() {});
    int id3 = callbacks.add([]() {});
    
    REQUIRE(id1 != id2);
    REQUIRE(id2 != id3);
    REQUIRE(id1 != id3);
}

TEST_CASE("function_list - remove() by ID") {
    function_list<void()> callbacks;
    int call_count_1 = 0;
    int call_count_2 = 0;
    int call_count_3 = 0;

    int id1 = callbacks.add([&call_count_1]() { call_count_1++; });
    int id2 = callbacks.add([&call_count_2]() { call_count_2++; });
    int id3 = callbacks.add([&call_count_3]() { call_count_3++; });
    (void)id1;
    (void)id3;

    callbacks.invoke();
    REQUIRE(call_count_1 == 1);
    REQUIRE(call_count_2 == 1);
    REQUIRE(call_count_3 == 1);

    callbacks.remove(id2);  // Remove middle callback
    callbacks.invoke();
    REQUIRE(call_count_1 == 2);
    REQUIRE(call_count_2 == 1);  // Should not be called again
    REQUIRE(call_count_3 == 2);
}

TEST_CASE("function_list - clear() removes all callbacks") {
    function_list<void()> callbacks;
    int call_count = 0;
    
    callbacks.add([&call_count]() { call_count++; });
    callbacks.add([&call_count]() { call_count++; });
    callbacks.add([&call_count]() { call_count++; });
    
    callbacks.invoke();
    REQUIRE(call_count == 3);
    
    callbacks.clear();
    callbacks.invoke();
    REQUIRE(call_count == 3);  // No additional calls after clear()
}

TEST_CASE("function_list - empty() and size()") {
    function_list<void()> callbacks;
    
    REQUIRE(callbacks.empty());
    REQUIRE(callbacks.size() == 0);
    
    int id1 = callbacks.add([]() {});
    REQUIRE_FALSE(callbacks.empty());
    REQUIRE(callbacks.size() == 1);

    int id2 = callbacks.add([]() {});
    (void)id2;
    REQUIRE(callbacks.size() == 2);
    
    callbacks.remove(id1);
    REQUIRE(callbacks.size() == 1);
    
    callbacks.clear();
    REQUIRE(callbacks.empty());
    REQUIRE(callbacks.size() == 0);
}

TEST_CASE("function_list - operator bool()") {
    function_list<void()> callbacks;
    
    REQUIRE_FALSE(callbacks);  // Empty list is false
    
    int id = callbacks.add([]() {});
    REQUIRE(callbacks);  // Non-empty list is true
    
    callbacks.remove(id);
    REQUIRE_FALSE(callbacks);  // Empty again
}

TEST_CASE("function_list - multiple callbacks invoked in order") {
    function_list<void(int)> callbacks;
    fl::vector<int> call_order;
    
    callbacks.add([&call_order](int value) { call_order.push_back(value * 1); });
    callbacks.add([&call_order](int value) { call_order.push_back(value * 2); });
    callbacks.add([&call_order](int value) { call_order.push_back(value * 3); });
    
    callbacks.invoke(10);
    
    REQUIRE(call_order.size() == 3);
    REQUIRE(call_order[0] == 10);
    REQUIRE(call_order[1] == 20);
    REQUIRE(call_order[2] == 30);
}

TEST_CASE("function_list - backward compatibility with function_list<void()>") {
    function_list<void()> callbacks;
    int call_count = 0;
    
    callbacks.add([&call_count]() { call_count++; });
    callbacks.invoke();
    REQUIRE(call_count == 1);
    
    callbacks.invoke();
    REQUIRE(call_count == 2);
}

TEST_CASE("function_list - self-removal during iteration") {
    function_list<void()> callbacks;
    int call_count_1 = 0;
    int call_count_2 = 0;
    int call_count_3 = 0;

    int id1 = -1;
    int id2 = -1;
    int id3 = -1;

    // First callback removes itself
    id1 = callbacks.add([&]() {
        call_count_1++;
        callbacks.remove(id1);
    });

    // Second callback is normal
    id2 = callbacks.add([&]() {
        call_count_2++;
    });
    FL_UNUSED(id2);  // id2 not used after assignment - callback doesn't remove itself

    // Third callback removes itself
    id3 = callbacks.add([&]() {
        call_count_3++;
        callbacks.remove(id3);
    });

    callbacks.invoke();
    REQUIRE(call_count_1 == 1);
    REQUIRE(call_count_2 == 1);
    REQUIRE(call_count_3 == 1);

    // Second invocation - only id2 should execute (id1 and id3 removed themselves)
    callbacks.invoke();
    REQUIRE(call_count_1 == 1);  // No change
    REQUIRE(call_count_2 == 2);  // Incremented
    REQUIRE(call_count_3 == 1);  // No change
}

TEST_CASE("function_list - remove other callback during iteration") {
    function_list<void()> callbacks;
    int call_count_1 = 0;
    int call_count_2 = 0;
    int call_count_3 = 0;

    int id1 = -1;
    int id2 = -1;
    int id3 = -1;

    // First callback removes the third callback
    id1 = callbacks.add([&]() {
        call_count_1++;
        callbacks.remove(id3);
    });
    FL_UNUSED(id1);  // id1 not used after assignment - only id3 is removed

    id2 = callbacks.add([&]() {
        call_count_2++;
    });
    FL_UNUSED(id2);  // id2 not used after assignment - normal callback

    id3 = callbacks.add([&]() {
        call_count_3++;
    });

    callbacks.invoke();
    REQUIRE(call_count_1 == 1);
    REQUIRE(call_count_2 == 1);
    REQUIRE(call_count_3 == 0);  // Never called because removed by id1
}

TEST_CASE("function_list - remove callback before current position") {
    function_list<void()> callbacks;
    int call_count_1 = 0;
    int call_count_2 = 0;
    int call_count_3 = 0;

    int id1 = -1;
    int id2 = -1;
    int id3 = -1;

    id1 = callbacks.add([&]() {
        call_count_1++;
    });

    id2 = callbacks.add([&]() {
        call_count_2++;
    });
    FL_UNUSED(id2);  // id2 not used after assignment - normal callback

    // Third callback removes the first callback (already executed)
    id3 = callbacks.add([&]() {
        call_count_3++;
        callbacks.remove(id1);
    });
    FL_UNUSED(id3);  // id3 not used after assignment - removes id1 in lambda

    callbacks.invoke();
    REQUIRE(call_count_1 == 1);
    REQUIRE(call_count_2 == 1);
    REQUIRE(call_count_3 == 1);

    // Second invocation - id1 should be gone
    callbacks.invoke();
    REQUIRE(call_count_1 == 1);  // No change
    REQUIRE(call_count_2 == 2);  // Incremented
    REQUIRE(call_count_3 == 2);  // Incremented
}

TEST_CASE("function_list - nested invocations") {
    function_list<void()> callbacks;
    int call_count_outer = 0;
    int call_count_inner = 0;

    function_list<void()> inner_callbacks;
    inner_callbacks.add([&]() {
        call_count_inner++;
    });

    // Outer callback invokes inner callback list
    callbacks.add([&]() {
        call_count_outer++;
        inner_callbacks.invoke();
    });

    callbacks.invoke();
    REQUIRE(call_count_outer == 1);
    REQUIRE(call_count_inner == 1);
}

TEST_CASE("function_list - recursive self-invocation") {
    function_list<void(int)> callbacks;
    int total_calls = 0;

    callbacks.add([&](int depth) {
        total_calls++;
        if (depth > 0) {
            callbacks.invoke(depth - 1);  // Recursive call
        }
    });

    callbacks.invoke(3);  // Should call: depth=3, depth=2, depth=1, depth=0
    REQUIRE(total_calls == 4);
}

TEST_CASE("function_list - multiple removals in one callback") {
    function_list<void()> callbacks;
    int call_count_1 = 0;
    int call_count_2 = 0;
    int call_count_3 = 0;
    int call_count_4 = 0;

    int id1 = -1;
    int id2 = -1;
    int id3 = -1;
    int id4 = -1;

    // First callback removes multiple others
    id1 = callbacks.add([&]() {
        call_count_1++;
        callbacks.remove(id2);
        callbacks.remove(id4);
    });
    FL_UNUSED(id1);  // id1 not used after assignment - removes id2 and id4 in lambda

    id2 = callbacks.add([&]() {
        call_count_2++;
    });

    id3 = callbacks.add([&]() {
        call_count_3++;
    });
    FL_UNUSED(id3);  // id3 not used after assignment - normal callback

    id4 = callbacks.add([&]() {
        call_count_4++;
    });

    callbacks.invoke();
    REQUIRE(call_count_1 == 1);
    REQUIRE(call_count_2 == 0);  // Removed before execution
    REQUIRE(call_count_3 == 1);  // Not removed
    REQUIRE(call_count_4 == 0);  // Removed before execution
}

TEST_CASE("function_list - add during iteration") {
    function_list<void()> callbacks;
    int call_count_1 = 0;
    int call_count_2 = 0;

    // First callback adds a second callback
    callbacks.add([&]() {
        call_count_1++;
        callbacks.add([&]() {
            call_count_2++;
        });
    });

    callbacks.invoke();
    REQUIRE(call_count_1 == 1);
    REQUIRE(call_count_2 == 0);  // New callback won't execute until next call

    // Second invocation should execute the newly added callback
    callbacks.invoke();
    REQUIRE(call_count_1 == 2);
    REQUIRE(call_count_2 == 1);  // Now the second callback executes
}

// Compile-time error test - commented out because it should NOT compile
// Uncomment this test to verify that non-void return types trigger static_assert
/*
TEST_CASE("function_list<int(float)> - should NOT compile due to non-void return type") {
    // This should trigger static_assert with message:
    // "function_list only supports void return type."
    function_list<int(float)> callbacks;  // ERROR: non-void return type
}
*/
