// g++ --std=c++11 test.cpp

#include <random>

#include "test.h"
#include "fl/vector.h"
#include "fl/initializer_list.h"


using namespace fl;

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
        SortedHeapVector<int, Less> vec;
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
        SortedHeapVector<int, Less> vec;
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
        SortedHeapVector<int, Less> vec;
        vec.setMaxSize(5);
        // Fill the vector to capacity
        vec.insert(1);
        vec.insert(2);
        vec.insert(3);
        vec.insert(4);
        vec.insert(5);  // Max size is 5

        InsertResult result;
        vec.insert(6, &result);  // Try to insert into full vector
        
        CHECK_EQ(InsertResult::kMaxSize, result);  // Should return false
        CHECK(vec.size() == 5);  // Size shouldn't change
        CHECK(vec[4] == 5);  // Last element should still be 5
    }

    SUBCASE("Erase from empty") {
        SortedHeapVector<int, Less> vec;
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

TEST_CASE("HeapVector") {
    SUBCASE("resize") {
        HeapVector<int> vec;
        vec.resize(5);
        CHECK(vec.size() == 5);
        CHECK(vec.capacity() >= 5);
        for (int i = 0; i < 5; ++i) {
            CHECK_EQ(0, vec[i]);
        }
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
    
    SUBCASE("HeapVector initializer list") {
        fl::HeapVector<int> vec{10, 20, 30, 40};
        
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
        fl::vector<int> vec{100, 200, 300};  // This uses HeapVector
        
        CHECK(vec.size() == 3);
        CHECK(vec[0] == 100);
        CHECK(vec[1] == 200);
        CHECK(vec[2] == 300);
    }
    
    SUBCASE("Empty initializer list") {
        fl::FixedVector<int, 5> fixed_vec{};
        fl::HeapVector<int> heap_vec{};
        fl::InlinedVector<int, 3> inlined_vec{};
        
        CHECK(fixed_vec.size() == 0);
        CHECK(fixed_vec.empty());
        CHECK(heap_vec.size() == 0);
        CHECK(heap_vec.empty());
        CHECK(inlined_vec.size() == 0);
        CHECK(inlined_vec.empty());
    }
}
