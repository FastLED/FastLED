// g++ --std=c++11 test.cpp

#include "test.h"
#include "fl/ui.h"
#include "fl/variant.h"
#include "fl/optional.h"
#include "fl/str.h"
#include "fl/shared_ptr.h"
#include "fl/function.h"

using namespace fl;

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

TEST_CASE("Variant move semantics and RAII") {
    // Test the core issue: moved-from variants should be empty and not destroy moved-from objects
    TrackedObject::reset_counters();
    
    // Test 1: Verify moved-from variant is empty
    {
        Variant<int, TrackedObject> source(TrackedObject(42));
        CHECK(source.is<TrackedObject>());
        
        // Move construct - this is where the bug was
        Variant<int, TrackedObject> destination(fl::move(source));
        
        // Critical test: source should be empty after move
        CHECK(source.empty());
        CHECK(!source.is<TrackedObject>());
        CHECK(!source.is<int>());
        
        // destination should have the object
        CHECK(destination.is<TrackedObject>());
        CHECK_EQ(destination.ptr<TrackedObject>()->value, 42);
    }
    
    TrackedObject::reset_counters();
    
    // Test 2: Verify moved-from variant via assignment is empty
    {
        Variant<int, TrackedObject> source(TrackedObject(100));
        Variant<int, TrackedObject> destination;
        
        CHECK(source.is<TrackedObject>());
        CHECK(destination.empty());
        
        // Move assign - this is where the bug was
        destination = fl::move(source);
        
        // Critical test: source should be empty after move
        CHECK(source.empty());
        CHECK(!source.is<TrackedObject>());
        CHECK(!source.is<int>());
        
        // destination should have the object
        CHECK(destination.is<TrackedObject>());
        CHECK_EQ(destination.ptr<TrackedObject>()->value, 100);
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
        Variant<int, MockCallback> callback_variant(fl::move(callback));
        CHECK(callback_variant.is<MockCallback>());
        
        // Extract via move (like takeCallback did) - this was causing heap-use-after-free
        Variant<int, MockCallback> extracted_callback(fl::move(callback_variant));
        
        // Original variant should be empty - this is the key fix
        CHECK(callback_variant.empty());
        CHECK(!callback_variant.is<MockCallback>());
        
        // Extracted callback should work and shared_ptr should be valid
        CHECK(extracted_callback.is<MockCallback>());
        CHECK_EQ(shared_resource.use_count(), 2); // One in extracted callback, one local
        
        // Call the extracted callback - should not crash
        if (auto* cb = extracted_callback.ptr<MockCallback>()) {
            (*cb)();
        }
        
        // Shared resource should still be valid
        CHECK_EQ(shared_resource.use_count(), 2);
    }
}

TEST_CASE("HashMap iterator-based erase") {
    fl::hash_map<int, fl::string> map;
    
    // Fill the map with some data
    map[1] = "one";
    map[2] = "two";
    map[3] = "three";
    map[4] = "four";
    map[5] = "five";
    
    CHECK_EQ(map.size(), 5);
    
    // Test iterator-based erase
    auto it = map.find(3);
    CHECK(it != map.end());
    CHECK_EQ(it->second, "three");
    
    // Erase using iterator - should return iterator to next element
    auto next_it = map.erase(it);
    CHECK_EQ(map.size(), 4);
    CHECK(map.find(3) == map.end()); // Element should be gone
    
    // Verify all other elements are still there
    CHECK(map.find(1) != map.end());
    CHECK(map.find(2) != map.end());
    CHECK(map.find(4) != map.end());
    CHECK(map.find(5) != map.end());
    
    // Test erasing at end
    auto end_it = map.find(999); // Non-existent key
    CHECK(end_it == map.end());
    auto result_it = map.erase(end_it); // Should handle gracefully
    CHECK(result_it == map.end());
    CHECK_EQ(map.size(), 4); // Size should be unchanged
    
    // Test erasing all remaining elements using iterators
    while (!map.empty()) {
        auto first = map.begin();
        map.erase(first);
    }
    CHECK_EQ(map.size(), 0);
    CHECK(map.empty());
}

