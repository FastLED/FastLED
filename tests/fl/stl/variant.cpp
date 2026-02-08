// g++ --std=c++11 test.cpp

#include "fl/stl/variant.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/function.h"
#include "fl/stl/new.h"
#include "test.h"
#include "fl/hash.h"
#include "fl/log.h"
#include "fl/stl/move.h"
#include "fl/stl/string.h"
#include "fl/stl/unordered_map.h"


// Test object that tracks construction/destruction for move semantics testing
struct TrackedObject {
    static int construction_count;
    static int destruction_count;
    static int move_construction_count;
    static int copy_construction_count;
    
    int value;
    bool moved_from;
    
    TrackedObject(int v = 0) : value(v), moved_from(false) {
        construction_count++;
    }
    
    TrackedObject(const TrackedObject& other) : value(other.value), moved_from(false) {
        copy_construction_count++;
    }
    
    TrackedObject(TrackedObject&& other) noexcept : value(other.value), moved_from(false) {
        other.moved_from = true;
        move_construction_count++;
    }
    
    TrackedObject& operator=(const TrackedObject& other) {
        if (this != &other) {
            value = other.value;
            moved_from = false;
        }
        return *this;
    }
    
    TrackedObject& operator=(TrackedObject&& other) noexcept {
        if (this != &other) {
            value = other.value;
            moved_from = false;
            other.moved_from = true;
        }
        return *this;
    }
    
    ~TrackedObject() {
        destruction_count++;
    }
    
    static void reset_counters() {
        construction_count = 0;
        destruction_count = 0;
        move_construction_count = 0;
        copy_construction_count = 0;
    }
};

// Static member definitions
int TrackedObject::construction_count = 0;
int TrackedObject::destruction_count = 0;
int TrackedObject::move_construction_count = 0;
int TrackedObject::copy_construction_count = 0;

FL_TEST_CASE("Variant move semantics and RAII") {
    // Test the core issue: moved-from variants should be empty and not destroy moved-from objects
    TrackedObject::reset_counters();
    
    // Test 1: Verify moved-from variant is empty
    {
        fl::variant<int, TrackedObject> source(TrackedObject(42));
        FL_CHECK(source.is<TrackedObject>());
        
        // Move construct - this is where the bug was
        fl::variant<int, TrackedObject> destination(fl::move(source));
        
        // Critical test: source should be empty after move
        FL_CHECK(source.empty());
        FL_CHECK(!source.is<TrackedObject>());
        FL_CHECK(!source.is<int>());
        
        // destination should have the object
        FL_CHECK(destination.is<TrackedObject>());
        FL_CHECK_EQ(destination.ptr<TrackedObject>()->value, 42);
    }
    
    TrackedObject::reset_counters();
    
    // Test 2: Verify moved-from variant via assignment is empty
    {
        fl::variant<int, TrackedObject> source(TrackedObject(100));
        fl::variant<int, TrackedObject> destination;
        
        FL_CHECK(source.is<TrackedObject>());
        FL_CHECK(destination.empty());
        
        // Move assign - this is where the bug was
        destination = fl::move(source);
        
        // Critical test: source should be empty after move
        FL_CHECK(source.empty());
        FL_CHECK(!source.is<TrackedObject>());
        FL_CHECK(!source.is<int>());
        
        // destination should have the object
        FL_CHECK(destination.is<TrackedObject>());
        FL_CHECK_EQ(destination.ptr<TrackedObject>()->value, 100);
    }
    
    TrackedObject::reset_counters();
    
    // Test 3: Simulate the original fetch callback scenario
    // The key issue was that function objects containing shared_ptr were being destroyed
    // after being moved, causing use-after-free in the shared_ptr reference counting
    {
        using MockCallback = fl::function<void()>;
        auto shared_resource = fl::make_shared<TrackedObject>(999);
        
        // Create callback that captures shared_ptr (like fetch callbacks did)
        MockCallback callback = [shared_resource]() {
            // Use the resource
            FL_WARN("Using resource with value: " << shared_resource->value);
        };
        
        // Store in variant (like WasmFetchCallbackManager did)
        fl::variant<int, MockCallback> callback_variant(fl::move(callback));
        FL_CHECK(callback_variant.is<MockCallback>());
        
        // Extract via move (like takeCallback did) - this was causing heap-use-after-free
        fl::variant<int, MockCallback> extracted_callback(fl::move(callback_variant));
        
        // Original variant should be empty - this is the key fix
        FL_CHECK(callback_variant.empty());
        FL_CHECK(!callback_variant.is<MockCallback>());
        
        // Extracted callback should work and shared_ptr should be valid
        FL_CHECK(extracted_callback.is<MockCallback>());
        FL_CHECK_EQ(shared_resource.use_count(), 2); // One in extracted callback, one local
        
        // Call the extracted callback - should not crash
        if (auto* cb = extracted_callback.ptr<MockCallback>()) {
            (*cb)();
        }
        
        // Shared resource should still be valid
        FL_CHECK_EQ(shared_resource.use_count(), 2);
    }
}

