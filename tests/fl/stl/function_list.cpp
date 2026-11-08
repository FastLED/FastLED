
/// @file function_list.cpp
/// @brief Comprehensive tests for function_list class

#include "fl/stl/function.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/stdint.h"
#include "fl/stl/new.h"
#include "test.h"
#include "fl/stl/allocator.h"
#include "fl/stl/move.h"
#include "fl/stl/vector.h"

#if defined(FASTLED_TEST_ALLOC_FAILURE_WRAP)
namespace {
bool failFunctionListAllocation = false;
}
extern "C" void* __real__ZN2fl9libc_impl13aligned_allocEmm(fl::size_t, fl::size_t) FL_NO_EXCEPT;
extern "C" void* __wrap__ZN2fl9libc_impl13aligned_allocEmm(fl::size_t alignment, fl::size_t size) FL_NO_EXCEPT {
    return failFunctionListAllocation ? nullptr
        : __real__ZN2fl9libc_impl13aligned_allocEmm(alignment, size);
}
#endif

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

FL_TEST_CASE("function_list<void()> - no arguments") {
    function_list<void()> callbacks;
    int call_count = 0;
    
    callbacks.add([&call_count]() { call_count++; });
    FL_REQUIRE(call_count == 0);
    
    callbacks.invoke();
    FL_REQUIRE(call_count == 1);
    
    callbacks.invoke();  // Test operator()
    FL_REQUIRE(call_count == 2);
}

FL_TEST_CASE("function_list<void(float)> - single argument") {
    function_list<void(float)> callbacks;
    float received_value = 0.0f;
    
    callbacks.add([&received_value](float v) { received_value = v; });
    callbacks.invoke(42.5f);
    FL_REQUIRE(received_value == 42.5f);
    
    callbacks.invoke(99.9f);  // Test operator()
    FL_REQUIRE(received_value == 99.9f);
}

FL_TEST_CASE("function_list<void(uint8_t, float, float)> - multiple arguments") {
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
    FL_REQUIRE(received_u8 == 123);
    FL_REQUIRE(received_f1 == 1.5f);
    FL_REQUIRE(received_f2 == 2.5f);
}

FL_TEST_CASE("function_list<void()> - function signature syntax with no args") {
    function_list<void()> callbacks;
    int call_count = 0;
    
    callbacks.add([&call_count]() { call_count++; });
    callbacks.invoke();
    FL_REQUIRE(call_count == 1);
    
    callbacks.invoke();
    FL_REQUIRE(call_count == 2);
}

FL_TEST_CASE("function_list<void(float)> - function signature syntax with single arg") {
    function_list<void(float)> callbacks;
    float received_value = 0.0f;
    
    callbacks.add([&received_value](float v) { received_value = v; });
    callbacks.invoke(3.14f);
    FL_REQUIRE(received_value == 3.14f);
}

FL_TEST_CASE("function_list<void(uint8_t, float, float)> - function signature syntax with multiple args") {
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
    FL_REQUIRE(received_u8 == 200);
    FL_REQUIRE(received_f1 == 5.0f);
    FL_REQUIRE(received_f2 == 10.0f);
}

FL_TEST_CASE("function_list - add() returns unique IDs") {
    function_list<void()> callbacks;
    
    int id1 = callbacks.add([]() {});
    int id2 = callbacks.add([]() {});
    int id3 = callbacks.add([]() {});
    
    FL_REQUIRE(id1 != id2);
    FL_REQUIRE(id2 != id3);
    FL_REQUIRE(id1 != id3);
}

FL_TEST_CASE("function_list - remove() by ID") {
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
    FL_REQUIRE(call_count_1 == 1);
    FL_REQUIRE(call_count_2 == 1);
    FL_REQUIRE(call_count_3 == 1);

    callbacks.remove(id2);  // Remove middle callback
    callbacks.invoke();
    FL_REQUIRE(call_count_1 == 2);
    FL_REQUIRE(call_count_2 == 1);  // Should not be called again
    FL_REQUIRE(call_count_3 == 2);
}

FL_TEST_CASE("function_list - clear() removes all callbacks") {
    function_list<void()> callbacks;
    int call_count = 0;
    
    callbacks.add([&call_count]() { call_count++; });
    callbacks.add([&call_count]() { call_count++; });
    callbacks.add([&call_count]() { call_count++; });
    
    callbacks.invoke();
    FL_REQUIRE(call_count == 3);
    
    callbacks.clear();
    callbacks.invoke();
    FL_REQUIRE(call_count == 3);  // No additional calls after clear()
}

