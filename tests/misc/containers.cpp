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
#include "fl/stl/vector.h"
#include "fl/stl/deque.h"
#include "fl/stl/list.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

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
// Test Functions - Generic Container Template Pattern
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
// Test Cases - Organized by Operation
// ============================================================================

FL_TEST_CASE("push_back - all container types (move semantics verified)") {
    FL_SUBCASE("fl::vector") { test_push_back<vector>(); }
    FL_SUBCASE("fl::deque") { test_push_back<deque>(); }
    FL_SUBCASE("fl::list") { test_push_back<list>(); }
}

FL_TEST_CASE("emplace_back - all container types") {
    FL_SUBCASE("fl::vector") { test_emplace_back<vector>(); }
    FL_SUBCASE("fl::deque") { test_emplace_back<deque>(); }
    FL_SUBCASE("fl::list") { test_emplace_back<list>(); }
}

FL_TEST_CASE("pop_back - all container types") {
    FL_SUBCASE("fl::vector") { test_pop_back<vector>(); }
    FL_SUBCASE("fl::deque") { test_pop_back<deque>(); }
    FL_SUBCASE("fl::list") { test_pop_back<list>(); }
}

FL_TEST_CASE("size and empty - all container types") {
    FL_SUBCASE("fl::vector") { test_size_and_empty<vector>(); }
    FL_SUBCASE("fl::deque") { test_size_and_empty<deque>(); }
    FL_SUBCASE("fl::list") { test_size_and_empty<list>(); }
}

FL_TEST_CASE("clear - all container types") {
    FL_SUBCASE("fl::vector") { test_clear<vector>(); }
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

FL_TEST_CASE("operator[] - vector and deque only") {
    FL_SUBCASE("fl::vector") { test_operator_subscript<vector>(); }
    FL_SUBCASE("fl::deque") { test_operator_subscript<deque>(); }
}

FL_TEST_CASE("insert and erase - all sequential containers") {
    FL_SUBCASE("fl::vector") { test_insert_and_erase<vector>(); }
    FL_SUBCASE("fl::deque") { test_insert_and_erase<deque>(); }
    FL_SUBCASE("fl::list") { test_insert_and_erase<list>(); }
}

} // FL_TEST_FILE
