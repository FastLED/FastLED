#include <random>

#include "test.h"
#include "fl/deque.h"
#include "fl/initializer_list.h"

using namespace fl;

TEST_CASE("deque construction and basic operations") {
    fl::deque<int> dq;

    SUBCASE("Initial state") {
        CHECK(dq.size() == 0);
        CHECK(dq.empty());
    }

    SUBCASE("Push back and access") {
        dq.push_back(10);
        dq.push_back(20);
        dq.push_back(30);

        CHECK(dq.size() == 3);
        CHECK_FALSE(dq.empty());
        CHECK(dq[0] == 10);
        CHECK(dq[1] == 20);
        CHECK(dq[2] == 30);
        CHECK(dq.front() == 10);
        CHECK(dq.back() == 30);
    }

    SUBCASE("Push front and access") {
        dq.push_front(10);
        dq.push_front(20);
        dq.push_front(30);

        CHECK(dq.size() == 3);
        CHECK_FALSE(dq.empty());
        CHECK(dq[0] == 30);
        CHECK(dq[1] == 20);
        CHECK(dq[2] == 10);
        CHECK(dq.front() == 30);
        CHECK(dq.back() == 10);
    }

    SUBCASE("Mixed push operations") {
        dq.push_back(20);
        dq.push_front(10);
        dq.push_back(30);
        dq.push_front(5);

        CHECK(dq.size() == 4);
        CHECK(dq[0] == 5);
        CHECK(dq[1] == 10);
        CHECK(dq[2] == 20);
        CHECK(dq[3] == 30);
        CHECK(dq.front() == 5);
        CHECK(dq.back() == 30);
    }

    SUBCASE("Pop operations") {
        dq.push_back(10);
        dq.push_back(20);
        dq.push_back(30);
        dq.push_back(40);

        dq.pop_back();
        CHECK(dq.size() == 3);
        CHECK(dq.back() == 30);

        dq.pop_front();
        CHECK(dq.size() == 2);
        CHECK(dq.front() == 20);
        CHECK(dq[0] == 20);
        CHECK(dq[1] == 30);
    }

    SUBCASE("Clear") {
        dq.push_back(10);
        dq.push_front(5);
        dq.push_back(20);
        dq.clear();

        CHECK(dq.size() == 0);
        CHECK(dq.empty());
    }
}

TEST_CASE("deque iterators") {
    fl::deque<int> dq;

    SUBCASE("Iterator traversal") {
        dq.push_back(10);
        dq.push_back(20);
        dq.push_back(30);

        int sum = 0;
        for (auto it = dq.begin(); it != dq.end(); ++it) {
            sum += *it;
        }
        CHECK(sum == 60);

        // Range-based for loop
        sum = 0;
        for (const auto& value : dq) {
            sum += value;
        }
        CHECK(sum == 60);
    }

    SUBCASE("Const iterator") {
        dq.push_back(5);
        dq.push_back(15);
        dq.push_back(25);

        const auto& const_dq = dq;
        int product = 1;
        for (auto it = const_dq.begin(); it != const_dq.end(); ++it) {
            product *= *it;
        }
        CHECK(product == 1875); // 5 * 15 * 25
    }

    SUBCASE("Empty deque iterators") {
        CHECK(dq.begin() == dq.end());
    }
}

TEST_CASE("deque copy and move semantics") {
    SUBCASE("Copy constructor") {
        fl::deque<int> dq1;
        dq1.push_back(10);
        dq1.push_front(5);
        dq1.push_back(20);

        fl::deque<int> dq2(dq1);
        CHECK(dq2.size() == 3);
        CHECK(dq2[0] == 5);
        CHECK(dq2[1] == 10);
        CHECK(dq2[2] == 20);

        // Ensure independence
        dq1.push_back(30);
        CHECK(dq2.size() == 3);
        CHECK(dq1.size() == 4);
    }

    SUBCASE("Copy assignment") {
        fl::deque<int> dq1;
        dq1.push_back(1);
        dq1.push_back(2);
        dq1.push_back(3);

        fl::deque<int> dq2;
        dq2.push_back(99);
        
        dq2 = dq1;
        CHECK(dq2.size() == 3);
        CHECK(dq2[0] == 1);
        CHECK(dq2[1] == 2);
        CHECK(dq2[2] == 3);
    }

    SUBCASE("Move constructor") {
        fl::deque<int> dq1;
        dq1.push_back(10);
        dq1.push_back(20);
        dq1.push_back(30);

        fl::deque<int> dq2(fl::move(dq1));
        CHECK(dq2.size() == 3);
        CHECK(dq2[0] == 10);
        CHECK(dq2[1] == 20);
        CHECK(dq2[2] == 30);
        CHECK(dq1.empty()); // dq1 should be empty after move
    }
}

