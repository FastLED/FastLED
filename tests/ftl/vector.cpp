// g++ --std=c++11 test.cpp


#include "fl/stl/vector.h"
#include "fl/slice.h"  // For fl::span
#include "fl/stl/cstddef.h"
#include "fl/stl/new.h"
#include "doctest.h"
#include "fl/insert_result.h"
#include "fl/stl/allocator.h"
#include "fl/stl/vector.h"
#include "fl/stl/move.h"
#include "fl/stl/type_traits.h"
#include "fl/unused.h"
#include "stdlib.h"



TEST_CASE("Fixed vector simple") {
    fl::FixedVector<int, 5> vec;

    SUBCASE("Initial state") {
        CHECK(vec.size() == 0);
        CHECK(vec.capacity() == 5);
        CHECK(vec.empty());
    }

    SUBCASE("Push back and access") {
        vec.push_back(10);
        vec.push_back(20);
        vec.push_back(30);

        CHECK(vec.size() == 3);
        CHECK_FALSE(vec.empty());
        CHECK(vec[0] == 10);
        CHECK(vec[1] == 20);
        CHECK(vec[2] == 30);
    }

    SUBCASE("Push back beyond capacity") {
        for (int i = 0; i < 7; ++i) {
            vec.push_back(i * 10);
        }

        CHECK(vec.size() == 5);
        CHECK(vec.capacity() == 5);
        CHECK(vec[4] == 40);
    }

    SUBCASE("Clear") {
        vec.push_back(10);
        vec.push_back(20);
        vec.clear();

        CHECK(vec.size() == 0);
        CHECK(vec.empty());
    }
}

TEST_CASE("Fixed vector insert") {
    fl::FixedVector<int, 5> vec;

    SUBCASE("Insert at beginning") {
        vec.push_back(20);
        vec.push_back(30);
        bool inserted = vec.insert(vec.begin(), 10);

        CHECK(inserted);
        CHECK(vec.size() == 3);
        CHECK(vec[0] == 10);
        CHECK(vec[1] == 20);
        CHECK(vec[2] == 30);
    }

    SUBCASE("Insert in middle") {
        vec.push_back(10);
        vec.push_back(30);
        bool inserted = vec.insert(vec.begin() + 1, 20);

        CHECK(inserted);
        CHECK(vec.size() == 3);
        CHECK(vec[0] == 10);
        CHECK(vec[1] == 20);
        CHECK(vec[2] == 30);
    }

    SUBCASE("Insert at end") {
        vec.push_back(10);
        vec.push_back(20);
        bool inserted = vec.insert(vec.end(), 30);

        CHECK(inserted);
        CHECK(vec.size() == 3);
        CHECK(vec[0] == 10);
        CHECK(vec[1] == 20);
        CHECK(vec[2] == 30);
    }

    SUBCASE("Insert when full") {
        vec.push_back(10);
        vec.push_back(20);
        vec.push_back(30);
        vec.push_back(40);
        vec.push_back(50);
        bool inserted = vec.insert(vec.begin() + 2, 25);

        CHECK_FALSE(inserted);
        CHECK(vec.size() == 5);
        CHECK(vec[0] == 10);
        CHECK(vec[1] == 20);
        CHECK(vec[2] == 30);
        CHECK(vec[3] == 40);
        CHECK(vec[4] == 50);
    }
}

TEST_CASE("Fixed vector find_if with predicate") {
    fl::FixedVector<int, 5> vec;

    SUBCASE("Find even number") {
        vec.push_back(1);
        vec.push_back(2);
        vec.push_back(3);
        vec.push_back(4);
        vec.push_back(5);

        auto it = vec.find_if([](int n) { return n % 2 == 0; });
        CHECK(it != vec.end());
        CHECK(*it == 2);
    }

    SUBCASE("Find number greater than 3") {
        vec.push_back(1);
        vec.push_back(2);
        vec.push_back(3);
        vec.push_back(4);
        vec.push_back(5);

        auto it = vec.find_if([](int n) { return n > 3; });
        CHECK(it != vec.end());
        CHECK(*it == 4);
    }

    SUBCASE("Find non-existent condition") {
        vec.push_back(1);
        vec.push_back(3);
        vec.push_back(5);

        auto it = vec.find_if([](int n) { return n % 2 == 0; });
        CHECK(it == vec.end());
    }

    SUBCASE("Find in empty vector") {
        auto it = vec.find_if([](int n) {
            FL_UNUSED(n);
            return true;
        });
        CHECK(it == vec.end());
    }
}

