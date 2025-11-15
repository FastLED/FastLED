/// @file function_list.cpp
/// @brief Comprehensive tests for FunctionList class

#include "test.h"
#include "fl/function_list.h"

using namespace fl;

TEST_CASE("FunctionList<void()> - no arguments") {
    FunctionList<void()> callbacks;
    int call_count = 0;
    
    callbacks.add([&call_count]() { call_count++; });
    REQUIRE(call_count == 0);
    
    callbacks.invoke();
    REQUIRE(call_count == 1);
    
    callbacks();  // Test operator()
    REQUIRE(call_count == 2);
}

TEST_CASE("FunctionList<void(float)> - single argument") {
    FunctionList<void(float)> callbacks;
    float received_value = 0.0f;
    
    callbacks.add([&received_value](float v) { received_value = v; });
    callbacks.invoke(42.5f);
    REQUIRE(received_value == 42.5f);
    
    callbacks(99.9f);  // Test operator()
    REQUIRE(received_value == 99.9f);
}

TEST_CASE("FunctionList<void(uint8_t, float, float)> - multiple arguments") {
    FunctionList<void(uint8_t, float, float)> callbacks;
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

TEST_CASE("FunctionList<void()> - function signature syntax with no args") {
    FunctionList<void()> callbacks;
    int call_count = 0;
    
    callbacks.add([&call_count]() { call_count++; });
    callbacks.invoke();
    REQUIRE(call_count == 1);
    
    callbacks();
    REQUIRE(call_count == 2);
}

TEST_CASE("FunctionList<void(float)> - function signature syntax with single arg") {
    FunctionList<void(float)> callbacks;
    float received_value = 0.0f;
    
    callbacks.add([&received_value](float v) { received_value = v; });
    callbacks.invoke(3.14f);
    REQUIRE(received_value == 3.14f);
}

TEST_CASE("FunctionList<void(uint8_t, float, float)> - function signature syntax with multiple args") {
    FunctionList<void(uint8_t, float, float)> callbacks;
    uint8_t received_u8 = 0;
    float received_f1 = 0.0f;
    float received_f2 = 0.0f;
    
    callbacks.add([&](uint8_t u8, float f1, float f2) {
        received_u8 = u8;
        received_f1 = f1;
        received_f2 = f2;
    });
    
    callbacks(200, 5.0f, 10.0f);
    REQUIRE(received_u8 == 200);
    REQUIRE(received_f1 == 5.0f);
    REQUIRE(received_f2 == 10.0f);
}

TEST_CASE("FunctionList - add() returns unique IDs") {
    FunctionList<void()> callbacks;
    
    int id1 = callbacks.add([]() {});
    int id2 = callbacks.add([]() {});
    int id3 = callbacks.add([]() {});
    
    REQUIRE(id1 != id2);
    REQUIRE(id2 != id3);
    REQUIRE(id1 != id3);
}

TEST_CASE("FunctionList - remove() by ID") {
    FunctionList<void()> callbacks;
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

TEST_CASE("FunctionList - clear() removes all callbacks") {
    FunctionList<void()> callbacks;
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

TEST_CASE("FunctionList - empty() and size()") {
    FunctionList<void()> callbacks;
    
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

TEST_CASE("FunctionList - operator bool()") {
    FunctionList<void()> callbacks;
    
    REQUIRE_FALSE(callbacks);  // Empty list is false
    
    int id = callbacks.add([]() {});
    REQUIRE(callbacks);  // Non-empty list is true
    
    callbacks.remove(id);
    REQUIRE_FALSE(callbacks);  // Empty again
}

TEST_CASE("FunctionList - multiple callbacks invoked in order") {
    FunctionList<void(int)> callbacks;
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

TEST_CASE("FunctionList - iterator support") {
    FunctionList<void()> callbacks;
    
    callbacks.add([]() {});
    callbacks.add([]() {});
    callbacks.add([]() {});
    
    int count = 0;
    for (const auto& pair : callbacks) {
        (void)pair;
        count++;
        // pair.first is the ID
        // pair.second is the function
    }
    
    REQUIRE(count == 3);
}

TEST_CASE("FunctionList - backward compatibility with FunctionList<void()>") {
    FunctionList<void()> callbacks;
    int call_count = 0;
    
    callbacks.add([&call_count]() { call_count++; });
    callbacks.invoke();
    REQUIRE(call_count == 1);
    
    callbacks();
    REQUIRE(call_count == 2);
}

// Compile-time error test - commented out because it should NOT compile
// Uncomment this test to verify that non-void return types trigger static_assert
/*
TEST_CASE("FunctionList<void(int(float))> - should NOT compile due to non-void return type") {
    // This should trigger static_assert with message:
    // "FunctionList only supports void return type."
    FunctionList<void(int(float))> callbacks;  // ERROR: non-void return type
}
*/