FL_TEST_CASE("HashMap iterator-based erase") {
    fl::hash_map<int, fl::string> map;
    
    // Fill the map with some data
    map[1] = "one";
    map[2] = "two";
    map[3] = "three";
    map[4] = "four";
    map[5] = "five";
    
    FL_CHECK_EQ(map.size(), 5);
    
    // Test iterator-based erase
    auto it = map.find(3);
    FL_CHECK(it != map.end());
    FL_CHECK_EQ(it->second, "three");
    
    // Erase using iterator - should return iterator to next element
    auto next_it = map.erase(it);
    FL_CHECK_EQ(map.size(), 4);
    FL_CHECK(map.find(3) == map.end()); // Element should be gone
    
    // Verify all other elements are still there
    FL_CHECK(map.find(1) != map.end());
    FL_CHECK(map.find(2) != map.end());
    FL_CHECK(map.find(4) != map.end());
    FL_CHECK(map.find(5) != map.end());
    
    // Test erasing at end
    auto end_it = map.find(999); // Non-existent key
    FL_CHECK(end_it == map.end());
    auto result_it = map.erase(end_it); // Should handle gracefully
    FL_CHECK(result_it == map.end());
    FL_CHECK_EQ(map.size(), 4); // Size should be unchanged
    
    // Test erasing all remaining elements using iterators
    while (!map.empty()) {
        auto first = map.begin();
        map.erase(first);
    }
    FL_CHECK_EQ(map.size(), 0);
    FL_CHECK(map.empty());
}

// Test the original test cases
FL_TEST_CASE("Variant tests") {
    // 1) Default is empty
    fl::variant<int, fl::string> v;
    FL_REQUIRE(v.empty());
    FL_REQUIRE(!v.is<int>());
    FL_REQUIRE(!v.is<fl::string>());

    // 2) Emplace a T
    v = 123;
    FL_REQUIRE(v.is<int>());
    FL_REQUIRE(!v.is<fl::string>());
    FL_REQUIRE_EQ(*v.ptr<int>(), 123);

    // 3) Reset back to empty
    v.reset();
    FL_REQUIRE(v.empty());


    // 4) Emplace a U
    v = fl::string("hello");
    FL_REQUIRE(v.is<fl::string>());
    FL_REQUIRE(!v.is<int>());
    FL_REQUIRE(v.equals(fl::string("hello")));


    // 5) Copy‐construct preserves the U
    fl::variant<int, fl::string> v2(v);
    FL_REQUIRE(v2.is<fl::string>());

    fl::string* str_ptr = v2.ptr<fl::string>();
    FL_REQUIRE_NE(str_ptr, nullptr);
    FL_REQUIRE_EQ(*str_ptr, fl::string("hello"));
    const bool is_str = v2.is<fl::string>();
    const bool is_int = v2.is<int>();

    FL_CHECK(is_str);
    FL_CHECK(!is_int);

#if 0
    // 6) Move‐construct leaves source empty
    fl::variant<int, fl::string> v3(fl::move(v2));
    FL_REQUIRE(v3.is<fl::string>());
    FL_REQUIRE_EQ(v3.getU(), fl::string("hello"));
    FL_REQUIRE(v2.isEmpty());

    // 7) Copy‐assign
    fl::variant<int, fl::string> v4;
    v4 = v3;
    FL_REQUIRE(v4.is<fl::string>());
    FL_REQUIRE_EQ(v4.getU(), fl::string("hello"));


    // 8) Swap two variants
    v4.emplaceT(7);
    v3.swap(v4);
    FL_REQUIRE(v3.is<int>());
    FL_REQUIRE_EQ(v3.getT(), 7);
    FL_REQUIRE(v4.is<fl::string>());
    FL_REQUIRE_EQ(v4.getU(), fl::string("hello"));
#endif
}

