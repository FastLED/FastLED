/// @file tests/misc/containers.cpp
/// @brief Comprehensive container operation tests across multiple container types
///
/// Tests basic container operations (push_back, emplace_back, etc.) for all
/// container types that support them, using a unified template-based approach.
///
/// Uses hybrid move-tracking approach: tracks move/copy operations and prevents
/// accidental copies with deleted copy constructor. This proves that operations
/// use move semantics, not copy constructors.

#include "test.h"
#include "test_container_helpers.h"
#include "fl/stl/vector.h"
#include "fl/stl/deque.h"
#include "fl/stl/list.h"
#include "fl/stl/queue.h"
#include "fl/stl/priority_queue.h"
#include "fl/stl/set.h"
#include "fl/stl/unordered_set.h"
#include "fl/stl/unordered_map.h"
#include "fl/stl/circular_buffer.h"
#include "fl/stl/shared_ptr.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

// ============================================================================
// Test Type and Helper Templates
// ============================================================================

// Template wrappers for vector variants with default size parameters
template <typename T> using vector_fixed_test = FixedVector<T, 512>;
template <typename T> using vector_inlined_test = InlinedVector<T, 64>;
template <typename T> using circular_buffer_test = circular_buffer<T, 512>;

// ============================================================================
// Hybrid Move-Tracking Test Type
// ============================================================================

struct TestItem {
    int value;
    int move_count = 0;
    int copy_count = 0;

    TestItem() : value(0) {}
    explicit TestItem(int v) : value(v) {}

    // Move constructor - tracks moves
    TestItem(TestItem&& other) noexcept
        : value(other.value), move_count(other.move_count + 1) {
        other.move_count++;
    }

    // Move assignment
    TestItem& operator=(TestItem&& other) noexcept {
        value = other.value;
        move_count = other.move_count + 1;
        other.move_count++;
        return *this;
    }

    // Delete copy constructor - prevents accidental copies
    TestItem(const TestItem&) = delete;
    TestItem& operator=(const TestItem&) = delete;

    // Equality for checking values
    bool operator==(const TestItem& other) const {
        return value == other.value;
    }
};

// ============================================================================
// Basic Operation Test Functions - Generic Container Template Pattern
// ============================================================================
// Uses template template parameters: test_func<vector>() instead of test_func<vector<TestItem>>()
// This decouples container type from element type.

template<template<typename> class ContainerTemplate>
void test_push_back() {
    using Container = ContainerTemplate<TestItem>;
    Container container;
    TestItem item(42);
    int initial_move_count = item.move_count;

    container.push_back(fl::move(item));

    FL_CHECK(container.size() == 1);
    FL_CHECK(container.back().value == 42);
    FL_CHECK(item.move_count > initial_move_count);
    FL_CHECK(container.back().copy_count == 0);
}

template<template<typename> class ContainerTemplate>
void test_emplace_back() {
    using Container = ContainerTemplate<TestItem>;
    Container container;
    container.emplace_back(99);

    FL_CHECK(container.size() == 1);
    FL_CHECK(container.back().value == 99);
    FL_CHECK(container.back().copy_count == 0);
}

template<template<typename> class ContainerTemplate>
void test_pop_back() {
    using Container = ContainerTemplate<TestItem>;
    Container container;
    container.push_back(fl::move(TestItem(10)));
    container.push_back(fl::move(TestItem(20)));
    FL_CHECK(container.size() == 2);

    container.pop_back();
    FL_CHECK(container.size() == 1);
    FL_CHECK(container.back().value == 10);
}

template<template<typename> class ContainerTemplate>
void test_size_and_empty() {
    using Container = ContainerTemplate<TestItem>;
    Container container;
    FL_CHECK(container.empty());
    FL_CHECK(container.size() == 0);

    container.push_back(fl::move(TestItem(1)));
    FL_CHECK(!container.empty());
    FL_CHECK(container.size() == 1);

    container.push_back(fl::move(TestItem(2)));
    FL_CHECK(container.size() == 2);
}

template<template<typename> class ContainerTemplate>
void test_clear() {
    using Container = ContainerTemplate<TestItem>;
    Container container;
    container.push_back(fl::move(TestItem(1)));
    container.push_back(fl::move(TestItem(2)));
    container.push_back(fl::move(TestItem(3)));
    FL_CHECK(container.size() == 3);

    container.clear();
    FL_CHECK(container.empty());
    FL_CHECK(container.size() == 0);
}