TEST_CASE("fl::FixedVector construction and destruction") {
    
    static int live_object_count = 0;

    struct TestObject {
        int value;
        TestObject(int v = 0) : value(v) { ++live_object_count; }
        ~TestObject() { --live_object_count; }
        TestObject(const TestObject& other) : value(other.value) { ++live_object_count; }
        TestObject& operator=(const TestObject& other) {
            value = other.value;
            return *this;
        }
    };

    SUBCASE("Construction and destruction") {
        REQUIRE_EQ(0, live_object_count);
        live_object_count = 0;
        {
            fl::FixedVector<TestObject, 3> vec;
            CHECK(live_object_count == 0);

            vec.push_back(TestObject(1));
            vec.push_back(TestObject(2));
            vec.push_back(TestObject(3));

            CHECK(live_object_count == 3);  // 3 objects in the vector

            vec.pop_back();
            CHECK(live_object_count == 2);  // 2 objects left in the vector
        }
        // vec goes out of scope here
        REQUIRE_EQ(live_object_count, 0);
    }

    SUBCASE("Clear") {
        live_object_count = 0;
        {
            fl::FixedVector<TestObject, 3> vec;
            vec.push_back(TestObject(1));
            vec.push_back(TestObject(2));

            CHECK(live_object_count == 2);

            vec.clear();

            CHECK(live_object_count == 0);  // All objects should be destroyed after clear
        }
        CHECK(live_object_count == 0);
    }

    SUBCASE("Stress test clear, insert and remove") {

        fl::vector_inlined<TestObject, 20> vec;
        size_t checked_size = 0;
        for (int i = 0; i < 1000; ++i) {
            int random_value = rand() % 4;

            switch (random_value) {
                case 0:
                    if (!vec.full()) {
                        vec.push_back(TestObject(i));
                        ++checked_size;
                    } else {
                        REQUIRE_EQ(20, vec.size());
                    }
                    break;
                case 1:
                    if (!vec.empty()) {
                        vec.pop_back();
                        --checked_size;
                    } else {
                        REQUIRE_EQ(0, checked_size);
                    }
                    break;
                case 2:
                    vec.clear();
                    checked_size = 0;
                    REQUIRE_EQ(0, vec.size());
                    break;
            }

        }
    }
}

TEST_CASE("Fixed vector implicit copy constructor from span") {
    SUBCASE("from C array via span") {
        int source_data[] = {10, 20, 30, 40, 50};
        fl::span<const int> s(source_data, 5);

        // Implicit conversion from span to FixedVector
        fl::FixedVector<int, 10> vec = s;  // N=10, span has 5 elements

        CHECK(vec.size() == 5);
        CHECK(vec[0] == 10);
        CHECK(vec[1] == 20);
        CHECK(vec[2] == 30);
        CHECK(vec[3] == 40);
        CHECK(vec[4] == 50);

        // Verify it's a copy
        vec[0] = 99;
        CHECK(source_data[0] == 10);
    }

    SUBCASE("from span larger than capacity") {
        int source_data[] = {1, 2, 3, 4, 5, 6, 7, 8};
        fl::span<const int> s(source_data, 8);

        // FixedVector with capacity 5 should only copy first 5 elements
        fl::FixedVector<int, 5> vec = s;

        CHECK(vec.size() == 5);
        CHECK(vec[0] == 1);
        CHECK(vec[1] == 2);
        CHECK(vec[2] == 3);
        CHECK(vec[3] == 4);
        CHECK(vec[4] == 5);
    }

    SUBCASE("from vector via span") {
        fl::vector<int> heap_vec;
        heap_vec.push_back(100);
        heap_vec.push_back(200);
        heap_vec.push_back(300);

        fl::span<const int> s(heap_vec);
        fl::FixedVector<int, 10> fixed_vec = s;

        CHECK(fixed_vec.size() == 3);
        CHECK(fixed_vec[0] == 100);
        CHECK(fixed_vec[1] == 200);
        CHECK(fixed_vec[2] == 300);
    }
}

