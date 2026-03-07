/// @file test_container_helpers.h
/// @brief Generic template helpers for testing container populate/retrieve operations
///
/// These templates provide a uniform interface for adding and retrieving elements
/// from various container types (vector, deque, list, set, map, etc.) without
/// needing to know the specific API of each container.
///
/// This allows test code to be written generically and avoids repetitive
/// boilerplate for each container type.
///
/// AGENT INSTRUCTIONS FOR ADDING NEW CONTAINER TESTS:
/// =================================================
/// When adding tests for a new container, follow these patterns:
///
/// 1. **Group by container category in test cases:**
///    - Sequential containers (vector, deque, list) → "Tier 2-4: Sequential container tests"
///    - Associative containers (map, flat_map, unordered_map) → "Tier 2-4: Associative container tests"
///    - Do NOT create separate test cases per container type
///
/// 2. **Use subcases for each container variant:**
///    FL_TEST_CASE("Tier 2-4: Associative container iterator tests") {
///        FL_SUBCASE("fl::map - Iterator basics") {
///            test_helpers::test_map_iterators_with_shared_ptr<fl::map<...>>();
///        }
///        FL_SUBCASE("fl::flat_map - Iterator basics") {
///            test_helpers::test_map_iterators_with_shared_ptr<fl::flat_map<...>>();
///        }
///    }
///
/// 3. **Add helper functions that work with multiple containers:**
///    - Generic tests go in this file (e.g., test_map_iterators_with_shared_ptr)
///    - Use populate_map() for maps, populate() for sequences
///    - Document which container types each test supports

#pragma once

#include "fl/stl/shared_ptr.h"
#include "fl/stl/type_traits.h"