// Test the original test cases
TEST_CASE("Variant tests") {
    // 1) Default is empty
    Variant<int, fl::string> v;
    REQUIRE(v.empty());
    REQUIRE(!v.is<int>());
    REQUIRE(!v.is<fl::string>());

    // 2) Emplace a T
    v = 123;
    REQUIRE(v.is<int>());
    REQUIRE(!v.is<fl::string>());
    REQUIRE_EQ(*v.ptr<int>(), 123);

    // 3) Reset back to empty
    v.reset();
    REQUIRE(v.empty());


    // 4) Emplace a U
    v = fl::string("hello");
    REQUIRE(v.is<fl::string>());
    REQUIRE(!v.is<int>());
    REQUIRE(v.equals(fl::string("hello")));


    // 5) Copy‐construct preserves the U
    Variant<int, fl::string> v2(v);
    REQUIRE(v2.is<fl::string>());

    fl::string* str_ptr = v2.ptr<fl::string>();
    REQUIRE_NE(str_ptr, nullptr);
    REQUIRE_EQ(*str_ptr, fl::string("hello"));
    const bool is_str = v2.is<fl::string>();
    const bool is_int = v2.is<int>();

    CHECK(is_str);
    CHECK(!is_int);

#if 0
    // 6) Move‐construct leaves source empty
    Variant<int, fl::string> v3(std::move(v2));
    REQUIRE(v3.is<fl::string>());
    REQUIRE_EQ(v3.getU(), fl::string("hello"));
    REQUIRE(v2.isEmpty());

    // 7) Copy‐assign
    Variant<int, fl::string> v4;
    v4 = v3;
    REQUIRE(v4.is<fl::string>());
    REQUIRE_EQ(v4.getU(), fl::string("hello"));


    // 8) Swap two variants
    v4.emplaceT(7);
    v3.swap(v4);
    REQUIRE(v3.is<int>());
    REQUIRE_EQ(v3.getT(), 7);
    REQUIRE(v4.is<fl::string>());
    REQUIRE_EQ(v4.getU(), fl::string("hello"));
#endif
}

TEST_CASE("Variant") {
    // 1) Default is empty
    Variant<int, fl::string, double> v;
    REQUIRE(v.empty());
    REQUIRE(!v.is<int>());
    REQUIRE(!v.is<fl::string>());
    REQUIRE(!v.is<double>());

    // 2) Construct with a value
    Variant<int, fl::string, double> v1(123);
    REQUIRE(v1.is<int>());
    REQUIRE(!v1.is<fl::string>());
    REQUIRE(!v1.is<double>());
    REQUIRE_EQ(*v1.ptr<int>(), 123);

    // 3) Construct with a different type
    Variant<int, fl::string, double> v2(fl::string("hello"));
    REQUIRE(!v2.is<int>());
    REQUIRE(v2.is<fl::string>());
    REQUIRE(!v2.is<double>());
    REQUIRE_EQ(*v2.ptr<fl::string>(), fl::string("hello"));

    // 4) Copy construction
    Variant<int, fl::string, double> v3(v2);
    REQUIRE(v3.is<fl::string>());
    REQUIRE(v3.equals(fl::string("hello")));

    // 5) Assignment
    v = v1;
    REQUIRE(v.is<int>());
    REQUIRE_EQ(*v.ptr<int>(), 123);

    // 6) Reset
    v.reset();
    REQUIRE(v.empty());

    // 7) Assignment of a value
    v = 3.14;
    REQUIRE(v.is<double>());
    // REQUIRE_EQ(v.get<double>(), 3.14);
    REQUIRE_EQ(*v.ptr<double>(), 3.14);

    // 8) Visitor pattern
    struct TestVisitor {
        int result = 0;
        
        void accept(int value) { result = value; }
        void accept(const fl::string& value) { result = value.length(); }
        void accept(double value) { result = static_cast<int>(value); }
    };

    TestVisitor visitor;
    v.visit(visitor);
    REQUIRE_EQ(visitor.result, 3); // 3.14 truncated to 3

    v = fl::string("hello world");
    v.visit(visitor);
    REQUIRE_EQ(visitor.result, 11); // length of "hello world"

    v = 42;
    v.visit(visitor);
    REQUIRE_EQ(visitor.result, 42);
}


// TEST_CASE("Optional") {
//     Optional<int> opt;
//     REQUIRE(opt.empty());

//     opt = 42;
//     REQUIRE(!opt.empty());
//     REQUIRE_EQ(*opt.ptr(), 42);

//     Optional<int> opt2 = opt;
//     REQUIRE(!opt2.empty());
//     REQUIRE_EQ(*opt2.ptr(), 42);

//     opt2 = 100;
//     REQUIRE_EQ(*opt2.ptr(), 100);
// }