// Front operations (deque, list)
template<template<typename> class ContainerTemplate>
void test_push_front() {
    using Container = ContainerTemplate<TestItem>;
    Container container;
    TestItem item(42);
    int initial_move_count = item.move_count;

    container.push_front(fl::move(item));

    FL_CHECK(container.size() == 1);
    FL_CHECK(container.front().value == 42);
    FL_CHECK(item.move_count > initial_move_count);
    FL_CHECK(container.front().copy_count == 0);
}

template<template<typename> class ContainerTemplate>
void test_emplace_front() {
    using Container = ContainerTemplate<TestItem>;
    Container container;
    container.emplace_front(88);

    FL_CHECK(container.size() == 1);
    FL_CHECK(container.front().value == 88);
    FL_CHECK(container.front().copy_count == 0);
}

template<template<typename> class ContainerTemplate>
void test_pop_front() {
    using Container = ContainerTemplate<TestItem>;
    Container container;
    container.push_front(fl::move(TestItem(20)));
    container.push_front(fl::move(TestItem(10)));
    FL_CHECK(container.size() == 2);

    container.pop_front();
    FL_CHECK(container.size() == 1);
    FL_CHECK(container.front().value == 20);
}

// Indexed access (vector, deque)
template<template<typename> class ContainerTemplate>
void test_operator_subscript() {
    using Container = ContainerTemplate<TestItem>;
    Container container;
    container.emplace_back(100);
    container.emplace_back(200);
    container.emplace_back(300);

    FL_CHECK(container[0].value == 100);
    FL_CHECK(container[1].value == 200);
    FL_CHECK(container[2].value == 300);
}

// Insert/erase operations
template<template<typename> class ContainerTemplate>
void test_insert_and_erase() {
    using Container = ContainerTemplate<TestItem>;
    Container container;
    container.push_back(fl::move(TestItem(10)));
    container.push_back(fl::move(TestItem(30)));
    FL_CHECK(container.size() == 2);

    // Insert at position 1
    auto it = container.begin();
    ++it;
    TestItem item(20);
    int initial_move_count = item.move_count;
    container.insert(it, fl::move(item));

    FL_CHECK(container.size() == 3);
    FL_CHECK(item.move_count > initial_move_count);

    // Erase middle element
    it = container.begin();
    ++it;
    container.erase(it);
    FL_CHECK(container.size() == 2);
}

// ============================================================================
// Comparison Operator Test Functions
// ============================================================================

// Equality comparison template (tests == and !=)
template<typename ContainerFactory>
void test_operator_equals() {
    auto c1 = ContainerFactory::create(1, 2, 3);
    auto c2 = ContainerFactory::create(1, 2, 3);
    auto c3 = ContainerFactory::create(1, 2, 4);

    FL_CHECK(c1 == c2);
    FL_CHECK_FALSE(c1 != c2);
    FL_CHECK(c1 != c3);
    FL_CHECK_FALSE(c1 == c3);
}

// Less-than comparison template (tests < and <=)
template<typename ContainerFactory>
void test_operator_less() {
    auto c1 = ContainerFactory::create(1, 2, 3);
    auto c2 = ContainerFactory::create(1, 2, 4);
    auto c3 = ContainerFactory::create(1, 2, 3);

    FL_CHECK(c1 < c2);
    FL_CHECK_FALSE(c2 < c1);
    FL_CHECK(c1 <= c2);
    FL_CHECK(c1 <= c3);
    FL_CHECK_FALSE(c2 <= c1);
}

// Greater-than comparison template (tests > and >=)
template<typename ContainerFactory>
void test_operator_greater() {
    auto c1 = ContainerFactory::create(1, 2, 4);
    auto c2 = ContainerFactory::create(1, 2, 3);
    auto c3 = ContainerFactory::create(1, 2, 4);

    FL_CHECK(c1 > c2);
    FL_CHECK_FALSE(c2 > c1);
    FL_CHECK(c1 >= c2);
    FL_CHECK(c1 >= c3);
    FL_CHECK_FALSE(c2 >= c1);
}

// ============================================================================
// Iterator Contract Test Functions
// ============================================================================

template<typename Container>
void test_container_iteration_contract() {
    Container c;
    c.push_back(10);
    c.push_back(20);
    c.push_back(30);

    // Test begin() and end()
    FL_CHECK(c.begin() != c.end());

    // Test const iterators
    const auto& const_ref = c;
    FL_CHECK(const_ref.begin() != const_ref.end());

    // Test iterator dereference
    FL_CHECK(*c.begin() == 10);

    // Test iterator increment
    auto it = c.begin();
    ++it;
    FL_CHECK(*it == 20);
}

