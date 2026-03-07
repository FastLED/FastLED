/// @file tests/misc/containers.cpp
/// @brief Comprehensive container operation tests across multiple container types
///
/// Tests basic container operations (push_back, emplace_back, etc.) for all
/// container types that support them, using a unified template-based approach.
///
/// Uses hybrid move-tracking approach: tracks move/copy operations and prevents
/// accidental copies with deleted copy constructor. This proves that operations
/// use move semantics, not copy constructors.
///
/// ============================================================================
/// CONTAINER TEST PATTERN DOCUMENTATION
/// ============================================================================
///
/// This file demonstrates the "templated validator pattern" for testing
/// containers across multiple types with maximum code reuse and test coverage.
///
/// ARCHITECTURE:
/// =============
/// The pattern has 4 main components:
///
/// 1. TEST TYPE SETUP (Container Normalization Layer)
///    - Container template aliases with default parameters
///    - Wrapper classes that normalize slightly different interfaces
///    - Allows one test to work for multiple containers
///
/// 2. TEST ITEM TYPE (Behavior Verification)
///    - TestItem struct tracks move/copy operations
///    - Deleted copy constructor catches accidental copies (compiler error)
///    - Enables verification that operations use move semantics
///
/// 3. TEMPLATED VALIDATORS (Generic Test Logic)
///    - Template functions that implement test logic once
///    - Three patterns: template template, concrete type, factory
///    - Each validator tests ONE behavior/feature
///
/// 4. THIN TEST CASES (Test Harness)
///    - FL_TEST_CASE with multiple FL_SUBCASE blocks
///    - Each SUBCASE instantiates the validator for one container type
///    - Clear relationship between validator and container coverage
///
/// HOW TO ADD A NEW TEST:
/// ======================
///
/// Step 1: Write a Templated Validator
/// ------------------------------------
/// Choose the pattern that fits your container(s):
///
/// PATTERN 1: Template Template Parameter
/// Use for: single-parameter containers (vector, deque, list)
///
///   template<template<typename> class ContainerTemplate>
///   void test_my_feature() {
///       using Container = ContainerTemplate<TestItem>;
///       Container c;
///       // Test logic here
///       FL_CHECK(c.my_method() == expected);
///   }
///
/// Usage: test_my_feature<vector>(), test_my_feature<deque>()
///
///
/// PATTERN 2: Concrete Container Type
/// Use for: multi-parameter containers or specific element types
///
///   template<typename Container>
///   void test_my_feature() {
///       Container c;
///       // Test logic here
///       FL_CHECK(c.capacity() >= c.size());
///   }
///
/// Usage: test_my_feature<vector<int>>(), test_my_feature<deque<int>>()
///
///
/// PATTERN 3: Container Factory
/// Use for: complex initialization or containers without template parameters
///
///   struct MyContainerFactory {
///       static my_container_type create(int a, int b, int c) {
///           auto c = my_container_type();
///           c.insert(a); c.insert(b); c.insert(c);
///           return c;
///       }
///   };
///
///   template<typename ContainerFactory>
///   void test_my_feature() {
///       auto c = ContainerFactory::create(1, 2, 3);
///       FL_CHECK(c.size() == 3);
///   }
///
/// Usage: test_my_feature<QueueIntFactory>()
///
///
/// Step 2: Create Test Case with FL_SUBCASEs
/// ------------------------------------------
/// Only add FL_SUBCASE for containers that SUPPORT the feature.
/// Comment containers that DON'T and WHY.
///
///   FL_TEST_CASE("My feature description") {
///       FL_SUBCASE("fl::vector") { test_my_feature<vector>(); }
///       FL_SUBCASE("fl::deque") { test_my_feature<deque>(); }
///       FL_SUBCASE("fl::list") { test_my_feature<list>(); }
///       // NOTE: fl::queue doesn't have operator[]
///   }
///
/// CONTAINER COVERAGE REFERENCE:
/// ==============================
///
/// DYNAMIC SEQUENCE CONTAINERS (all support push_back/pop_back/etc):
///   - vector, deque, list
///
/// VECTOR VARIANTS:
///   - vector_fixed_test (FixedVector with capacity 512)
///   - vector_inlined_test (InlinedVector with inline storage)
///
/// CONTAINERS WITH capacity()/reserve():
///   - vector, deque
///   - NOTE: list, vector_fixed, vector_inlined don't support capacity()
///
/// RANDOM-ACCESS ITERATORS (support arithmetic like it+1, end-begin):
///   - vector, deque
///   - NOTE: list only has bidirectional iterators
///
/// COMPARISON OPERATORS (==, !=, <, <=, >, >=):
///   - queue, set, circular_buffer, and most sequential containers
///   - NOTE: Some maps may not support comparison
///
/// ASSOCIATIVE CONTAINERS:
///   - map, unordered_map, set, unordered_set
///   - NOTE: Require factory pattern for complex initialization
///
/// ADAPTER CONTAINERS:
///   - queue, priority_queue
///   - NOTE: No random access, require factory pattern
///
/// VERIFYING MOVE SEMANTICS:
/// ==========================
/// TestItem tracks moves to prove containers use move semantics:
///
///   TestItem item(42);
///   int initial_move_count = item.move_count;
///
///   container.push_back(fl::move(item));
///
///   // Verify move happened
///   FL_CHECK(item.move_count > initial_move_count);
///   // Verify NO copies (copy constructor is deleted)
///   FL_CHECK(container.back().copy_count == 0);
///
/// BEST PRACTICES:
/// ===============
/// 1. Keep validators focused - test ONE behavior per function
/// 2. Test ALL containers that support the feature
/// 3. Comment containers that DON'T support it and WHY
/// 4. Use appropriate pattern: template template for single-param containers
/// 5. Test case names describe WHAT, not WHICH CONTAINERS
///    Good: "push_back - all container types"
///    Bad:  "push_back on vector"
/// 6. When containers have different interfaces, create wrapper classes
///    to normalize them (see unsorted_map_fixedNormalized example below)
/// 7. Use TestItem's move tracking to verify semantics, not just behavior