TEST_CASE("Fixed vector advanced") {
    fl::FixedVector<int, 5> vec;

    SUBCASE("Pop back") {
        vec.push_back(10);
        vec.push_back(20);
        vec.pop_back();

        CHECK(vec.size() == 1);
        CHECK(vec[0] == 10);
    }

    SUBCASE("Front and back") {
        vec.push_back(10);
        vec.push_back(20);
        vec.push_back(30);

        CHECK(vec.front() == 10);
        CHECK(vec.back() == 30);
    }

    SUBCASE("Iterator") {
        vec.push_back(10);
        vec.push_back(20);
        vec.push_back(30);

        int sum = 0;
        for (auto it = vec.begin(); it != vec.end(); ++it) {
            sum += *it;
        }

        CHECK(sum == 60);
    }

    SUBCASE("Erase") {
        vec.push_back(10);
        vec.push_back(20);
        vec.push_back(30);

        vec.erase(vec.begin() + 1);

        CHECK(vec.size() == 2);
        CHECK(vec[0] == 10);
        CHECK(vec[1] == 30);
    }

    SUBCASE("Find and has") {
        vec.push_back(10);
        vec.push_back(20);
        vec.push_back(30);

        CHECK(vec.has(20));
        CHECK_FALSE(vec.has(40));

        auto it = vec.find(20);
        CHECK(it != vec.end());
        CHECK(*it == 20);

        it = vec.find(40);
        CHECK(it == vec.end());
    }
}

TEST_CASE("Fixed vector with custom type") {
    struct Point {
        int x, y;
        Point(int x = 0, int y = 0) : x(x), y(y) {}
        bool operator==(const Point& other) const { return x == other.x && y == other.y; }
    };

    fl::FixedVector<Point, 3> vec;

    SUBCASE("Push and access custom type") {
        vec.push_back(Point(1, 2));
        vec.push_back(Point(3, 4));

        CHECK(vec.size() == 2);
        CHECK(vec[0].x == 1);
        CHECK(vec[0].y == 2);
        CHECK(vec[1].x == 3);
        CHECK(vec[1].y == 4);
    }

    SUBCASE("Find custom type") {
        vec.push_back(Point(1, 2));
        vec.push_back(Point(3, 4));

        auto it = vec.find(Point(3, 4));
        CHECK(it != vec.end());
        CHECK(it->x == 3);
        CHECK(it->y == 4);
    }
}

TEST_CASE("SortedVector") {    
    struct Less {
        bool operator()(int a, int b) const { return a < b; }
    };
    


    SUBCASE("Insert maintains order") {
        fl::SortedHeapVector<int, Less> vec;
        vec.insert(3);
        vec.insert(1);
        vec.insert(4);
        vec.insert(2);

        CHECK(vec.size() == 4);
        CHECK(vec[0] == 1);
        CHECK(vec[1] == 2);
        CHECK(vec[2] == 3);
        CHECK(vec[3] == 4);
    }

    SUBCASE("Erase removes element") {
        fl::SortedHeapVector<int, Less> vec;
        vec.insert(3);
        vec.insert(1);
        vec.insert(4);
        vec.insert(2);
        
        vec.erase(3);  // Remove the value 3
        
        CHECK(vec.size() == 3);
        CHECK_FALSE(vec.has(3));  // Verify 3 is no longer present
        
        // Verify remaining elements are still in order
        CHECK(vec[0] == 1);
        CHECK(vec[1] == 2);
        CHECK(vec[2] == 4);
    }

    SUBCASE("Insert when full") {
        fl::SortedHeapVector<int, Less> vec;
        vec.setMaxSize(5);
        // Fill the vector to capacity
        vec.insert(1);
        vec.insert(2);
        vec.insert(3);
        vec.insert(4);
        vec.insert(5);  // Max size is 5

        fl::InsertResult result;
        vec.insert(6, &result);  // Try to insert into full vector
        
        CHECK_EQ(fl::InsertResult::kMaxSize, result);  // Should return false
        CHECK(vec.size() == 5);  // Size shouldn't change
        CHECK(vec[4] == 5);  // Last element should still be 5
    }

    SUBCASE("Erase from empty") {
        fl::SortedHeapVector<int, Less> vec;
        bool ok = vec.erase(1);  // Try to erase from empty vector
        CHECK(!ok);  // Should return false
        CHECK(vec.size() == 0);  // Should still be empty
        CHECK(vec.empty());

        ok = vec.erase(vec.end());
        CHECK(!ok);  // Should return false
        CHECK(vec.size() == 0);  // Should still be empty
        CHECK(vec.empty());

        ok = vec.erase(vec.begin());
        CHECK(!ok);  // Should return false
        CHECK(vec.size() == 0);  // Should still be empty
        CHECK(vec.empty());
    }
}