template<typename Container>
void test_container_reverse_iteration_contract() {
    Container c;
    c.push_back(10);
    c.push_back(20);
    c.push_back(30);

    // Test rbegin() and rend()
    FL_CHECK(c.rbegin() != c.rend());

    // Test reverse iterator dereference (should point to last element)
    FL_CHECK(*c.rbegin() == 30);
}

template<typename Container>
void test_container_const_iterator_contract() {
    Container c;
    c.push_back(10);
    c.push_back(20);
    c.push_back(30);

    const auto& const_c = c;

    // Test const begin/end
    FL_CHECK(const_c.begin() != const_c.end());

    // Test const dereference
    FL_CHECK(*const_c.begin() == 10);
}

// ============================================================================
// Insert/Erase Contract Test Functions
// ============================================================================

template<typename Container>
void test_container_tier2_contract() {
    Container c;
    c.push_back(10);
    c.push_back(30);

    FL_CHECK(c.size() == 2);
    FL_CHECK(c.front() == 10);
    FL_CHECK(c.back() == 30);

    // Test insert (if supported)
    auto it = c.begin();
    ++it;
    c.insert(it, 20);
    FL_CHECK(c.size() == 3);

    // Test erase (if supported)
    it = c.begin();
    ++it;
    c.erase(it);
    FL_CHECK(c.size() == 2);
}

// ============================================================================
// Capacity Management Contract Test Functions
// ============================================================================

template<typename Container>
void test_container_tier3_contract() {
    Container c;

    // Test capacity contract
    FL_CHECK(c.capacity() >= c.size());

    // Test reserve
    c.reserve(100);
    FL_CHECK(c.capacity() >= 100);

    // Add elements and test shrink_to_fit
    c.push_back(10);
    c.push_back(20);
    FL_CHECK(c.size() == 2);
    c.shrink_to_fit();
    FL_CHECK(c.capacity() >= 2);
}

// ============================================================================
// Iterator Arithmetic Contract Test Functions
// ============================================================================

template<typename Container>
void test_container_tier4_arithmetic() {
    Container c;
    c.push_back(10);
    c.push_back(20);
    c.push_back(30);

    // Test iterator arithmetic (vector/deque only - RandomAccess)
    auto it = c.begin();
    it = it + 1;
    FL_CHECK(*it == 20);

    it = it - 1;
    FL_CHECK(*it == 10);

    FL_CHECK(c.end() - c.begin() == 3);
}

template<typename Container>
void test_container_tier4_comparison() {
    Container c;
    c.push_back(10);
    c.push_back(20);
    c.push_back(30);

    // Test iterator comparison (vector/deque only - RandomAccess)
    auto it1 = c.begin();
    auto it2 = c.begin();
    ++it2;

    FL_CHECK(it1 < it2);
    FL_CHECK(it1 <= it2);
    FL_CHECK(it2 > it1);
    FL_CHECK(it2 >= it1);
    FL_CHECK(it1 <= it1);
}

// ============================================================================
// Container Factories for Comparison Operators
// ============================================================================

// Queue factory
struct QueueIntFactory {
    static queue<int> create(int a, int b, int c) {
        queue<int> q;
        q.push(a);
        q.push(b);
        q.push(c);
        return q;
    }
};

// Set factory
struct SetIntFactory {
    static set<int> create(int a, int b, int c) {
        set<int> s;
        s.insert(a);
        s.insert(b);
        s.insert(c);
        return s;
    }
};

// Circular buffer factory
struct CircularBufferIntFactory {
    static circular_buffer<int, 10> create(int a, int b, int c) {
        circular_buffer<int, 10> cb;
        cb.push_back(a);
        cb.push_back(b);
        cb.push_back(c);
        return cb;
    }
};

// ============================================================================
// Test Cases - All organized at the end after helper declarations
// ============================================================================

// Iterator contracts
FL_TEST_CASE("Iterator contracts - begin/end/const") {
    FL_SUBCASE("fl::vector") { test_container_iteration_contract<vector<int>>(); }
    FL_SUBCASE("fl::vector_fixed") { test_container_iteration_contract<vector_fixed_test<int>>(); }
    FL_SUBCASE("fl::vector_inlined") { test_container_iteration_contract<vector_inlined_test<int>>(); }
    FL_SUBCASE("fl::deque") { test_container_iteration_contract<deque<int>>(); }
    FL_SUBCASE("fl::list") { test_container_iteration_contract<list<int>>(); }
}