#include "test.h"
#include "test_container_helpers.h"
#include "fl/stl/vector.h"
#include "fl/stl/deque.h"
#include "fl/stl/list.h"
#include "fl/stl/queue.h"
#include "fl/stl/priority_queue.h"
#include "fl/stl/set.h"
#include "fl/stl/flat_set.h"
#include "fl/stl/unordered_set.h"
#include "fl/stl/map.h"
#include "fl/stl/unordered_map.h"
#include "fl/stl/multi_map.h"
#include "fl/stl/circular_buffer.h"
#include "fl/stl/shared_ptr.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

// ============================================================================
// COMPONENT 1: TYPE SETUP & NORMALIZATION
// ============================================================================
// Define container aliases and wrapper classes to normalize interfaces so
// tests can be generic. This layer allows one test to work across multiple
// container types by abstracting away their differences.
//
// EXAMPLE: unsorted_map_fixedNormalized wraps unsorted_map_fixed to match map's interface.
// Before: unsorted_map_fixed::insert(key, value)
// After:  unsorted_map_fixedNormalized::insert(pair<key, value>)
// Result: One test works for both map and unsorted_map_fixedNormalized
//
// ============================================================================

// Template wrappers for vector variants with default size parameters
template <typename T> using vector_fixed_test = FixedVector<T, 512>;
template <typename T> using vector_inlined_test = InlinedVector<T, 64>;
template <typename T> using circular_buffer_test = circular_buffer<T, 512>;

// Template wrapper for unsorted_map_fixed to normalize interface (provides pair-based insert)
template <typename Key, typename Value, fl::size N = 512>
class unsorted_map_fixedNormalized : public unsorted_map_fixed<Key, Value, N> {
  public:
    using Base = unsorted_map_fixed<Key, Value, N>;
    using iterator = typename Base::iterator;
    using const_iterator = typename Base::const_iterator;

    // Normalize insert to accept pair (like other maps)
    fl::pair<iterator, bool> insert(const fl::pair<Key, Value>& p) {
        auto result = Base::insert(p.first, p.second);
        return {result.second, result.first};
    }

    // Normalize count to use has()
    fl::size count(const Key& key) const {
        return Base::has(key) ? 1 : 0;
    }

    // Erase is now properly implemented in unsorted_map_fixed base class
    // This just calls the base implementation which actually removes the element
    using Base::erase;
};