TEST_CASE("vector") {
    SUBCASE("resize") {
        fl::vector<int> vec;
        vec.resize(5);
        CHECK(vec.size() == 5);
        CHECK(vec.capacity() >= 5);
        for (int i = 0; i < 5; ++i) {
            CHECK_EQ(0, vec[i]);
        }
    }

    SUBCASE("implicit copy constructor from span") {
        // Create a source array
        int source_data[] = {10, 20, 30, 40, 50};

        // Create a span from the array (implicit conversion)
        fl::span<const int> s(source_data, 5);

        // Test implicit conversion from span to vector
        fl::vector<int> vec = s;  // This should work with the implicit constructor

        CHECK(vec.size() == 5);
        CHECK(vec[0] == 10);
        CHECK(vec[1] == 20);
        CHECK(vec[2] == 30);
        CHECK(vec[3] == 40);
        CHECK(vec[4] == 50);

        // Verify it's a copy (modifying vec doesn't affect source)
        vec[0] = 99;
        CHECK(source_data[0] == 10);  // Original unchanged
        CHECK(vec[0] == 99);           // Copy modified
    }

    SUBCASE("copy constructor from span of different containers") {
        // Test with FixedVector
        fl::FixedVector<int, 5> fixed_vec;
        fixed_vec.push_back(1);
        fixed_vec.push_back(2);
        fixed_vec.push_back(3);

        fl::span<const int> fixed_span(fixed_vec);
        fl::vector<int> from_fixed = fixed_span;

        CHECK(from_fixed.size() == 3);
        CHECK(from_fixed[0] == 1);
        CHECK(from_fixed[1] == 2);
        CHECK(from_fixed[2] == 3);

        // Test with another vector
        fl::vector<int> heap_vec;
        heap_vec.push_back(100);
        heap_vec.push_back(200);

        fl::span<const int> heap_span(heap_vec);
        fl::vector<int> from_heap = heap_span;

        CHECK(from_heap.size() == 2);
        CHECK(from_heap[0] == 100);
        CHECK(from_heap[1] == 200);
    }
}