FL_TEST_CASE("Reverse iterator contracts - rbegin/rend") {
    FL_SUBCASE("fl::vector") { test_container_reverse_iteration_contract<vector<int>>(); }
    FL_SUBCASE("fl::deque") { test_container_reverse_iteration_contract<deque<int>>(); }
    FL_SUBCASE("fl::list") { test_container_reverse_iteration_contract<list<int>>(); }
    // NOTE: vector_fixed and vector_inlined do not support rbegin/rend
}

FL_TEST_CASE("Const iterator contracts - cbegin/cend") {
    FL_SUBCASE("fl::vector") { test_container_const_iterator_contract<vector<int>>(); }
    FL_SUBCASE("fl::vector_fixed") { test_container_const_iterator_contract<vector_fixed_test<int>>(); }
    FL_SUBCASE("fl::vector_inlined") { test_container_const_iterator_contract<vector_inlined_test<int>>(); }
    FL_SUBCASE("fl::deque") { test_container_const_iterator_contract<deque<int>>(); }
    FL_SUBCASE("fl::list") { test_container_const_iterator_contract<list<int>>(); }
}

// Insert/Erase contracts
FL_TEST_CASE("Insert/Erase contracts") {
    FL_SUBCASE("fl::vector") { test_container_tier2_contract<vector<int>>(); }
    FL_SUBCASE("fl::vector_fixed") { test_container_tier2_contract<vector_fixed_test<int>>(); }
    FL_SUBCASE("fl::vector_inlined") { test_container_tier2_contract<vector_inlined_test<int>>(); }
    FL_SUBCASE("fl::deque") { test_container_tier2_contract<deque<int>>(); }
    FL_SUBCASE("fl::list") { test_container_tier2_contract<list<int>>(); }
}

// Capacity management contracts
FL_TEST_CASE("Capacity management contracts") {
    FL_SUBCASE("fl::vector") { test_container_tier3_contract<vector<int>>(); }
    FL_SUBCASE("fl::deque") { test_container_tier3_contract<deque<int>>(); }
    // NOTE: fl::list, vector_fixed, and vector_inlined do not support capacity/reserve
}

// Iterator arithmetic contracts (RandomAccess only)
FL_TEST_CASE("Iterator arithmetic contracts") {
    FL_SUBCASE("fl::vector") { test_container_tier4_arithmetic<vector<int>>(); }
    FL_SUBCASE("fl::vector_fixed") { test_container_tier4_arithmetic<vector_fixed_test<int>>(); }
    FL_SUBCASE("fl::vector_inlined") { test_container_tier4_arithmetic<vector_inlined_test<int>>(); }
    FL_SUBCASE("fl::deque") { test_container_tier4_arithmetic<deque<int>>(); }
    // NOTE: fl::list is Bidirectional, does not support arithmetic
}

FL_TEST_CASE("Iterator comparison contracts") {
    FL_SUBCASE("fl::vector") { test_container_tier4_comparison<vector<int>>(); }
    FL_SUBCASE("fl::vector_fixed") { test_container_tier4_comparison<vector_fixed_test<int>>(); }
    FL_SUBCASE("fl::vector_inlined") { test_container_tier4_comparison<vector_inlined_test<int>>(); }
    FL_SUBCASE("fl::deque") { test_container_tier4_comparison<deque<int>>(); }
    // NOTE: fl::list is Bidirectional, does not support arithmetic comparison
}

// Basic container operations
FL_TEST_CASE("push_back - all container types (move semantics verified)") {
    FL_SUBCASE("fl::vector") { test_push_back<vector>(); }
    FL_SUBCASE("fl::vector_fixed") { test_push_back<vector_fixed_test>(); }
    FL_SUBCASE("fl::vector_inlined") { test_push_back<vector_inlined_test>(); }
    FL_SUBCASE("fl::deque") { test_push_back<deque>(); }
    FL_SUBCASE("fl::list") { test_push_back<list>(); }
}

FL_TEST_CASE("emplace_back - all container types") {
    FL_SUBCASE("fl::vector") { test_emplace_back<vector>(); }
    FL_SUBCASE("fl::vector_fixed") { test_emplace_back<vector_fixed_test>(); }
    FL_SUBCASE("fl::vector_inlined") { test_emplace_back<vector_inlined_test>(); }
    FL_SUBCASE("fl::deque") { test_emplace_back<deque>(); }
    FL_SUBCASE("fl::list") { test_emplace_back<list>(); }
}