// Template alias for normalized unsorted_map_fixed
template <typename Key, typename Value>
using fixed_map_test = unsorted_map_fixedNormalized<Key, Value, 512>;

// ============================================================================
// COMPONENT 2: TEST ITEM TYPE
// ============================================================================
// TestItem is the element type for all container tests. It tracks move/copy
// operations to verify that containers use move semantics correctly.
//
// KEY FEATURES:
// - move_count: Incremented on each move (proves move semantics)
// - copy_count: Would be set if copy happened (but copy is DELETED)
// - Deleted copy constructor: Compiler ERROR if any code tries to copy
//   This catches bugs immediately rather than silently copying.
//
// USAGE IN TESTS:
//   TestItem item(42);
//   int initial = item.move_count;
//   container.push_back(fl::move(item));
//   FL_CHECK(item.move_count > initial);      // Move happened
//   FL_CHECK(container.back().copy_count == 0); // No copies
//
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
// COMPONENT 3: TEMPLATED VALIDATORS
// ============================================================================
// Test functions that implement test logic generically. Each validator tests
// ONE behavior/feature and works across multiple container types.
//
// PATTERN USED HERE (Pattern 1 - Template Template Parameter):
// This section uses template template parameters: test_func<vector>()
// This decouples container type from element type.
//
// EXAMPLE:
//   template<template<typename> class ContainerTemplate>
//   void test_push_back() {
//       using Container = ContainerTemplate<TestItem>;
//       Container container;
//       container.push_back(fl::move(TestItem(42)));
//       FL_CHECK(container.size() == 1);
//   }
//
// USAGE:
//   test_push_back<vector>();           // Tests fl::vector<TestItem>
//   test_push_back<deque>();            // Tests fl::deque<TestItem>
//   test_push_back<vector_fixed_test>(); // Tests FixedVector<TestItem, 512>
//
// ADDING A NEW TEST:
// 1. Write template function with one test behavior
// 2. Add FL_SUBCASE in test case section (at bottom) for each container
// 3. Only add FL_SUBCASE for containers that SUPPORT the feature
// 4. Comment containers that DON'T support it and WHY
//
// ============================================================================

// Basic Operation Test Functions - Generic Container Template Pattern
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
// Map Container Test Functions (PATTERN 2: Concrete Container Type)
// ============================================================================
// Map containers require multi-parameter templates: map<Key, Value>
// So we use Pattern 2 with concrete types instead of template template params.
//
// EXAMPLE:
//   template<typename Map>
//   void test_map_insert_find() {
//       Map m;
//       m.insert(fl::make_pair(1, 100));
//       auto it = m.find(1);
//       FL_CHECK(it->second == 100);
//   }
//
// USAGE IN TEST CASE:
//   FL_SUBCASE("fl::map") { test_map_insert_find<map<int, int>>(); }
//   FL_SUBCASE("fl::unordered_map") { test_map_insert_find<unordered_map<int, int>>(); }
//
// ============================================================================

// Insert/Find operations for map containers
template<typename Map>
void test_map_insert_find() {
    Map m;
    m.insert(fl::make_pair(1, 100));
    m.insert(fl::make_pair(2, 200));
    m.insert(fl::make_pair(3, 300));

    FL_CHECK(m.size() == 3);

    // Find existing keys
    auto it = m.find(2);
    FL_CHECK(it != m.end());
    FL_CHECK(it->second == 200);

    // Find non-existing key
    auto it_not = m.find(99);
    FL_CHECK(it_not == m.end());
}

// Operator[] access for map containers
template<typename Map>
void test_map_operator_subscript() {
    Map m;
    m[10] = 1000;
    m[20] = 2000;
    m[30] = 3000;

    FL_CHECK(m.size() == 3);
    FL_CHECK(m[10] == 1000);
    FL_CHECK(m[20] == 2000);
    FL_CHECK(m[30] == 3000);
}

// Erase operations for map containers
template<typename Map>
void test_map_erase() {
    Map m;
    m.insert(fl::make_pair(1, 100));
    m.insert(fl::make_pair(2, 200));
    m.insert(fl::make_pair(3, 300));

    FL_CHECK(m.size() == 3);

    // Erase by key
    auto count = m.erase(2);
    FL_CHECK(count == 1);
    FL_CHECK(m.size() == 2);
    FL_CHECK(m.find(2) == m.end());
}