TEST_CASE("Initializer list constructors") {
    
    SUBCASE("FixedVector initializer list") {
        fl::FixedVector<int, 10> vec{1, 2, 3, 4, 5};  
        
        CHECK(vec.size() == 5);
        CHECK(vec[0] == 1);
        CHECK(vec[1] == 2);
        CHECK(vec[2] == 3);
        CHECK(vec[3] == 4);
        CHECK(vec[4] == 5);
    }
    
    SUBCASE("FixedVector initializer list with overflow") {
        // Test that overflow is handled gracefully - only first N elements are taken
        fl::FixedVector<int, 3> vec{1, 2, 3, 4, 5, 6, 7};
        
        CHECK(vec.size() == 3);
        CHECK(vec[0] == 1);
        CHECK(vec[1] == 2);
        CHECK(vec[2] == 3);
    }
    
    SUBCASE("vector initializer list") {
        fl::vector<int> vec{10, 20, 30, 40};
        
        CHECK(vec.size() == 4);
        CHECK(vec[0] == 10);
        CHECK(vec[1] == 20);
        CHECK(vec[2] == 30);
        CHECK(vec[3] == 40);
    }
    
    SUBCASE("InlinedVector initializer list - small size") {
        fl::InlinedVector<int, 10> vec{1, 2, 3};
        
        CHECK(vec.size() == 3);
        CHECK(vec[0] == 1);
        CHECK(vec[1] == 2);
        CHECK(vec[2] == 3);
    }
    
    SUBCASE("InlinedVector initializer list - large size") {
        fl::InlinedVector<int, 3> vec{1, 2, 3, 4, 5, 6};  // Should trigger heap mode
        
        CHECK(vec.size() == 6);
        CHECK(vec[0] == 1);
        CHECK(vec[1] == 2);
        CHECK(vec[2] == 3);
        CHECK(vec[3] == 4);
        CHECK(vec[4] == 5);
        CHECK(vec[5] == 6);
    }
    
    SUBCASE("fl::vector initializer list") {
        fl::vector<int> vec{100, 200, 300};  // This uses vector
        
        CHECK(vec.size() == 3);
        CHECK(vec[0] == 100);
        CHECK(vec[1] == 200);
        CHECK(vec[2] == 300);
    }
    
    SUBCASE("Empty initializer list") {
        fl::FixedVector<int, 5> fixed_vec{};
        fl::vector<int> heap_vec{};
        fl::InlinedVector<int, 3> inlined_vec{};

        CHECK(fixed_vec.size() == 0);
        CHECK(fixed_vec.empty());
        CHECK(heap_vec.size() == 0);
        CHECK(heap_vec.empty());
        CHECK(inlined_vec.size() == 0);
        CHECK(inlined_vec.empty());
    }
}

// Test automatic realloc() optimization for trivially copyable types
TEST_CASE("Automatic realloc optimization for trivially copyable types") {
    SUBCASE("Default allocator with int (trivially copyable)") {
        // int is trivially copyable - default allocator should automatically use realloc()
        fl::vector<int> vec;  // Uses fl::allocator<int> by default

        // Test basic operations
        vec.push_back(10);
        vec.push_back(20);
        vec.push_back(30);

        CHECK(vec.size() == 3);
        CHECK(vec[0] == 10);
        CHECK(vec[1] == 20);
        CHECK(vec[2] == 30);

        // Test resize - this should automatically trigger reallocate() optimization
        vec.resize(10);
        CHECK(vec.size() == 10);
        CHECK(vec[0] == 10);
        CHECK(vec[1] == 20);
        CHECK(vec[2] == 30);

        // Test shrink
        vec.resize(2);
        CHECK(vec.size() == 2);
        CHECK(vec[0] == 10);
        CHECK(vec[1] == 20);
    }

    SUBCASE("Default allocator with struct POD (trivially copyable)") {
        // Simple POD struct should automatically use realloc() optimization
        struct SimplePOD {
            int x;
            int y;
        };

        fl::vector<SimplePOD> vec;  // Uses fl::allocator<SimplePOD> by default

        vec.push_back({1, 2});
        vec.push_back({3, 4});

        CHECK(vec.size() == 2);
        CHECK(vec[0].x == 1);
        CHECK(vec[0].y == 2);
        CHECK(vec[1].x == 3);
        CHECK(vec[1].y == 4);

        // Trigger reallocation - should automatically use realloc()
        vec.reserve(100);
        CHECK(vec.capacity() >= 100);
        CHECK(vec[0].x == 1);
        CHECK(vec[1].x == 3);
    }

    SUBCASE("Default allocator stress test with automatic realloc") {
        // Test many reallocations - all automatically optimized
        fl::vector<int> vec;

        for (int i = 0; i < 1000; ++i) {
            vec.push_back(i);
        }

        CHECK(vec.size() == 1000);

        // Verify all values are correct after many reallocations
        for (int i = 0; i < 1000; ++i) {
            CHECK(vec[i] == i);
        }
    }

    SUBCASE("Non-trivially copyable types use safe path") {
        // This struct has a non-trivial destructor
        // Default allocator should automatically use allocate-copy-deallocate
        struct NonTriviallyCopyable {
            int* ptr;
            NonTriviallyCopyable(int val = 0) : ptr(new int(val)) {}
            ~NonTriviallyCopyable() { delete ptr; }
            NonTriviallyCopyable(const NonTriviallyCopyable& other) : ptr(new int(*other.ptr)) {}
            NonTriviallyCopyable& operator=(const NonTriviallyCopyable& other) {
                if (this != &other) {
                    delete ptr;
                    ptr = new int(*other.ptr);
                }
                return *this;
            }
        };

        // Should compile and work correctly (using safe allocate-copy-deallocate)
        fl::vector<NonTriviallyCopyable> vec;
        vec.push_back(NonTriviallyCopyable(42));
        vec.push_back(NonTriviallyCopyable(100));

        CHECK(vec.size() == 2);
        CHECK(*vec[0].ptr == 42);
        CHECK(*vec[1].ptr == 100);

        // Trigger reallocation - should use safe path, not realloc()
        vec.reserve(100);
        CHECK(vec.capacity() >= 100);
        CHECK(*vec[0].ptr == 42);
        CHECK(*vec[1].ptr == 100);
    }
}