FL_TEST_CASE("function_list - empty() and size()") {
    function_list<void()> callbacks;
    
    FL_REQUIRE(callbacks.empty());
    FL_REQUIRE(callbacks.size() == 0);
    
    int id1 = callbacks.add([]() {});
    FL_REQUIRE_FALSE(callbacks.empty());
    FL_REQUIRE(callbacks.size() == 1);

    int id2 = callbacks.add([]() {});
    (void)id2;
    FL_REQUIRE(callbacks.size() == 2);
    
    callbacks.remove(id1);
    FL_REQUIRE(callbacks.size() == 1);
    
    callbacks.clear();
    FL_REQUIRE(callbacks.empty());
    FL_REQUIRE(callbacks.size() == 0);
}

FL_TEST_CASE("function_list - operator bool()") {
    function_list<void()> callbacks;
    
    FL_REQUIRE_FALSE(callbacks);  // Empty list is false
    
    int id = callbacks.add([]() {});
    FL_REQUIRE(callbacks);  // Non-empty list is true
    
    callbacks.remove(id);
    FL_REQUIRE_FALSE(callbacks);  // Empty again
}

FL_TEST_CASE("function_list - multiple callbacks invoked in order") {
    function_list<void(int)> callbacks;
    fl::vector<int> call_order;
    
    callbacks.add([&call_order](int value) { call_order.push_back(value * 1); });
    callbacks.add([&call_order](int value) { call_order.push_back(value * 2); });
    callbacks.add([&call_order](int value) { call_order.push_back(value * 3); });
    
    callbacks.invoke(10);
    
    FL_REQUIRE(call_order.size() == 3);
    FL_REQUIRE(call_order[0] == 10);
    FL_REQUIRE(call_order[1] == 20);
    FL_REQUIRE(call_order[2] == 30);
}

FL_TEST_CASE("function_list - backward compatibility with function_list<void()>") {
    function_list<void()> callbacks;
    int call_count = 0;
    
    callbacks.add([&call_count]() { call_count++; });
    callbacks.invoke();
    FL_REQUIRE(call_count == 1);
    
    callbacks.invoke();
    FL_REQUIRE(call_count == 2);
}

FL_TEST_CASE("function_list - self-removal during iteration") {
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
    FL_REQUIRE(call_count_1 == 1);
    FL_REQUIRE(call_count_2 == 1);
    FL_REQUIRE(call_count_3 == 1);

    // Second invocation - only id2 should execute (id1 and id3 removed themselves)
    callbacks.invoke();
    FL_REQUIRE(call_count_1 == 1);  // No change
    FL_REQUIRE(call_count_2 == 2);  // Incremented
    FL_REQUIRE(call_count_3 == 1);  // No change
}

FL_TEST_CASE("function_list - remove other callback during iteration") {
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
    FL_REQUIRE(call_count_1 == 1);
    FL_REQUIRE(call_count_2 == 1);
    FL_REQUIRE(call_count_3 == 0);  // Never called because removed by id1
}

FL_TEST_CASE("function_list - remove callback before current position") {
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
    FL_REQUIRE(call_count_1 == 1);
    FL_REQUIRE(call_count_2 == 1);
    FL_REQUIRE(call_count_3 == 1);

    // Second invocation - id1 should be gone
    callbacks.invoke();
    FL_REQUIRE(call_count_1 == 1);  // No change
    FL_REQUIRE(call_count_2 == 2);  // Incremented
    FL_REQUIRE(call_count_3 == 2);  // Incremented
}

FL_TEST_CASE("function_list - nested invocations") {
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
    FL_REQUIRE(call_count_outer == 1);
    FL_REQUIRE(call_count_inner == 1);
}

FL_TEST_CASE("function_list - recursive self-invocation") {
    function_list<void(int)> callbacks;
    int total_calls = 0;

    callbacks.add([&](int depth) {
        total_calls++;
        if (depth > 0) {
            callbacks.invoke(depth - 1);  // Recursive call
        }
    });

    callbacks.invoke(3);  // Should call: depth=3, depth=2, depth=1, depth=0
    FL_REQUIRE(total_calls == 4);
}

FL_TEST_CASE("function_list - multiple removals in one callback") {
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
    FL_REQUIRE(call_count_1 == 1);
    FL_REQUIRE(call_count_2 == 0);  // Removed before execution
    FL_REQUIRE(call_count_3 == 1);  // Not removed
    FL_REQUIRE(call_count_4 == 0);  // Removed before execution
}