// Iteration for map containers
template<typename Map>
void test_map_iteration() {
    Map m;
    m.insert(fl::make_pair(1, 100));
    m.insert(fl::make_pair(2, 200));
    m.insert(fl::make_pair(3, 300));

    int sum = 0;
    for (auto it = m.begin(); it != m.end(); ++it) {
        sum += it->second;
    }

    FL_CHECK(sum == 600);
}

// Size and clear for map containers
template<typename Map>
void test_map_size_clear() {
    Map m;
    FL_CHECK(m.empty());
    FL_CHECK(m.size() == 0);

    m.insert(fl::make_pair(1, 100));
    m.insert(fl::make_pair(2, 200));
    FL_CHECK(!m.empty());
    FL_CHECK(m.size() == 2);

    m.clear();
    FL_CHECK(m.empty());
    FL_CHECK(m.size() == 0);
}

// Count method for map containers
template<typename Map>
void test_map_count() {
    Map m;
    m.insert(fl::make_pair(1, 100));
    m.insert(fl::make_pair(2, 200));
    m.insert(fl::make_pair(3, 300));

    // count returns 0 or 1 for unique keys in map
    FL_CHECK(m.count(1) > 0);
    FL_CHECK(m.count(2) > 0);
    FL_CHECK(m.count(99) == 0);
}

// Front and back element access for map containers
template<typename Map>
void test_map_front_back() {
    Map m;
    m.insert(fl::make_pair(1, 100));
    m.insert(fl::make_pair(2, 200));
    m.insert(fl::make_pair(3, 300));

    // For sorted maps (flat_map, map), elements are ordered by key
    // front() should give the element with smallest key
    // back() should give the element with largest key
    auto front_pair = m.front();
    auto back_pair = m.back();

    FL_CHECK(front_pair.first == 1);
    FL_CHECK(front_pair.second == 100);
    FL_CHECK(back_pair.first == 3);
    FL_CHECK(back_pair.second == 300);
}