TEST_CASE("deque with custom types") {
    struct Point {
        int x, y;
        Point(int x = 0, int y = 0) : x(x), y(y) {}
        Point(const Point& other) : x(other.x), y(other.y) {}
        Point& operator=(const Point& other) {
            x = other.x;
            y = other.y;
            return *this;
        }
        bool operator==(const Point& other) const { 
            return x == other.x && y == other.y; 
        }
    };

    fl::deque<Point> dq;

    SUBCASE("Push and access custom type") {
        dq.push_back(Point(1, 2));
        dq.push_front(Point(3, 4));
        dq.push_back(Point(5, 6));

        CHECK(dq.size() == 3);
        CHECK(dq[0].x == 3);
        CHECK(dq[0].y == 4);
        CHECK(dq[1].x == 1);
        CHECK(dq[1].y == 2);
        CHECK(dq[2].x == 5);
        CHECK(dq[2].y == 6);
    }

    SUBCASE("Object lifecycle management") {
        static int construct_count = 0;
        static int destruct_count = 0;

        struct TestObject {
            int value;
            TestObject(int v = 0) : value(v) { ++construct_count; }
            TestObject(const TestObject& other) : value(other.value) { ++construct_count; }
            ~TestObject() { ++destruct_count; }
            TestObject& operator=(const TestObject& other) {
                value = other.value;
                return *this;
            }
        };

        construct_count = 0;
        destruct_count = 0;

        {
            fl::deque<TestObject> test_dq;
            test_dq.push_back(TestObject(1));
            test_dq.push_back(TestObject(2));
            test_dq.push_front(TestObject(3));

            CHECK(construct_count >= 3); // At least 3 objects constructed

            test_dq.pop_back();
            test_dq.pop_front();

            CHECK(destruct_count >= 2); // At least 2 objects destroyed
        }
        // Deque goes out of scope, remaining objects should be destroyed
        CHECK(construct_count == destruct_count); // All constructed objects should be destroyed
    }
}

TEST_CASE("deque initializer list constructor") {
    SUBCASE("Basic initializer list") {
        fl::deque<int> dq{1, 2, 3, 4, 5};
        
        CHECK(dq.size() == 5);
        CHECK(dq[0] == 1);
        CHECK(dq[1] == 2);
        CHECK(dq[2] == 3);
        CHECK(dq[3] == 4);
        CHECK(dq[4] == 5);
    }

    SUBCASE("Empty initializer list") {
        fl::deque<int> dq{};
        
        CHECK(dq.size() == 0);
        CHECK(dq.empty());
    }

    SUBCASE("Single element initializer list") {
        fl::deque<int> dq{42};
        
        CHECK(dq.size() == 1);
        CHECK(dq[0] == 42);
        CHECK(dq.front() == 42);
        CHECK(dq.back() == 42);
    }
}

TEST_CASE("deque resize operations") {
    fl::deque<int> dq;

    SUBCASE("Resize to larger size") {
        dq.push_back(1);
        dq.push_back(2);
        dq.resize(5);

        CHECK(dq.size() == 5);
        CHECK(dq[0] == 1);
        CHECK(dq[1] == 2);
        CHECK(dq[2] == 0); // Default constructed
        CHECK(dq[3] == 0);
        CHECK(dq[4] == 0);
    }

    SUBCASE("Resize to smaller size") {
        dq.push_back(1);
        dq.push_back(2);
        dq.push_back(3);
        dq.push_back(4);
        dq.push_back(5);
        dq.resize(3);

        CHECK(dq.size() == 3);
        CHECK(dq[0] == 1);
        CHECK(dq[1] == 2);
        CHECK(dq[2] == 3);
    }

    SUBCASE("Resize with value") {
        dq.push_back(1);
        dq.push_back(2);
        dq.resize(5, 99);

        CHECK(dq.size() == 5);
        CHECK(dq[0] == 1);
        CHECK(dq[1] == 2);
        CHECK(dq[2] == 99);
        CHECK(dq[3] == 99);
        CHECK(dq[4] == 99);
    }
}

TEST_CASE("deque edge cases") {
    fl::deque<int> dq;

    SUBCASE("Multiple push/pop cycles") {
        // Simulate queue-like usage
        for (int i = 0; i < 20; ++i) {
            dq.push_back(i);
        }
        
        for (int i = 0; i < 10; ++i) {
            dq.pop_front();
        }
        
        for (int i = 20; i < 30; ++i) {
            dq.push_back(i);
        }
        
        CHECK(dq.size() == 20);
        CHECK(dq.front() == 10); // First 10 elements were popped
        CHECK(dq.back() == 29);
    }

    SUBCASE("Stack-like usage from both ends") {
        dq.push_front(1);
        dq.push_back(2);
        dq.push_front(3);
        dq.push_back(4);
        
        CHECK(dq.size() == 4);
        CHECK(dq[0] == 3);
        CHECK(dq[1] == 1);
        CHECK(dq[2] == 2);
        CHECK(dq[3] == 4);
        
        dq.pop_front();
        dq.pop_back();
        
        CHECK(dq.size() == 2);
        CHECK(dq[0] == 1);
        CHECK(dq[1] == 2);
    }

    SUBCASE("Empty deque operations") {
        // These operations should be safe on empty deque
        CHECK(dq.empty());
        CHECK(dq.size() == 0);
        CHECK(dq.begin() == dq.end());
        
        // Add and remove single element
        dq.push_back(42);
        CHECK(dq.size() == 1);
        dq.pop_back();
        CHECK(dq.empty());
        
        dq.push_front(42);
        CHECK(dq.size() == 1);
        dq.pop_front();
        CHECK(dq.empty());
    }
}

TEST_CASE("deque stress test") {
    fl::deque<int> dq;
    
    SUBCASE("Large number of operations") {
        const int num_ops = 1000;
        
        // Fill with many elements
        for (int i = 0; i < num_ops; ++i) {
            if (i % 2 == 0) {
                dq.push_back(i);
            } else {
                dq.push_front(i);
            }
        }
        
        CHECK(dq.size() == num_ops);
        
        // Remove half the elements
        for (int i = 0; i < num_ops / 2; ++i) {
            if (i % 2 == 0) {
                dq.pop_back();
            } else {
                dq.pop_front();
            }
        }
        
        CHECK(dq.size() == num_ops / 2);
        
        // Clear everything
        dq.clear();
        CHECK(dq.empty());
    }
}