FL_TEST_CASE("function_list - add during iteration") {
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
    FL_REQUIRE(call_count_1 == 1);
    FL_REQUIRE(call_count_2 == 0);  // New callback won't execute until next call

    // Second invocation should execute the newly added callback
    callbacks.invoke();
    FL_REQUIRE(call_count_1 == 2);
    FL_REQUIRE(call_count_2 == 1);  // Now the second callback executes
}

FL_TEST_CASE("function_list - empty dispatch then late subscription and copy") {
    function_list<void(int)> callbacks;
    callbacks(1);
    callbacks.clear();
    callbacks(2);
    int received = 0;
    int id = callbacks.add([&](int value) { received += value; });
    callbacks(3);
    FL_CHECK_EQ(received, 3);
    auto copied = callbacks;
    copied(4);
    FL_CHECK_EQ(received, 7);
    callbacks.remove(id);
    callbacks(5);
    FL_CHECK_EQ(received, 7);
    callbacks.add([&](int value) { received += value; });
    callbacks(6);
    FL_CHECK_EQ(received, 13);
}

FL_TEST_CASE("function_list - priorities preserve insertion and removal") {
    function_list<void()> callbacks;
    fl::vector<int> order;
    int removed = callbacks.add([&]() { order.push_back(0); }, -1);
    callbacks.add([&]() { order.push_back(1); callbacks.remove(removed); }, 10);
    callbacks.add([&]() { order.push_back(2); }, 10);
    callbacks.add([&]() { order.push_back(3); }, 0);
    callbacks();
    FL_REQUIRE_EQ(order.size(), 3);
    FL_CHECK_EQ(order[0], 1);
    FL_CHECK_EQ(order[1], 2);
    FL_CHECK_EQ(order[2], 3);
}

FL_TEST_CASE("function_list - lazy storage keeps copy and move value semantics") {
    function_list<void()> original;
    original();
    int calls = 0;
    int id = original.add([&]() { ++calls; });
    function_list<void()> assigned;
    assigned = original;
    original.remove(id);
    original();
    assigned();
    FL_CHECK_EQ(calls, 1);

    function_list<void()> moved(fl::move(assigned));
    assigned();
    assigned.add([&]() { calls += 10; });
    assigned();
    FL_CHECK_EQ(calls, 11);
    moved();
    FL_CHECK_EQ(calls, 12);

    function_list<void()> moveAssigned;
    moveAssigned = fl::move(moved);
    moved();
    moved.add([&]() { calls += 100; });
    moved();
    FL_CHECK_EQ(calls, 112);
    moveAssigned();
    FL_CHECK_EQ(calls, 113);

    function_list<void()> empty;
    moveAssigned = empty;
    moveAssigned();
    FL_CHECK_TRUE(moveAssigned.empty());
    FL_CHECK_EQ(calls, 113);
}

#if defined(FASTLED_TEST_ALLOC_FAILURE_WRAP)
FL_TEST_CASE("function_list - failed copies consistently become empty") {
    struct AllocationProbe {};
    function_list<void(AllocationProbe)> source;
    function_list<void(AllocationProbe)> destination;
    source.add([](AllocationProbe) {});
    destination.add([](AllocationProbe) {});
    failFunctionListAllocation = true;
    function_list<void(AllocationProbe)> copied(source);
    destination = source;
    failFunctionListAllocation = false;
    FL_CHECK_TRUE(copied.empty());
    FL_CHECK_TRUE(destination.empty());
    FL_CHECK_EQ(source.size(), 1u);
}
#endif

FL_TEST_CASE("function_list - reassignment during dispatch retains the active state") {
    function_list<void()> callbacks;
    function_list<void()> replacement;
    int action = 0;
    FL_SUBCASE("self move") { action = 2; }
    FL_SUBCASE("assign other") { action = 1; }
    FL_SUBCASE("assign empty") { action = 0; }
    int first = 0;
    int tail = 0;
    int next = 0;
    replacement.add([&]() { ++next; });
    callbacks.add([&]() {
        ++first;
        if (action == 0) callbacks = function_list<void()>();
        else if (action == 1) callbacks = replacement;
        else callbacks = fl::move(callbacks);
    }, 10);
    callbacks.add([&]() { ++tail; });
    callbacks();
    FL_CHECK_EQ(first, 1);
    FL_CHECK_EQ(tail, action == 2 ? 1 : 0);
    FL_CHECK_EQ(next, 0);
    callbacks();
    FL_CHECK_EQ(next, action == 1 ? 1 : 0);
    FL_CHECK_EQ(tail, action == 2 ? 2 : 0);
}