// ============================================================================
// PATTERN 2: CONCRETE CONTAINER TYPE (Specific Element Types)
// ============================================================================
// When you need to test with specific element types (e.g., int), use concrete
// container types instead of template template parameters.
//
// EXAMPLE:
//   template<typename Container>
//   void test_iterator_arithmetic() {
//       Container c;
//       c.push_back(10);
//       auto it = c.begin();
//       it = it + 1;
//       FL_CHECK(*it == 20);
//   }
//
// USAGE:
//   test_iterator_arithmetic<vector<int>>();
//   test_iterator_arithmetic<deque<int>>();
//
// WHEN TO USE THIS PATTERN:
// - Testing with primitive types (int, float, etc.)
// - Multi-parameter containers (map<K,V>, etc.)
// - When element type must be explicit
//
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
void test_sequential_insert_erase() {
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
void test_capacity_management() {
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
void test_iterator_arithmetic() {
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
void test_iterator_comparison() {
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
// Erase While Iterating Test Functions
// ============================================================================

// Erase while iterating for sequential containers (Pattern 1)
template<template<typename> class ContainerTemplate>
void test_erase_while_iterating() {
    using Container = ContainerTemplate<int>;
    Container c;
    // Insert multiple elements
    for (int i = 1; i <= 5; ++i) {
        c.push_back(i);
    }

    FL_CHECK(c.size() == 5);

    // Erase every other element while iterating
    int erased_count = 0;
    for (auto it = c.begin(); it != c.end(); ) {
        if (*it % 2 == 0) {
            it = c.erase(it);  // erase returns next iterator
            erased_count++;
        } else {
            ++it;
        }
    }

    FL_CHECK(erased_count == 2);  // Elements 2 and 4 were erased
    FL_CHECK(c.size() == 3);      // Elements 1, 3, 5 remain

    // Verify remaining elements
    int count = 0;
    for (auto it = c.begin(); it != c.end(); ++it) {
        count++;
        FL_CHECK(*it % 2 == 1);  // All remaining should be odd
    }
    FL_CHECK(count == 3);
}

// Erase while iterating for map containers (Pattern 2)
template<typename Map>
void test_map_erase_while_iterating() {
    Map m;
    // Insert multiple elements
    for (int i = 1; i <= 5; ++i) {
        m.insert(fl::make_pair(i, i * 10));
    }

    FL_CHECK(m.size() == 5);

    // Erase every other element while iterating
    int erased_count = 0;
    for (auto it = m.begin(); it != m.end(); ) {
        if (it->first % 2 == 0) {
            it = m.erase(it);  // erase returns next iterator
            erased_count++;
        } else {
            ++it;
        }
    }

    FL_CHECK(erased_count == 2);  // Keys 2 and 4 were erased
    FL_CHECK(m.size() == 3);      // Keys 1, 3, 5 remain

    // Verify remaining elements
    FL_CHECK(m.count(1) > 0);
    FL_CHECK(m.count(3) > 0);
    FL_CHECK(m.count(5) > 0);
    FL_CHECK(m.count(2) == 0);
    FL_CHECK(m.count(4) == 0);

    // Verify values
    auto it = m.find(1);
    FL_CHECK(it != m.end());
    FL_CHECK(it->second == 10);
}

// ============================================================================
// PATTERN 3: CONTAINER FACTORY (Complex Initialization)
// ============================================================================
// For containers that don't expose constructors for testing (like queue, set),
// use factory classes that wrap the initialization logic.
//
// EXAMPLE:
//   struct QueueIntFactory {
//       static queue<int> create(int a, int b, int c) {
//           queue<int> q;
//           q.push(a);
//           q.push(b);
//           q.push(c);
//           return q;
//       }
//   };
//
//   template<typename ContainerFactory>
//   void test_operator_equals() {
//       auto c1 = ContainerFactory::create(1, 2, 3);
//       auto c2 = ContainerFactory::create(1, 2, 3);
//       FL_CHECK(c1 == c2);
//   }
//
// USAGE:
//   test_operator_equals<QueueIntFactory>();
//
// WHEN TO USE THIS PATTERN:
// - queue, priority_queue (no templated constructor)
// - set, unordered_set (require special initialization)
// - Any container needing complex setup before testing
//
// ============================================================================

// Container Factories for Comparison Operators
// ============================================================================

// Sequential Container Factories
struct VectorIntFactory {
    static vector<int> create(int a, int b, int c) {
        vector<int> v;
        v.push_back(a);
        v.push_back(b);
        v.push_back(c);
        return v;
    }
};

struct DequeIntFactory {
    static deque<int> create(int a, int b, int c) {
        deque<int> d;
        d.push_back(a);
        d.push_back(b);
        d.push_back(c);
        return d;
    }
};

struct ListIntFactory {
    static list<int> create(int a, int b, int c) {
        list<int> l;
        l.push_back(a);
        l.push_back(b);
        l.push_back(c);
        return l;
    }
};

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

// Priority Queue factory
struct PriorityQueueIntFactory {
    static fl::PriorityQueue<int> create(int a, int b, int c) {
        fl::PriorityQueue<int> pq;
        pq.push(a);
        pq.push(b);
        pq.push(c);
        return pq;
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

// Flat Set factory
struct FlatSetIntFactory {
    static flat_set<int> create(int a, int b, int c) {
        flat_set<int> s;
        s.insert(a);
        s.insert(b);
        s.insert(c);
        return s;
    }
};

// Unordered Set factory
struct UnorderedSetIntFactory {
    static unordered_set<int> create(int a, int b, int c) {
        unordered_set<int> us;
        us.insert(a);
        us.insert(b);
        us.insert(c);
        return us;
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

// Map Factories
struct MapIntFactory {
    static fl::map<int, int> create(int a, int b, int c) {
        fl::map<int, int> m;
        m.insert(fl::make_pair(1, a));
        m.insert(fl::make_pair(2, b));
        m.insert(fl::make_pair(3, c));
        return m;
    }
};

struct UnorderedMapIntFactory {
    static fl::unordered_map<int, int> create(int a, int b, int c) {
        fl::unordered_map<int, int> m;
        m.insert(fl::make_pair(1, a));
        m.insert(fl::make_pair(2, b));
        m.insert(fl::make_pair(3, c));
        return m;
    }
};

struct MultiMapIntFactory {
    static fl::multi_map<int, int> create(int a, int b, int c) {
        fl::multi_map<int, int> m;
        m.insert(fl::make_pair(1, a));
        m.insert(fl::make_pair(2, b));
        m.insert(fl::make_pair(3, c));
        return m;
    }
};

// ============================================================================
// COMPONENT 4: TEST CASES (Test Harness)
// ============================================================================
// FL_TEST_CASE blocks with FL_SUBCASE for each container type.
// Each SUBCASE instantiates a validator with one specific container.
//
// STRUCTURE:
// ----------
//   FL_TEST_CASE("feature description") {
//       FL_SUBCASE("container1") { validator<container1>(); }
//       FL_SUBCASE("container2") { validator<container2>(); }
//       // NOTE: Why certain containers are NOT tested here
//   }
//
// NAMING:
// -------
// - Test case name: Describes WHAT feature is tested
//   Good:  "push_back - all container types"
//   Bad:   "push_back on vector"
//
// - SUBCASE name: Identifies which container is being tested
//   Good:  "fl::vector"
//   Bad:   "test vector push_back"
//
// COVERAGE:
// ---------
// - Add FL_SUBCASE for EVERY container that supports the feature
// - Comment containers that DON'T support it and WHY
// - Use this reference:
//
//   PATTERN 1 (Template Template): vector, deque, list, vector_fixed, vector_inlined
//   PATTERN 2 (Concrete Type):
//     - All types: vector<int>, deque<int>, list<int>
//     - Maps: map<int,int>, unordered_map<int,int>, fixed_map_test<int,int>
//     - Random access only: vector<int>, deque<int> (not list)
//     - With capacity(): vector<int>, deque<int> (not list, fixed, inlined)
//   PATTERN 3 (Factory): QueueIntFactory, SetIntFactory, CircularBufferIntFactory
//
// EXAMPLE - Random Access Only:
//   FL_TEST_CASE("Iterator arithmetic contracts") {
//       FL_SUBCASE("fl::vector") { test_iterator_arithmetic<vector<int>>(); }
//       FL_SUBCASE("fl::deque") { test_iterator_arithmetic<deque<int>>(); }
//       // NOTE: fl::list is Bidirectional, does not support arithmetic
//   }
//
// EXAMPLE - With Capacity:
//   FL_TEST_CASE("Capacity management contracts") {
//       FL_SUBCASE("fl::vector") { test_capacity_management<vector<int>>(); }
//       FL_SUBCASE("fl::deque") { test_capacity_management<deque<int>>(); }
//       // NOTE: fl::list, vector_fixed, and vector_inlined don't support capacity()
//   }
//
// EXAMPLE - Factory Pattern:
//   FL_TEST_CASE("operator== and operator!= - all containers") {
//       FL_SUBCASE("fl::queue") { test_operator_equals<QueueIntFactory>(); }
//       FL_SUBCASE("fl::set") { test_operator_equals<SetIntFactory>(); }
//   }
//
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
    FL_SUBCASE("fl::vector") { test_sequential_insert_erase<vector<int>>(); }
    FL_SUBCASE("fl::vector_fixed") { test_sequential_insert_erase<vector_fixed_test<int>>(); }
    FL_SUBCASE("fl::vector_inlined") { test_sequential_insert_erase<vector_inlined_test<int>>(); }
    FL_SUBCASE("fl::deque") { test_sequential_insert_erase<deque<int>>(); }
    FL_SUBCASE("fl::list") { test_sequential_insert_erase<list<int>>(); }
}

// Capacity management contracts
FL_TEST_CASE("Capacity management contracts") {
    FL_SUBCASE("fl::vector") { test_capacity_management<vector<int>>(); }
    FL_SUBCASE("fl::deque") { test_capacity_management<deque<int>>(); }
    // NOTE: fl::list, vector_fixed, and vector_inlined do not support capacity/reserve
}

// Iterator arithmetic contracts (RandomAccess only)
FL_TEST_CASE("Iterator arithmetic contracts") {
    FL_SUBCASE("fl::vector") { test_iterator_arithmetic<vector<int>>(); }
    FL_SUBCASE("fl::vector_fixed") { test_iterator_arithmetic<vector_fixed_test<int>>(); }
    FL_SUBCASE("fl::vector_inlined") { test_iterator_arithmetic<vector_inlined_test<int>>(); }
    FL_SUBCASE("fl::deque") { test_iterator_arithmetic<deque<int>>(); }
    // NOTE: fl::list is Bidirectional, does not support arithmetic
}

FL_TEST_CASE("Iterator comparison contracts") {
    FL_SUBCASE("fl::vector") { test_iterator_comparison<vector<int>>(); }
    FL_SUBCASE("fl::vector_fixed") { test_iterator_comparison<vector_fixed_test<int>>(); }
    FL_SUBCASE("fl::vector_inlined") { test_iterator_comparison<vector_inlined_test<int>>(); }
    FL_SUBCASE("fl::deque") { test_iterator_comparison<deque<int>>(); }
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
    FL_SUBCASE("fl::flat_set") { test_operator_equals<FlatSetIntFactory>(); }
    FL_SUBCASE("fl::circular_buffer") { test_operator_equals<CircularBufferIntFactory>(); }
}

FL_TEST_CASE("operator< and operator<= - all containers with comparison") {
    FL_SUBCASE("fl::queue") { test_operator_less<QueueIntFactory>(); }
    FL_SUBCASE("fl::set") { test_operator_less<SetIntFactory>(); }
    FL_SUBCASE("fl::flat_set") { test_operator_less<FlatSetIntFactory>(); }
    FL_SUBCASE("fl::circular_buffer") { test_operator_less<CircularBufferIntFactory>(); }
}

FL_TEST_CASE("operator> and operator>= - all containers with comparison") {
    FL_SUBCASE("fl::queue") { test_operator_greater<QueueIntFactory>(); }
    FL_SUBCASE("fl::set") { test_operator_greater<SetIntFactory>(); }
    FL_SUBCASE("fl::flat_set") { test_operator_greater<FlatSetIntFactory>(); }
    FL_SUBCASE("fl::circular_buffer") { test_operator_greater<CircularBufferIntFactory>(); }
}

// ============================================================================
// Map Container Tests
// ============================================================================

FL_TEST_CASE("map insert and find operations") {
    FL_SUBCASE("fl::map") { test_map_insert_find<fl::map<int, int>>(); }
    FL_SUBCASE("fl::unordered_map") { test_map_insert_find<fl::unordered_map<int, int>>(); }
    FL_SUBCASE("fl::multi_map") { test_map_insert_find<fl::multi_map<int, int>>(); }
    FL_SUBCASE("fl::unsorted_map_fixed") { test_map_insert_find<fixed_map_test<int, int>>(); }
}

FL_TEST_CASE("map operator[] access") {
    FL_SUBCASE("fl::map") { test_map_operator_subscript<fl::map<int, int>>(); }
    FL_SUBCASE("fl::unordered_map") { test_map_operator_subscript<fl::unordered_map<int, int>>(); }
    FL_SUBCASE("fl::unsorted_map_fixed") { test_map_operator_subscript<fixed_map_test<int, int>>(); }
    // NOTE: fl::multi_map doesn't support operator[] (ambiguous with duplicate keys)
}

FL_TEST_CASE("map erase operations") {
    FL_SUBCASE("fl::map") { test_map_erase<fl::map<int, int>>(); }
    FL_SUBCASE("fl::unordered_map") { test_map_erase<fl::unordered_map<int, int>>(); }
    FL_SUBCASE("fl::multi_map") { test_map_erase<fl::multi_map<int, int>>(); }
    FL_SUBCASE("fl::unsorted_map_fixed") { test_map_erase<fixed_map_test<int, int>>(); }
}

FL_TEST_CASE("map iteration") {
    FL_SUBCASE("fl::map") { test_map_iteration<fl::map<int, int>>(); }
    FL_SUBCASE("fl::unordered_map") { test_map_iteration<fl::unordered_map<int, int>>(); }
    FL_SUBCASE("fl::multi_map") { test_map_iteration<fl::multi_map<int, int>>(); }
    FL_SUBCASE("fl::unsorted_map_fixed") { test_map_iteration<fixed_map_test<int, int>>(); }
}

FL_TEST_CASE("map size and clear") {
    FL_SUBCASE("fl::map") { test_map_size_clear<fl::map<int, int>>(); }
    FL_SUBCASE("fl::unordered_map") { test_map_size_clear<fl::unordered_map<int, int>>(); }
    FL_SUBCASE("fl::multi_map") { test_map_size_clear<fl::multi_map<int, int>>(); }
    FL_SUBCASE("fl::unsorted_map_fixed") { test_map_size_clear<fixed_map_test<int, int>>(); }
}

FL_TEST_CASE("map count operations") {
    FL_SUBCASE("fl::map") { test_map_count<fl::map<int, int>>(); }
    FL_SUBCASE("fl::unordered_map") { test_map_count<fl::unordered_map<int, int>>(); }
    FL_SUBCASE("fl::multi_map") { test_map_count<fl::multi_map<int, int>>(); }
    FL_SUBCASE("fl::unsorted_map_fixed") { test_map_count<fixed_map_test<int, int>>(); }
}

FL_TEST_CASE("map front and back access") {
    // Only flat_map (vector-based) supports front()/back()
    // Tree-based maps (map, unordered_map, multi_map) and unsorted_map_fixed do not support these
    FL_SUBCASE("fl::flat_map") { test_map_front_back<fl::flat_map<int, int>>(); }
}

// ============================================================================
// ADDITIONAL COMPARISON OPERATOR TESTS
// ============================================================================
// Tests for comparison operators (==, !=, <, <=, >, >=) across all container types
// NOTE: Only containers with implemented comparison operators are tested here.
//
// Supported containers:
//   - vector (==, !=)
//   - deque (==, !=, <, <=, >, >=)
//   - queue (all via adapter)
//   - set (all via tree)
//   - priority_queue (all implemented)
//   - circular_buffer (all implemented)
//
// Not supported:
//   - list (no comparison operators)
//   - map, unordered_map, multi_map (no comparison operators)
//   - unordered_set (unordered - no comparison operators)

// Comparison operators for vector (== and != only)
FL_TEST_CASE("operator== and operator!= - vector") {
    FL_SUBCASE("fl::vector") { test_operator_equals<VectorIntFactory>(); }
}

// Comparison operators for deque (all)
FL_TEST_CASE("operator== and operator!= - deque") {
    FL_SUBCASE("fl::deque") { test_operator_equals<DequeIntFactory>(); }
}

FL_TEST_CASE("operator< and operator<= - deque") {
    FL_SUBCASE("fl::deque") { test_operator_less<DequeIntFactory>(); }
}

FL_TEST_CASE("operator> and operator>= - deque") {
    FL_SUBCASE("fl::deque") { test_operator_greater<DequeIntFactory>(); }
}

// NOTE: priority_queue is ordered by priority, not by element position
// Ordering operators (<, >, <=, >=) don't have semantic meaning for priority_queue
// Only equality operators (==, !=) are tested if implemented

// ============================================================================
// ERASE WHILE ITERATING TESTS
// ============================================================================
// Tests for iterator safety when erasing elements during iteration.
// This is critical for containers that return valid iterators after erase.

// Erase while iterating for sequential containers
FL_TEST_CASE("erase while iterating - sequential containers") {
    FL_SUBCASE("fl::vector") { test_erase_while_iterating<vector>(); }
    FL_SUBCASE("fl::vector_fixed") { test_erase_while_iterating<vector_fixed_test>(); }
    FL_SUBCASE("fl::vector_inlined") { test_erase_while_iterating<vector_inlined_test>(); }
    FL_SUBCASE("fl::deque") { test_erase_while_iterating<deque>(); }
    FL_SUBCASE("fl::list") { test_erase_while_iterating<list>(); }
}

// Erase while iterating for map containers
FL_TEST_CASE("erase while iterating - map containers") {
    FL_SUBCASE("fl::map") { test_map_erase_while_iterating<fl::map<int, int>>(); }
    FL_SUBCASE("fl::unordered_map") { test_map_erase_while_iterating<fl::unordered_map<int, int>>(); }
    FL_SUBCASE("fl::multi_map") { test_map_erase_while_iterating<fl::multi_map<int, int>>(); }
}

} // FL_TEST_FILE