FL_TEST_CASE("Variant") {
    // 1) Default is empty
    fl::variant<int, fl::string, double> v;
    FL_REQUIRE(v.empty());
    FL_REQUIRE(!v.is<int>());
    FL_REQUIRE(!v.is<fl::string>());
    FL_REQUIRE(!v.is<double>());

    // 2) Construct with a value
    fl::variant<int, fl::string, double> v1(123);
    FL_REQUIRE(v1.is<int>());
    FL_REQUIRE(!v1.is<fl::string>());
    FL_REQUIRE(!v1.is<double>());
    FL_REQUIRE_EQ(*v1.ptr<int>(), 123);

    // 3) Construct with a different type
    fl::variant<int, fl::string, double> v2(fl::string("hello"));
    FL_REQUIRE(!v2.is<int>());
    FL_REQUIRE(v2.is<fl::string>());
    FL_REQUIRE(!v2.is<double>());
    FL_REQUIRE_EQ(*v2.ptr<fl::string>(), fl::string("hello"));

    // 4) Copy construction
    fl::variant<int, fl::string, double> v3(v2);
    FL_REQUIRE(v3.is<fl::string>());
    FL_REQUIRE(v3.equals(fl::string("hello")));

    // 5) Assignment
    v = v1;
    FL_REQUIRE(v.is<int>());
    FL_REQUIRE_EQ(*v.ptr<int>(), 123);

    // 6) Reset
    v.reset();
    FL_REQUIRE(v.empty());

    // 7) Assignment of a value
    v = 3.14;
    FL_REQUIRE(v.is<double>());
    // FL_REQUIRE_EQ(v.get<double>(), 3.14);
    FL_REQUIRE_EQ(*v.ptr<double>(), 3.14);

    // 8) Visitor pattern
    struct TestVisitor {
        int result = 0;
        
        void accept(int value) { result = value; }
        void accept(const fl::string& value) { result = value.length(); }
        void accept(double value) { result = static_cast<int>(value); }
    };

    TestVisitor visitor;
    v.visit(visitor);
    FL_REQUIRE_EQ(visitor.result, 3); // 3.14 truncated to 3

    v = fl::string("hello world");
    v.visit(visitor);
    FL_REQUIRE_EQ(visitor.result, 11); // length of "hello world"

    v = 42;
    v.visit(visitor);
    FL_REQUIRE_EQ(visitor.result, 42);
}


// FL_TEST_CASE("Optional") {
//     Optional<int> opt;
//     FL_REQUIRE(opt.empty());

//     opt = 42;
//     FL_REQUIRE(!opt.empty());
//     FL_REQUIRE_EQ(*opt.ptr(), 42);

//     Optional<int> opt2 = opt;
//     FL_REQUIRE(!opt2.empty());
//     FL_REQUIRE_EQ(*opt2.ptr(), 42);

//     opt2 = 100;
//     FL_REQUIRE_EQ(*opt2.ptr(), 100);
// }