template <typename List>
void runningCallbackLifetime(bool grow) {
    List callbacks;
    int result = 0;
    auto captured = fl::make_shared<int>(73);
    callbacks.add([captured, &callbacks, &result, grow]() {
        auto& list = callbacks;
        auto& output = result;
        if (grow) {
            for (int i = 0; i < 64; ++i) list.add([]() {});
        } else {
            list.clear();
        }
        output = *captured;
    });
    captured.reset();
    callbacks();
    FL_CHECK_EQ(result, 73);
}

FL_TEST_CASE("function_list - running callback survives clear and vector growth") {
    using SmallList = fl::detail::FunctionListImpl<void(), false>;
    FL_SUBCASE("small reallocate then read capture") { runningCallbackLifetime<SmallList>(true); }
    FL_SUBCASE("small clear then read capture") { runningCallbackLifetime<SmallList>(false); }
    FL_SUBCASE("large clear then read capture") { runningCallbackLifetime<function_list<void()>>(false); }
    FL_SUBCASE("large reallocate then read capture") { runningCallbackLifetime<function_list<void()>>(true); }
}

#if defined(FASTLED_TEST_ALLOC_FAILURE_WRAP)
FL_TEST_CASE("function_list - small tier add has no State allocation failure") {
    fl::detail::FunctionListImpl<void(), false> callbacks;
    int calls = 0;
    failFunctionListAllocation = true;
    const int id = callbacks.add([&]() { ++calls; });
    callbacks();
    failFunctionListAllocation = false;
    FL_CHECK_EQ(id, 0);
    FL_CHECK_EQ(calls, 1);
}
#endif

template <typename List>
void mutableCallbackAndCopy() {
    List callbacks;
    List copied;
    int observed = 0;
    struct Counter {
        fl::shared_ptr<int> value;
        List* list;
        List* copy;
        int* output;
        Counter(List& source, List& target, int& result) FL_NO_EXCEPT
            : value(fl::make_shared<int>(0)), list(&source), copy(&target), output(&result) {}
        Counter(const Counter& other) FL_NO_EXCEPT
            : value(fl::make_shared<int>(*other.value)), list(other.list), copy(other.copy), output(other.output) {}
        Counter(Counter&&) FL_NO_EXCEPT = default;
        void operator()() const FL_NO_EXCEPT {
            ++*value;
            if (*value == 1) {
                for (int i = 0; i < 64; ++i) list->add([]() {});
                ++*value;
                *copy = *list;
            }
            *output = *value;
        }
    };
    callbacks.add(Counter(callbacks, copied, observed));
    callbacks();
    FL_CHECK_EQ(observed, 2);
    callbacks();
    FL_CHECK_EQ(observed, 3);
    copied();
    FL_CHECK_EQ(observed, 3);
    callbacks();
    FL_CHECK_EQ(observed, 4);
}

FL_TEST_CASE("function_list - active owned callbacks keep state and copy safely") {
    FL_SUBCASE("small tier") { mutableCallbackAndCopy<fl::detail::FunctionListImpl<void(), false>>(); }
    FL_SUBCASE("large tier") { mutableCallbackAndCopy<function_list<void()>>(); }
}

template <typename List>
void clearAndRefillDuringDispatch() {
    List callbacks;
    int oldTail = 0;
    int added = 0;
    callbacks.add([&]() {
        callbacks.clear();
        callbacks.add([&]() { ++added; });
        callbacks.add([&]() { ++added; });
    });
    callbacks.add([&]() { ++oldTail; });
    callbacks();
    FL_CHECK_EQ(oldTail, 0);
    FL_CHECK_EQ(added, 0);
    callbacks();
    FL_CHECK_EQ(added, 2);
}

FL_TEST_CASE("function_list - clear and refill defers new callbacks") {
    FL_SUBCASE("small tier") { clearAndRefillDuringDispatch<fl::detail::FunctionListImpl<void(), false>>(); }
    FL_SUBCASE("large tier") { clearAndRefillDuringDispatch<function_list<void()>>(); }
}

// Compile-time error test - commented out because it should NOT compile
// Uncomment this test to verify that non-void return types trigger static_assert
/*
FL_TEST_CASE("function_list<int(float)> - should NOT compile due to non-void return type") {
    // This should trigger static_assert with message:
    // "function_list only supports void return type."
    function_list<int(float)> callbacks;  // ERROR: non-void return type
}
*/

} // FL_TEST_FILE