// BACKWARDS COMPATIBILITY: allocator_realloc still works but is now redundant
TEST_CASE("allocator_realloc backwards compatibility") {
    SUBCASE("allocator_realloc still works (now redundant)") {
        // This still compiles and works, but provides no benefit over default allocator
        fl::vector<int, fl::allocator_realloc<int>> vec;

        vec.push_back(10);
        vec.push_back(20);
        vec.push_back(30);

        CHECK(vec.size() == 3);
        CHECK(vec[0] == 10);
        CHECK(vec[1] == 20);
        CHECK(vec[2] == 30);
    }
}

// COMPILE-TIME TEST: Non-trivially copyable types should fail to compile
// Uncomment these tests to verify static_assert works:
//
// TEST_CASE("allocator_realloc with non-POD types - SHOULD NOT COMPILE") {
//     // This struct has a non-trivial destructor
//     struct NonTriviallyCopyable {
//         int* ptr;
//         NonTriviallyCopyable() : ptr(new int(0)) {}
//         ~NonTriviallyCopyable() { delete ptr; }
//     };
//
//     // ❌ This should fail to compile with static_assert error:
//     // "allocator_realloc<T> requires T to be trivially copyable"
//     fl::vector<NonTriviallyCopyable, fl::allocator_realloc<NonTriviallyCopyable>> vec;
// }
//
// TEST_CASE("allocator_realloc with fl::vector - SHOULD NOT COMPILE") {
//     // ❌ Nested vectors with allocator_realloc should fail:
//     // fl::vector is not trivially copyable
//     fl::vector<fl::vector<int>, fl::allocator_realloc<fl::vector<int>>> nested;
// }

TEST_CASE("is_trivially_copyable trait") {
    SUBCASE("Fundamental types are trivially copyable") {
        CHECK(fl::is_trivially_copyable<int>::value);
        CHECK(fl::is_trivially_copyable<float>::value);
        CHECK(fl::is_trivially_copyable<double>::value);
        CHECK(fl::is_trivially_copyable<char>::value);
        CHECK(fl::is_trivially_copyable<bool>::value);
    }

    SUBCASE("Pointers are trivially copyable") {
        CHECK(fl::is_trivially_copyable<int*>::value);
        CHECK(fl::is_trivially_copyable<void*>::value);
    }

    SUBCASE("Simple POD structs are trivially copyable") {
        struct SimplePOD {
            int x;
            float y;
        };
        CHECK(fl::is_trivially_copyable<SimplePOD>::value);
    }

    SUBCASE("Types with non-trivial operations are NOT trivially copyable") {
        struct NonTriviallyCopyable {
            int* ptr;
            NonTriviallyCopyable() : ptr(nullptr) {}
            ~NonTriviallyCopyable() { delete ptr; }
        };
        CHECK_FALSE(fl::is_trivially_copyable<NonTriviallyCopyable>::value);
    }
}