FL_TEST_CASE("pop_back - all container types") {
    FL_SUBCASE("fl::vector") { test_pop_back<vector>(); }
    FL_SUBCASE("fl::vector_fixed") { test_pop_back<vector_fixed_test>(); }
    FL_SUBCASE("fl::vector_inlined") { test_pop_back<vector_inlined_test>(); }
    FL_SUBCASE("fl::deque") { test_pop_back<deque>(); }
    FL_SUBCASE("fl::list") { test_pop_back<list>(); }
}

FL_TEST_CASE("size and empty - all container types") {
    FL_SUBCASE("fl::vector") { test_size_and_empty<vector>(); }
    FL_SUBCASE("fl::vector_fixed") { test_size_and_empty<vector_fixed_test>(); }
    FL_SUBCASE("fl::vector_inlined") { test_size_and_empty<vector_inlined_test>(); }
    FL_SUBCASE("fl::deque") { test_size_and_empty<deque>(); }
    FL_SUBCASE("fl::list") { test_size_and_empty<list>(); }
}

FL_TEST_CASE("clear - all container types") {
    FL_SUBCASE("fl::vector") { test_clear<vector>(); }
    FL_SUBCASE("fl::vector_fixed") { test_clear<vector_fixed_test>(); }
    FL_SUBCASE("fl::vector_inlined") { test_clear<vector_inlined_test>(); }
    FL_SUBCASE("fl::deque") { test_clear<deque>(); }
    FL_SUBCASE("fl::list") { test_clear<list>(); }
}

FL_TEST_CASE("push_front - deque and list only") {
    FL_SUBCASE("fl::deque") { test_push_front<deque>(); }
    FL_SUBCASE("fl::list") { test_push_front<list>(); }
}

FL_TEST_CASE("emplace_front - deque and list") {
    FL_SUBCASE("fl::deque") { test_emplace_front<deque>(); }
    FL_SUBCASE("fl::list") { test_emplace_front<list>(); }
}

FL_TEST_CASE("pop_front - deque and list only") {
    FL_SUBCASE("fl::deque") { test_pop_front<deque>(); }
    FL_SUBCASE("fl::list") { test_pop_front<list>(); }
}

FL_TEST_CASE("operator[] - vector variants and deque") {
    FL_SUBCASE("fl::vector") { test_operator_subscript<vector>(); }
    FL_SUBCASE("fl::vector_fixed") { test_operator_subscript<vector_fixed_test>(); }
    FL_SUBCASE("fl::vector_inlined") { test_operator_subscript<vector_inlined_test>(); }
    FL_SUBCASE("fl::deque") { test_operator_subscript<deque>(); }
}

FL_TEST_CASE("insert and erase - all sequential containers") {
    FL_SUBCASE("fl::vector") { test_insert_and_erase<vector>(); }
    FL_SUBCASE("fl::vector_fixed") { test_insert_and_erase<vector_fixed_test>(); }
    FL_SUBCASE("fl::vector_inlined") { test_insert_and_erase<vector_inlined_test>(); }
    FL_SUBCASE("fl::deque") { test_insert_and_erase<deque>(); }
    FL_SUBCASE("fl::list") { test_insert_and_erase<list>(); }
}

// Comparison operators
FL_TEST_CASE("operator== and operator!= - all containers with comparison") {
    FL_SUBCASE("fl::queue") { test_operator_equals<QueueIntFactory>(); }
    FL_SUBCASE("fl::set") { test_operator_equals<SetIntFactory>(); }
    FL_SUBCASE("fl::circular_buffer") { test_operator_equals<CircularBufferIntFactory>(); }
}

FL_TEST_CASE("operator< and operator<= - all containers with comparison") {
    FL_SUBCASE("fl::queue") { test_operator_less<QueueIntFactory>(); }
    FL_SUBCASE("fl::set") { test_operator_less<SetIntFactory>(); }
    FL_SUBCASE("fl::circular_buffer") { test_operator_less<CircularBufferIntFactory>(); }
}

FL_TEST_CASE("operator> and operator>= - all containers with comparison") {
    FL_SUBCASE("fl::queue") { test_operator_greater<QueueIntFactory>(); }
    FL_SUBCASE("fl::set") { test_operator_greater<SetIntFactory>(); }
    FL_SUBCASE("fl::circular_buffer") { test_operator_greater<CircularBufferIntFactory>(); }
}

} // FL_TEST_FILE