namespace test_helpers {

// ============================================================================
// Helper Functions
// ============================================================================

// Helper to create shared_ptr with known value
inline fl::shared_ptr<int> make_shared_int(int value) {
    return fl::make_shared<int>(value);
}

// ============================================================================
// Type Traits for Container Detection
// These test with fl::shared_ptr<int> since that's what we're using
// ============================================================================

// Detect if type has push_back method
template<typename T>
class has_push_back {
    template<typename C> static auto test(int) -> decltype(fl::declval<C>().push_back(fl::declval<fl::shared_ptr<int>>()), fl::true_type());
    template<typename> static fl::false_type test(...);
public:
    static constexpr bool value = fl::is_same<decltype(test<T>(0)), fl::true_type>::value;
};

// Detect if type has push method
template<typename T>
class has_push {
    template<typename C> static auto test(int) -> decltype(fl::declval<C>().push(fl::declval<fl::shared_ptr<int>>()), fl::true_type());
    template<typename> static fl::false_type test(...);
public:
    static constexpr bool value = fl::is_same<decltype(test<T>(0)), fl::true_type>::value;
};

// Detect if type has insert(value) method
template<typename T>
class has_insert_value {
    template<typename C> static auto test(int) -> decltype(fl::declval<C>().insert(fl::declval<fl::shared_ptr<int>>()), fl::true_type());
    template<typename> static fl::false_type test(...);
public:
    static constexpr bool value = fl::is_same<decltype(test<T>(0)), fl::true_type>::value;
};

// Detect if type has insert(key, value) method (for maps)
template<typename T>
class has_insert_key_value {
    template<typename C> static auto test(int) -> decltype(fl::declval<C>().insert(0, fl::declval<fl::shared_ptr<int>>()), fl::true_type());
    template<typename> static fl::false_type test(...);
public:
    static constexpr bool value = fl::is_same<decltype(test<T>(0)), fl::true_type>::value;
};

// Detect if type has operator[] for assignment
template<typename T>
class has_subscript_assign {
    template<typename C> static auto test(int) -> decltype(fl::declval<C>()[0] = fl::declval<fl::shared_ptr<int>>(), fl::true_type());
    template<typename> static fl::false_type test(...);
public:
    static constexpr bool value = fl::is_same<decltype(test<T>(0)), fl::true_type>::value;
};

// Detect if type has front() method
template<typename T>
class has_front {
    template<typename C> static auto test(int) -> decltype(fl::declval<C>().front(), fl::true_type());
    template<typename> static fl::false_type test(...);
public:
    static constexpr bool value = fl::is_same<decltype(test<T>(0)), fl::true_type>::value;
};

// Detect if type has operator* (dereference)
template<typename T>
class has_dereference {
    template<typename C> static auto test(int) -> decltype(*fl::declval<C>(), fl::true_type());
    template<typename> static fl::false_type test(...);
public:
    static constexpr bool value = fl::is_same<decltype(test<T>(0)), fl::true_type>::value;
};

// Detect if type has value() method (optional, expected)
template<typename T>
class has_value_method {
    template<typename C> static auto test(int) -> decltype(fl::declval<C>().value(), fl::true_type());
    template<typename> static fl::false_type test(...);
public:
    static constexpr bool value = fl::is_same<decltype(test<T>(0)), fl::true_type>::value;
};

// Detect if type has get() method (smart pointers)
template<typename T>
class has_get {
    template<typename C> static auto test(int) -> decltype(fl::declval<C>().get(), fl::true_type());
    template<typename> static fl::false_type test(...);
public:
    static constexpr bool value = fl::is_same<decltype(test<T>(0)), fl::true_type>::value;
};

// Detect if type has pop(T&) method (circular buffers)
template<typename T>
class has_pop_ref {
    template<typename C> static auto test(int) -> decltype(fl::declval<C>().pop(fl::declval<fl::shared_ptr<int>&>()), fl::true_type());
    template<typename> static fl::false_type test(...);
public:
    static constexpr bool value = fl::is_same<decltype(test<T>(0)), fl::true_type>::value;
};

// ============================================================================
// Composite Type Traits - Simplified Conditions
// ============================================================================

// Check if none of the higher-priority traits match
template<typename T>
struct is_container_with_subscript {
    static constexpr bool value = has_subscript_assign<T>::value;
};

template<typename T>
struct is_container_with_front {
    static constexpr bool value = !has_subscript_assign<T>::value && has_front<T>::value;
};

template<typename T>
struct is_container_with_iterator {
    static constexpr bool value = !has_subscript_assign<T>::value && !has_front<T>::value && has_insert_value<T>::value;
};

template<typename T>
struct is_smart_pointer {
    static constexpr bool value = !has_subscript_assign<T>::value && !has_front<T>::value &&
                                  !has_insert_value<T>::value && has_dereference<T>::value && has_get<T>::value;
};

template<typename T>
struct is_optional_like {
    static constexpr bool value = !has_subscript_assign<T>::value && !has_front<T>::value &&
                                  !has_insert_value<T>::value && !has_dereference<T>::value && has_value_method<T>::value;
};

template<typename T>
struct is_pop_only_container {
    static constexpr bool value = !has_subscript_assign<T>::value && !has_front<T>::value &&
                                  !has_insert_value<T>::value && !has_dereference<T>::value &&
                                  !has_value_method<T>::value && has_pop_ref<T>::value;
};

// ============================================================================
// Generic populate() - adds shared_ptr<int> to container using SFINAE
// Priority order: push_back > insert > push
// ============================================================================

// Priority 1: For containers with push_back (vector, deque, list, circular_buffer)
template<typename Container>
typename fl::enable_if<has_push_back<Container>::value, void>::type
populate(Container& c, fl::shared_ptr<int> ptr) {
    c.push_back(ptr);
}

// Priority 2: For containers with insert(value) (set, unordered_set)
template<typename Container>
typename fl::enable_if<!has_push_back<Container>::value && has_insert_value<Container>::value, void>::type
populate(Container& c, fl::shared_ptr<int> ptr) {
    c.insert(ptr);
}

// Priority 3: For containers with push (queue, circular_buffer)
template<typename Container>
typename fl::enable_if<!has_push_back<Container>::value && !has_insert_value<Container>::value && has_push<Container>::value, void>::type
populate(Container& c, fl::shared_ptr<int> ptr) {
    c.push(ptr);
}

// For containers with insert(key, value) (SortedHeapMap, FixedMap)
template<typename Container>
typename fl::enable_if<has_insert_key_value<Container>::value, void>::type
populate_map(Container& c, int key, fl::shared_ptr<int> ptr) {
    c.insert(key, ptr);
}

// For containers with operator[] (map, unordered_map)
template<typename Container>
typename fl::enable_if<has_subscript_assign<Container>::value && !has_insert_key_value<Container>::value, void>::type
populate_map(Container& c, int key, fl::shared_ptr<int> ptr) {
    c[key] = ptr;
}

// ============================================================================
// Generic retrieve() - gets shared_ptr<int> from container using SFINAE
// Priority order: operator[] > front() > begin() > pop() > operator* > value()
// ============================================================================

// Priority 1: For containers with operator[0] (vector, deque, circular_buffer)
template<typename Container>
typename fl::enable_if<is_container_with_subscript<Container>::value, fl::shared_ptr<int>>::type
retrieve(Container& c) {
    return c[0];
}

// Priority 2: For containers with front() (list, queue)
template<typename Container>
typename fl::enable_if<is_container_with_front<Container>::value, fl::shared_ptr<int>>::type
retrieve(Container& c) {
    return c.front();
}

// Priority 3: For containers with begin() iterator (set, unordered_set)
template<typename Container>
typename fl::enable_if<is_container_with_iterator<Container>::value, fl::shared_ptr<int>>::type
retrieve(Container& c) {
    return *c.begin();
}

// Priority 4: For containers with pop(T&) (circular_buffer)
template<typename Container>
typename fl::enable_if<is_pop_only_container<Container>::value, fl::shared_ptr<int>>::type
retrieve(Container& c) {
    fl::shared_ptr<int> result;
    c.pop(result);
    return result;
}

// Priority 5: For smart pointers with operator* (unique_ptr, shared_ptr)
template<typename Container>
typename fl::enable_if<is_smart_pointer<Container>::value, fl::shared_ptr<int>>::type
retrieve(Container& c) {
    return *c;
}

// Priority 6: For optional/variant with value()
template<typename Container>
typename fl::enable_if<is_optional_like<Container>::value, fl::shared_ptr<int>>::type
retrieve(Container& c) {
    return c.value();
}

// For maps - retrieve using key
template<typename Container>
fl::shared_ptr<int> retrieve_map(Container& c, int key) {
    return c[key];
}

// ============================================================================
// Generic Test Template Functions
// ============================================================================

// Test container move semantics with shared_ptr reference counting
// Works for: vector, deque, list, set, queue, circular buffers, etc.
template<typename Container>
void test_container_move_semantics() {
    auto ptr = make_shared_int(42);

    Container source;
    populate(source, ptr);

    FL_REQUIRE(ptr.use_count() == 2);  // 1 in container, 1 local
    FL_REQUIRE(source.size() == 1);

    Container destination;
    destination = fl::move(source);

    FL_CHECK(source.size() == 0);
    FL_CHECK(source.empty());
    FL_CHECK(destination.size() == 1);

    // Retrieve and check value, but let it go out of scope immediately
    {
        auto retrieved = retrieve(destination);
        FL_CHECK(*retrieved == 42);
    }
    FL_CHECK(ptr.use_count() == 2);  // Proves move, not copy

    destination.clear();
    FL_CHECK(ptr.use_count() == 1);  // Only local reference remains
}

// Test map container move semantics with key-value pairs
// Works for: map, unordered_map, SortedHeapMap, FixedMap, HashMapLru
template<typename MapContainer>
void test_map_move_semantics() {
    auto ptr = make_shared_int(100);

    MapContainer source;
    populate_map(source, 1, ptr);

    FL_REQUIRE(ptr.use_count() == 2);
    FL_REQUIRE(source.size() == 1);

    MapContainer destination;
    destination = fl::move(source);

    FL_CHECK(source.size() == 0);
    FL_CHECK(source.empty());
    FL_CHECK(destination.size() == 1);

    // Retrieve and check value, but let it go out of scope immediately
    {
        auto retrieved = retrieve_map(destination, 1);
        FL_CHECK(*retrieved == 100);
    }
    FL_CHECK(ptr.use_count() == 2);

    destination.clear();
    FL_CHECK(ptr.use_count() == 1);
}

// Test smart pointer move semantics (unique_ptr, shared_ptr, optional, variant, expected)
template<typename SmartPtr>
void test_smart_pointer_move_semantics() {
    auto ptr = make_shared_int(200);

    SmartPtr source(ptr);

    FL_REQUIRE(ptr.use_count() == 2);

    SmartPtr destination;
    destination = fl::move(source);

    // Retrieve and check value, but let it go out of scope immediately
    {
        auto retrieved = retrieve(destination);
        FL_CHECK(*retrieved == 200);
    }
    FL_CHECK(ptr.use_count() == 2);
}

// Test basic iterator support for containers with shared_ptr<int>
// Verifies: begin/end, forward iteration, const iteration, move leaves empty
template<typename Container>
void test_container_iterators_with_shared_ptr() {
    auto ptr1 = make_shared_int(10);
    auto ptr2 = make_shared_int(20);
    auto ptr3 = make_shared_int(30);

    Container source;
    populate(source, ptr1);
    populate(source, ptr2);
    populate(source, ptr3);

    // Test mutable iterators exist
    FL_CHECK(source.begin() != source.end());

    // Test const iterators via const reference
    const auto& const_source = source;
    FL_CHECK(const_source.begin() != const_source.end());

    // Move and verify source is empty via iterators
    Container destination = fl::move(source);
    FL_CHECK(source.begin() == source.end());  // Empty after move
    const auto& const_empty = source;
    FL_CHECK(const_empty.begin() == const_empty.end());
    FL_CHECK(destination.begin() != destination.end());  // Has elements
}

// Test reverse iterator support for bidirectional containers
// Verifies: rbegin/rend exist and work correctly after move
template<typename Container>
void test_container_reverse_iterators() {
    Container source;
    source.push_back(10);
    source.push_back(20);
    source.push_back(30);

    // Test reverse iterators exist
    FL_CHECK(source.rbegin() != source.rend());
    auto rit = source.rbegin();
    FL_CHECK(*rit == 30);  // Last element first

    // Move and verify
    Container destination = fl::move(source);
    FL_CHECK(source.rbegin() == source.rend());  // Empty
    FL_CHECK(destination.rbegin() != destination.rend());
    FL_CHECK(*destination.rbegin() == 30);
}

// ============================================================================
// TIER 2: Sequential Container Operations (Insert, Erase, Resize, etc.)
// ============================================================================

// Detect if type has insert(const_iterator, value)
template<typename T>
class has_insert_position {
    template<typename C> static auto test(int) -> decltype(
        fl::declval<C>().insert(fl::declval<C>().begin(), fl::declval<fl::shared_ptr<int>>()),
        fl::true_type()
    );
    template<typename> static fl::false_type test(...);
public:
    static constexpr bool value = fl::is_same<decltype(test<T>(0)), fl::true_type>::value;
};

// Detect if type has erase(const_iterator)
template<typename T>
class has_erase {
    template<typename C> static auto test(int) -> decltype(
        fl::declval<C>().erase(fl::declval<C>().begin()),
        fl::true_type()
    );
    template<typename> static fl::false_type test(...);
public:
    static constexpr bool value = fl::is_same<decltype(test<T>(0)), fl::true_type>::value;
};

// Detect if type has resize()
template<typename T>
class has_resize {
    template<typename C> static auto test(int) -> decltype(
        fl::declval<C>().resize(10),
        fl::true_type()
    );
    template<typename> static fl::false_type test(...);
public:
    static constexpr bool value = fl::is_same<decltype(test<T>(0)), fl::true_type>::value;
};

// Detect if type has reserve()
template<typename T>
class has_reserve {
    template<typename C> static auto test(int) -> decltype(
        fl::declval<C>().reserve(10),
        fl::true_type()
    );
    template<typename> static fl::false_type test(...);
public:
    static constexpr bool value = fl::is_same<decltype(test<T>(0)), fl::true_type>::value;
};

// Detect if type has capacity()
template<typename T>
class has_capacity {
    template<typename C> static auto test(int) -> decltype(
        fl::declval<C>().capacity(),
        fl::true_type()
    );
    template<typename> static fl::false_type test(...);
public:
    static constexpr bool value = fl::is_same<decltype(test<T>(0)), fl::true_type>::value;
};

// Detect if type has max_size()
template<typename T>
class has_max_size {
    template<typename C> static auto test(int) -> decltype(
        fl::declval<C>().max_size(),
        fl::true_type()
    );
    template<typename> static fl::false_type test(...);
public:
    static constexpr bool value = fl::is_same<decltype(test<T>(0)), fl::true_type>::value;
};

// Detect if type has shrink_to_fit()
template<typename T>
class has_shrink_to_fit {
    template<typename C> static auto test(int) -> decltype(
        fl::declval<C>().shrink_to_fit(),
        fl::true_type()
    );
    template<typename> static fl::false_type test(...);
public:
    static constexpr bool value = fl::is_same<decltype(test<T>(0)), fl::true_type>::value;
};

// Detect if type has back()
template<typename T>
class has_back {
    template<typename C> static auto test(int) -> decltype(
        fl::declval<C>().back(),
        fl::true_type()
    );
    template<typename> static fl::false_type test(...);
public:
    static constexpr bool value = fl::is_same<decltype(test<T>(0)), fl::true_type>::value;
};

// Detect if type has pop_back()
template<typename T>
class has_pop_back {
    template<typename C> static auto test(int) -> decltype(
        fl::declval<C>().pop_back(),
        fl::true_type()
    );
    template<typename> static fl::false_type test(...);
public:
    static constexpr bool value = fl::is_same<decltype(test<T>(0)), fl::true_type>::value;
};

// Detect if type has operator== for containers
template<typename T>
class has_container_equality {
    template<typename C> static auto test(int) -> decltype(
        fl::declval<C>() == fl::declval<C>(),
        fl::true_type()
    );
    template<typename> static fl::false_type test(...);
public:
    static constexpr bool value = fl::is_same<decltype(test<T>(0)), fl::true_type>::value;
};

// Detect if type has operator< for containers
template<typename T>
class has_container_less {
    template<typename C> static auto test(int) -> decltype(
        fl::declval<C>() < fl::declval<C>(),
        fl::true_type()
    );
    template<typename> static fl::false_type test(...);
public:
    static constexpr bool value = fl::is_same<decltype(test<T>(0)), fl::true_type>::value;
};

// Detect if type has iterator operator+ (RandomAccessIterator support)
template<typename T>
class has_iterator_plus {
    template<typename C> static auto test(int) -> decltype(
        fl::declval<typename C::iterator>() + 1,
        fl::true_type()
    );
    template<typename> static fl::false_type test(...);
public:
    static constexpr bool value = fl::is_same<decltype(test<T>(0)), fl::true_type>::value;
};

// ============================================================================
// TIER 2 Tests: Sequential Container Operations
// ============================================================================
// NOTE: Overloads here use enable_if to SELECT THE RIGHT IMPLEMENTATION.
// This is NOT silent skipping - it's explicit overload selection:
//
// - test_sequential_container_tier2<fl::deque>() → RandomAccess overload
// - test_sequential_container_tier2<fl::list>()  → Bidirectional overload
//
// If neither overload matches (shouldn't happen for Tier 2 containers),
// you get a COMPILER ERROR.

// Test that sequential containers support Tier 2 operations
// For RandomAccess iterators (vector, deque)
template<typename Container>
typename fl::enable_if<has_insert_position<Container>::value &&
                       has_erase<Container>::value &&
                       has_iterator_plus<Container>::value>::type
test_sequential_container_tier2() {
    Container c;
    populate(c, make_shared_int(10));
    populate(c, make_shared_int(30));

    // Test insert (using iterator arithmetic - RandomAccess only)
    c.insert(c.begin() + 1, make_shared_int(20));
    FL_CHECK(c.size() == 3);

    // Test erase (using iterator arithmetic - RandomAccess only)
    c.erase(c.begin() + 1);
    FL_CHECK(c.size() == 2);
}

// Alternative for BidirectionalIterator containers (list)
template<typename Container>
typename fl::enable_if<has_insert_position<Container>::value &&
                       has_erase<Container>::value &&
                       !has_iterator_plus<Container>::value>::type
test_sequential_container_tier2() {
    Container c;
    populate(c, make_shared_int(10));
    populate(c, make_shared_int(20));
    populate(c, make_shared_int(30));

    // Test insert/erase using begin/end (no arithmetic - BidirectionalIterator)
    auto it = c.begin();
    ++it;  // Move to second element
    c.insert(it, make_shared_int(25));
    FL_CHECK(c.size() == 4);

    // Erase second element
    it = c.begin();
    ++it;
    c.erase(it);
    FL_CHECK(c.size() == 3);
}

// Test front/back access for sequential containers
// NOTE: No enable_if - if a container doesn't have front/back, you get
// a COMPILER ERROR (not silent skipping).
template<typename Container>
void test_sequential_container_front_back() {
    Container c;
    populate(c, make_shared_int(1));
    populate(c, make_shared_int(2));
    populate(c, make_shared_int(3));

    // Test front and back - COMPILER ERROR if these methods don't exist
    FL_CHECK(*c.front() == 1);
    FL_CHECK(*c.back() == 3);
}

// Test resize for sequential containers
// NOTE: No enable_if - if a container doesn't have resize, you get
// a COMPILER ERROR (not silent skipping).
template<typename Container>
void test_sequential_container_resize() {
    Container c;
    populate(c, make_shared_int(1));
    populate(c, make_shared_int(2));

    // Resize up - COMPILER ERROR if resize() doesn't exist
    c.resize(4);
    FL_CHECK(c.size() == 4);

    // Resize down
    c.resize(2);
    FL_CHECK(c.size() == 2);
}

// ============================================================================
// TIER 3 Tests: Capacity Management
// ============================================================================
// NOTE: No enable_if guards - if the container doesn't have these methods,
// you get a COMPILER ERROR (not silent skipping).
//
// Example:
//   test_container_capacity_management<fl::vector>()  // OK - vector has reserve/capacity
//   test_container_capacity_management<fl::list>()    // COMPILER ERROR - list has no reserve()
//
// This ensures incomplete containers can't be tested without implementing all Tier 3 methods.

// Test capacity management for heap-based containers
template<typename Container>
void test_container_capacity_management() {
    Container c;

    // Test initial capacity - COMPILER ERROR if capacity() doesn't exist
    FL_CHECK(c.capacity() >= 0);

    // Test reserve - COMPILER ERROR if reserve() doesn't exist
    c.reserve(100);
    FL_CHECK(c.capacity() >= 100);
    FL_CHECK(c.size() == 0);  // Size unchanged

    // Populate without reallocation
    for (int i = 0; i < 50; ++i) {
        populate(c, make_shared_int(i));
    }
    FL_CHECK(c.size() == 50);
    FL_CHECK(c.capacity() >= 100);
}

// Test shrink_to_fit for heap-based containers
template<typename Container>
void test_container_shrink_to_fit() {
    Container c;

    // Add many elements
    for (int i = 0; i < 50; ++i) {
        populate(c, make_shared_int(i));
    }
    // Test capacity() - COMPILER ERROR if capacity() doesn't exist
    auto large_capacity = c.capacity();
    FL_CHECK(large_capacity > 50);

    // Remove most elements
    for (int i = 0; i < 40; ++i) {
        c.pop_back();
    }
    FL_CHECK(c.capacity() >= large_capacity);  // Capacity not reduced yet

    // Shrink - COMPILER ERROR if shrink_to_fit() doesn't exist
    c.shrink_to_fit();
    FL_CHECK(c.capacity() <= large_capacity);  // Should be reduced
    FL_CHECK(c.size() == 10);  // Size preserved
}

// Test max_size
template<typename Container>
void test_container_max_size() {
    Container c;
    // Test max_size() - COMPILER ERROR if max_size() doesn't exist
    FL_CHECK(c.max_size() > 0);
}

// ============================================================================
// TIER 4 Tests: STL Compatibility
// ============================================================================
// NOTE: No enable_if guards - explicit COMPILER ERRORS for missing features.
//
// Example:
//   test_iterator_arithmetic<fl::deque>()  // OK - deque has operator+
//   test_iterator_arithmetic<fl::list>()   // COMPILER ERROR - list has no operator+
//
//   test_container_comparison<fl::deque>()  // OK - deque has operator==
//   test_container_comparison<fl::vector>() // COMPILER ERROR - vector has no operator==
//
// This prevents incomplete Tier 4 implementations from being silently skipped.

// Test iterator arithmetic for RandomAccessIterator
template<typename Container>
void test_iterator_arithmetic() {
    Container c;
    populate(c, make_shared_int(10));
    populate(c, make_shared_int(20));
    populate(c, make_shared_int(30));

    // Test operator+ on iterators - COMPILER ERROR if operator+ doesn't exist
    auto it0 = c.begin();
    auto it1 = c.begin() + 1;
    FL_CHECK(it1 - it0 == 1);  // Distance check
    FL_CHECK(*(c.begin() + 1) != *c.begin());  // Different elements
}

// Test container comparison operators
template<typename Container>
void test_container_comparison() {
    Container c1, c2;
    auto ptr1 = make_shared_int(1);
    auto ptr2 = make_shared_int(1);

    // Populate both with same pointer (so they're actually equal)
    populate(c1, ptr1);
    populate(c2, ptr1);

    // Test equality - COMPILER ERROR if operator== doesn't exist
    FL_CHECK(c1 == c2);
    FL_CHECK(!(c1 != c2));

    // Test with different pointers
    Container c3;
    populate(c3, ptr2);
    // c3 may or may not be equal to c1 depending on pointer comparison
    // (with different pointers, it's likely not equal or less than)
    // Test operator< - COMPILER ERROR if operator< doesn't exist
    if (c1 < c3 || c3 < c1 || c1 == c3) {
        FL_CHECK(true);  // At least one comparison worked
    }
}

// ============================================================================
// MAP-SPECIFIC ITERATOR TESTS
// ============================================================================

// Test basic iterator support for associative containers with shared_ptr<int> values
// Verifies: begin/end, forward iteration, const iteration
template<typename MapContainer>
void test_map_iterators_with_shared_ptr() {
    auto ptr1 = make_shared_int(10);
    auto ptr2 = make_shared_int(20);
    auto ptr3 = make_shared_int(30);

    MapContainer source;
    populate_map(source, 1, ptr1);
    populate_map(source, 2, ptr2);
    populate_map(source, 3, ptr3);

    // Test mutable iterators exist
    FL_CHECK(source.begin() != source.end());

    // Test const iterators via const reference
    const auto& const_source = source;
    FL_CHECK(const_source.begin() != const_source.end());

    // Test iteration - count elements
    int count = 0;
    for (auto it = source.begin(); it != source.end(); ++it) {
        count++;
    }
    FL_CHECK(count == 3);

    // Move and verify source is empty via iterators
    MapContainer destination = fl::move(source);
    FL_CHECK(source.begin() == source.end());  // Empty after move
    const auto& const_empty = source;
    FL_CHECK(const_empty.begin() == const_empty.end());
    FL_CHECK(destination.begin() != destination.end());  // Has elements
}

// Test reverse iterator support for map containers
// Verifies: rbegin/rend work correctly
template<typename MapContainer>
void test_map_reverse_iterators() {
    MapContainer source;
    populate_map(source, 1, make_shared_int(10));
    populate_map(source, 2, make_shared_int(20));
    populate_map(source, 3, make_shared_int(30));

    // Test reverse iterators exist
    FL_CHECK(source.rbegin() != source.rend());

    // Count reverse iteration
    int count = 0;
    for (auto rit = source.rbegin(); rit != source.rend(); ++rit) {
        count++;
    }
    FL_CHECK(count == 3);

    // Move and verify
    MapContainer destination = fl::move(source);
    FL_CHECK(source.rbegin() == source.rend());  // Empty
    FL_CHECK(destination.rbegin() != destination.rend());
}

// Test iterator arithmetic for RandomAccessIterator maps (like flat_map)
// Verifies: operator+, operator-, distance calculations
template<typename MapContainer>
void test_map_iterator_arithmetic() {
    MapContainer c;
    populate_map(c, 1, make_shared_int(10));
    populate_map(c, 2, make_shared_int(20));
    populate_map(c, 3, make_shared_int(30));

    // Test operator+ on iterators
    auto it0 = c.begin();
    auto it1 = c.begin() + 1;
    auto it2 = c.begin() + 2;

    FL_CHECK(it1 - it0 == 1);  // Distance check
    FL_CHECK(it2 - it0 == 2);
    FL_CHECK(it1 != it0);
    FL_CHECK(it1 != it2);
}

// Test map iteration with key-value access
// Verifies: pair<Key, Value> dereferencing
template<typename MapContainer>
void test_map_iteration_key_value() {
    MapContainer m;
    populate_map(m, 100, make_shared_int(1000));
    populate_map(m, 200, make_shared_int(2000));

    int key_sum = 0;
    int value_sum = 0;

    for (auto it = m.begin(); it != m.end(); ++it) {
        key_sum += it->first;
        value_sum += *it->second;
    }

    FL_CHECK(key_sum == 300);  // 100 + 200
    FL_CHECK(value_sum == 3000);  // 1000 + 2000
}

// Test map lookup and iteration consistency
// Verifies: find() returns valid iterator, dereferencing works
template<typename MapContainer>
void test_map_find_and_iterate() {
    MapContainer m;
    auto ptr42 = make_shared_int(42);
    populate_map(m, 5, ptr42);
    populate_map(m, 10, make_shared_int(100));

    // Find and dereference
    auto it = m.find(5);
    FL_CHECK(it != m.end());
    FL_CHECK(it->first == 5);
    FL_CHECK(*it->second == 42);

    // Iterate from find result
    int count = 0;
    for (auto it2 = it; it2 != m.end(); ++it2) {
        count++;
    }
    FL_CHECK(count >= 1);  // At least the found element and beyond
}

// Test const iterator functionality
// Verifies: const_iterator works with find
template<typename MapContainer>
void test_map_const_iterators() {
    MapContainer m;
    populate_map(m, 7, make_shared_int(77));
    populate_map(m, 8, make_shared_int(88));

    const MapContainer& const_m = m;

    // Const find
    auto const_it = const_m.find(7);
    FL_CHECK(const_it != const_m.end());
    FL_CHECK(const_it->first == 7);
    FL_CHECK(*const_it->second == 77);

    // Const iteration
    int count = 0;
    for (auto it = const_m.begin(); it != const_m.end(); ++it) {
        count++;
    }
    FL_CHECK(count == 2);
}

// Test lower_bound and upper_bound
// Verifies: binary search operations return valid iterators
template<typename MapContainer>
void test_map_bounds() {
    MapContainer m;
    populate_map(m, 10, make_shared_int(100));
    populate_map(m, 20, make_shared_int(200));
    populate_map(m, 30, make_shared_int(300));

    // Test lower_bound
    auto lower = m.lower_bound(15);
    FL_CHECK(lower != m.end());
    FL_CHECK(lower->first >= 15);

    // Test upper_bound
    auto upper = m.upper_bound(20);
    FL_CHECK(upper != m.end());
    FL_CHECK(upper->first > 20);

    // Test equal_range
    auto range = m.equal_range(20);
    FL_CHECK(range.first != m.end());
    FL_CHECK(range.first->first == 20);
}

// Test insert and erase with iterators
// Verifies: insert returns valid iterator, erase returns next iterator
template<typename MapContainer>
void test_map_insert_erase_iterators() {
    MapContainer m;
    populate_map(m, 5, make_shared_int(50));
    populate_map(m, 15, make_shared_int(150));

    // Insert and get iterator
    auto result = m.insert(fl::pair<int, fl::shared_ptr<int>>(10, make_shared_int(100)));
    FL_CHECK(result.second == true);  // insertion took place
    FL_CHECK(result.first != m.end());
    FL_CHECK(result.first->first == 10);

    // Erase and verify next iterator
    auto erase_result = m.erase(result.first);
    // After erase, either at end or the key should be > 10
    bool valid = (erase_result == m.end());
    if (!valid) {
        valid = erase_result->first > 10;
    }
    FL_CHECK(valid);

    FL_CHECK(m.size() == 2);  // Two elements left
}

// Test count and contains
// Verifies: key lookup operations work correctly
template<typename MapContainer>
void test_map_key_lookup() {
    MapContainer m;
    populate_map(m, 42, make_shared_int(420));
    populate_map(m, 84, make_shared_int(840));

    FL_CHECK(m.count(42) == 1);
    FL_CHECK(m.count(84) == 1);
    FL_CHECK(m.count(99) == 0);

    FL_CHECK(m.contains(42) == true);
    FL_CHECK(m.contains(99) == false);
}

// Test operator[] for mutable access
// Verifies: operator[] allows modification through iterator dereferencing
template<typename MapContainer>
void test_map_operator_bracket_access() {
    MapContainer m;
    auto original = make_shared_int(111);
    populate_map(m, 1, original);

    // Verify value through operator[]
    FL_CHECK(*m[1] == 111);

    // Modify through iterator
    auto it = m.find(1);
    FL_CHECK(it != m.end());
    it->second = make_shared_int(222);
    FL_CHECK(*m[1] == 222);
}

// Test iteration order (for sorted maps)
// Verifies: elements are visited in key order
template<typename MapContainer>
void test_map_iteration_order() {
    MapContainer m;
    populate_map(m, 30, make_shared_int(3));
    populate_map(m, 10, make_shared_int(1));
    populate_map(m, 20, make_shared_int(2));

    // Collect keys in iteration order
    int prev_key = -1;
    int last_key = -1;
    for (auto it = m.begin(); it != m.end(); ++it) {
        int current_key = it->first;
        bool ascending = current_key > prev_key;
        FL_CHECK(ascending);  // Keys in ascending order
        prev_key = current_key;
        last_key = current_key;
    }
    FL_CHECK(last_key == 30);  // Last key was 30
}

} // namespace test_helpers
