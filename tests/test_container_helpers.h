/// @file test_container_helpers.h
/// @brief Generic template helpers for testing container populate/retrieve operations
///
/// These templates provide a uniform interface for adding and retrieving elements
/// from various container types (vector, deque, list, set, map, etc.) without
/// needing to know the specific API of each container.
///
/// This allows test code to be written generically and avoids repetitive
/// boilerplate for each container type.

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

// Priority 1: For containers with push_back (vector, deque, list, DynamicCircularBuffer)
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

// Priority 3: For containers with push (queue, StaticCircularBuffer)
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

// Priority 1: For containers with operator[0] (vector, deque, DynamicCircularBuffer)
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

// Priority 4: For containers with pop(T&) (StaticCircularBuffer)
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

} // namespace test_helpers
