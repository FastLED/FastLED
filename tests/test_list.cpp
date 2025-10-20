#include "test.h"
#include "fl/list.h"
#include "fl/initializer_list.h"


TEST_CASE("list basic operations") {
    fl::list<int> lst;

    SUBCASE("Initial state") {
        CHECK(lst.size() == 0);
        CHECK(lst.empty());
    }

    SUBCASE("Push back and access") {
        lst.push_back(10);
        lst.push_back(20);
        lst.push_back(30);

        CHECK(lst.size() == 3);
        CHECK_FALSE(lst.empty());
        CHECK(lst.front() == 10);
        CHECK(lst.back() == 30);
    }

    SUBCASE("Push front") {
        lst.push_back(20);
        lst.push_front(10);
        lst.push_back(30);

        CHECK(lst.size() == 3);
        CHECK(lst.front() == 10);
        CHECK(lst.back() == 30);
    }

    SUBCASE("Pop back") {
        lst.push_back(10);
        lst.push_back(20);
        lst.push_back(30);
        lst.pop_back();

        CHECK(lst.size() == 2);
        CHECK(lst.back() == 20);
    }

    SUBCASE("Pop front") {
        lst.push_back(10);
        lst.push_back(20);
        lst.push_back(30);
        lst.pop_front();

        CHECK(lst.size() == 2);
        CHECK(lst.front() == 20);
    }

    SUBCASE("Clear") {
        lst.push_back(10);
        lst.push_back(20);
        lst.clear();

        CHECK(lst.size() == 0);
        CHECK(lst.empty());
    }
}

TEST_CASE("list iterators") {
    fl::list<int> lst;

    SUBCASE("Forward iteration") {
        lst.push_back(10);
        lst.push_back(20);
        lst.push_back(30);

        int sum = 0;
        for (auto it = lst.begin(); it != lst.end(); ++it) {
            sum += *it;
        }

        CHECK(sum == 60);
    }

    SUBCASE("Range-based for loop") {
        lst.push_back(10);
        lst.push_back(20);
        lst.push_back(30);

        int sum = 0;
        for (int value : lst) {
            sum += value;
        }

        CHECK(sum == 60);
    }

    SUBCASE("Backward iteration") {
        lst.push_back(10);
        lst.push_back(20);
        lst.push_back(30);

        auto it = lst.end();
        --it;
        CHECK(*it == 30);
        --it;
        CHECK(*it == 20);
        --it;
        CHECK(*it == 10);
    }

    SUBCASE("Iterator equality") {
        lst.push_back(10);
        auto it1 = lst.begin();
        auto it2 = lst.begin();
        CHECK(it1 == it2);

        ++it2;
        CHECK(it1 != it2);
    }
}

TEST_CASE("list insert and erase") {
    fl::list<int> lst;

    SUBCASE("Insert at beginning") {
        lst.push_back(20);
        lst.push_back(30);
        auto it = lst.insert(lst.begin(), 10);

        CHECK(lst.size() == 3);
        CHECK(*it == 10);
        CHECK(lst.front() == 10);
    }

    SUBCASE("Insert in middle") {
        lst.push_back(10);
        lst.push_back(30);
        auto it = lst.begin();
        ++it;
        it = lst.insert(it, 20);

        CHECK(lst.size() == 3);
        CHECK(*it == 20);

        int expected[] = {10, 20, 30};
        int i = 0;
        for (int value : lst) {
            CHECK(value == expected[i++]);
        }
    }

    SUBCASE("Insert at end") {
        lst.push_back(10);
        lst.push_back(20);
        auto it = lst.insert(lst.end(), 30);

        CHECK(lst.size() == 3);
        CHECK(*it == 30);
        CHECK(lst.back() == 30);
    }

    SUBCASE("Erase at beginning") {
        lst.push_back(10);
        lst.push_back(20);
        lst.push_back(30);
        auto it = lst.erase(lst.begin());

        CHECK(lst.size() == 2);
        CHECK(*it == 20);
        CHECK(lst.front() == 20);
    }

    SUBCASE("Erase in middle") {
        lst.push_back(10);
        lst.push_back(20);
        lst.push_back(30);
        auto it = lst.begin();
        ++it;
        it = lst.erase(it);

        CHECK(lst.size() == 2);
        CHECK(*it == 30);
        CHECK(lst.front() == 10);
        CHECK(lst.back() == 30);
    }

    SUBCASE("Erase range") {
        lst.push_back(10);
        lst.push_back(20);
        lst.push_back(30);
        lst.push_back(40);

        auto first = lst.begin();
        ++first;
        auto last = first;
        ++last;
        ++last;

        lst.erase(first, last);

        CHECK(lst.size() == 2);
        CHECK(lst.front() == 10);
        CHECK(lst.back() == 40);
    }
}

TEST_CASE("list construction and destruction") {
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
        bool operator==(const TestObject& other) const { return value == other.value; }
    };

    SUBCASE("Construction and destruction") {
        live_object_count = 0;
        {
            fl::list<TestObject> lst;
            CHECK(live_object_count == 0);

            lst.push_back(TestObject(1));
            lst.push_back(TestObject(2));
            lst.push_back(TestObject(3));

            CHECK(live_object_count == 3);

            lst.pop_back();
            CHECK(live_object_count == 2);
        }
        CHECK(live_object_count == 0);
    }

    SUBCASE("Clear") {
        live_object_count = 0;
        {
            fl::list<TestObject> lst;
            lst.push_back(TestObject(1));
            lst.push_back(TestObject(2));

            CHECK(live_object_count == 2);

            lst.clear();

            CHECK(live_object_count == 0);
        }
        CHECK(live_object_count == 0);
    }
}

TEST_CASE("list with custom type") {
    struct Point {
        int x, y;
        Point(int x = 0, int y = 0) : x(x), y(y) {}
        bool operator==(const Point& other) const {
            return x == other.x && y == other.y;
        }
    };

    fl::list<Point> lst;

    SUBCASE("Push and access custom type") {
        lst.push_back(Point(1, 2));
        lst.push_back(Point(3, 4));

        CHECK(lst.size() == 2);
        CHECK(lst.front().x == 1);
        CHECK(lst.front().y == 2);
        CHECK(lst.back().x == 3);
        CHECK(lst.back().y == 4);
    }

    SUBCASE("Find custom type") {
        lst.push_back(Point(1, 2));
        lst.push_back(Point(3, 4));
        lst.push_back(Point(5, 6));

        auto it = lst.find(Point(3, 4));
        CHECK(it != lst.end());
        CHECK(it->x == 3);
        CHECK(it->y == 4);

        it = lst.find(Point(99, 99));
        CHECK(it == lst.end());
    }
}

TEST_CASE("list resize") {
    fl::list<int> lst;

    SUBCASE("Resize to larger size") {
        lst.push_back(10);
        lst.push_back(20);
        lst.resize(5);

        CHECK(lst.size() == 5);
        auto it = lst.begin();
        CHECK(*it == 10);
        ++it;
        CHECK(*it == 20);
        ++it;
        CHECK(*it == 0);
    }

    SUBCASE("Resize to smaller size") {
        lst.push_back(10);
        lst.push_back(20);
        lst.push_back(30);
        lst.push_back(40);
        lst.resize(2);

        CHECK(lst.size() == 2);
        CHECK(lst.front() == 10);
        CHECK(lst.back() == 20);
    }

    SUBCASE("Resize with value") {
        lst.push_back(10);
        lst.resize(4, 99);

        CHECK(lst.size() == 4);
        auto it = lst.begin();
        CHECK(*it == 10);
        ++it;
        CHECK(*it == 99);
        ++it;
        CHECK(*it == 99);
        ++it;
        CHECK(*it == 99);
    }
}

TEST_CASE("list remove operations") {
    fl::list<int> lst;

    SUBCASE("Remove single value") {
        lst.push_back(10);
        lst.push_back(20);
        lst.push_back(30);
        lst.push_back(20);
        lst.push_back(40);

        lst.remove(20);

        CHECK(lst.size() == 3);
        int expected[] = {10, 30, 40};
        int i = 0;
        for (int value : lst) {
            CHECK(value == expected[i++]);
        }
    }

    SUBCASE("Remove with predicate") {
        lst.push_back(1);
        lst.push_back(2);
        lst.push_back(3);
        lst.push_back(4);
        lst.push_back(5);

        lst.remove_if([](int n) { return n % 2 == 0; });

        CHECK(lst.size() == 3);
        int expected[] = {1, 3, 5};
        int i = 0;
        for (int value : lst) {
            CHECK(value == expected[i++]);
        }
    }
}

TEST_CASE("list reverse") {
    fl::list<int> lst;

    SUBCASE("Reverse empty list") {
        lst.reverse();
        CHECK(lst.empty());
    }

    SUBCASE("Reverse single element") {
        lst.push_back(10);
        lst.reverse();
        CHECK(lst.size() == 1);
        CHECK(lst.front() == 10);
    }

    SUBCASE("Reverse multiple elements") {
        lst.push_back(10);
        lst.push_back(20);
        lst.push_back(30);
        lst.push_back(40);

        lst.reverse();

        CHECK(lst.size() == 4);
        int expected[] = {40, 30, 20, 10};
        int i = 0;
        for (int value : lst) {
            CHECK(value == expected[i++]);
        }
    }
}

TEST_CASE("list unique") {
    fl::list<int> lst;

    SUBCASE("Unique removes consecutive duplicates") {
        lst.push_back(10);
        lst.push_back(10);
        lst.push_back(20);
        lst.push_back(20);
        lst.push_back(20);
        lst.push_back(30);

        lst.unique();

        CHECK(lst.size() == 3);
        int expected[] = {10, 20, 30};
        int i = 0;
        for (int value : lst) {
            CHECK(value == expected[i++]);
        }
    }

    SUBCASE("Unique on list with no duplicates") {
        lst.push_back(10);
        lst.push_back(20);
        lst.push_back(30);

        lst.unique();

        CHECK(lst.size() == 3);
    }
}

TEST_CASE("list sort") {
    fl::list<int> lst;

    SUBCASE("Sort empty list") {
        lst.sort();
        CHECK(lst.empty());
    }

    SUBCASE("Sort single element") {
        lst.push_back(10);
        lst.sort();
        CHECK(lst.size() == 1);
        CHECK(lst.front() == 10);
    }

    SUBCASE("Sort multiple elements") {
        lst.push_back(30);
        lst.push_back(10);
        lst.push_back(40);
        lst.push_back(20);

        lst.sort();

        CHECK(lst.size() == 4);
        int expected[] = {10, 20, 30, 40};
        int i = 0;
        for (int value : lst) {
            CHECK(value == expected[i++]);
        }
    }

    SUBCASE("Sort with custom comparator") {
        lst.push_back(10);
        lst.push_back(30);
        lst.push_back(20);
        lst.push_back(40);

        lst.sort([](int a, int b) { return a > b; }); // Descending order

        CHECK(lst.size() == 4);
        int expected[] = {40, 30, 20, 10};
        int i = 0;
        for (int value : lst) {
            CHECK(value == expected[i++]);
        }
    }
}

TEST_CASE("list splice") {
    SUBCASE("Splice entire list") {
        fl::list<int> lst1;
        lst1.push_back(10);
        lst1.push_back(20);

        fl::list<int> lst2;
        lst2.push_back(30);
        lst2.push_back(40);

        lst1.splice(lst1.end(), lst2);

        CHECK(lst1.size() == 4);
        CHECK(lst2.size() == 0);
        CHECK(lst2.empty());

        int expected[] = {10, 20, 30, 40};
        int i = 0;
        for (int value : lst1) {
            CHECK(value == expected[i++]);
        }
    }

    SUBCASE("Splice single element") {
        fl::list<int> lst1;
        lst1.push_back(10);
        lst1.push_back(30);

        fl::list<int> lst2;
        lst2.push_back(20);
        lst2.push_back(40);

        auto it = lst1.begin();
        ++it; // Point to 30

        lst1.splice(it, lst2, lst2.begin());

        CHECK(lst1.size() == 3);
        CHECK(lst2.size() == 1);

        int expected1[] = {10, 20, 30};
        int i = 0;
        for (int value : lst1) {
            CHECK(value == expected1[i++]);
        }
    }

    SUBCASE("Splice range") {
        fl::list<int> lst1;
        lst1.push_back(10);
        lst1.push_back(50);

        fl::list<int> lst2;
        lst2.push_back(20);
        lst2.push_back(30);
        lst2.push_back(40);
        lst2.push_back(60);

        auto it = lst1.begin();
        ++it; // Point to 50

        auto first = lst2.begin();
        ++first; // Point to 30
        auto last = first;
        ++last;
        ++last; // Point to 60

        lst1.splice(it, lst2, first, last);

        CHECK(lst1.size() == 4);
        CHECK(lst2.size() == 2);

        int expected1[] = {10, 30, 40, 50};
        int i = 0;
        for (int value : lst1) {
            CHECK(value == expected1[i++]);
        }

        int expected2[] = {20, 60};
        i = 0;
        for (int value : lst2) {
            CHECK(value == expected2[i++]);
        }
    }
}

TEST_CASE("list copy and move") {
    SUBCASE("Copy constructor") {
        fl::list<int> lst1;
        lst1.push_back(10);
        lst1.push_back(20);
        lst1.push_back(30);

        fl::list<int> lst2(lst1);

        CHECK(lst2.size() == 3);
        CHECK(lst1.size() == 3);

        int expected[] = {10, 20, 30};
        int i = 0;
        for (int value : lst2) {
            CHECK(value == expected[i++]);
        }
    }

    SUBCASE("Copy assignment") {
        fl::list<int> lst1;
        lst1.push_back(10);
        lst1.push_back(20);

        fl::list<int> lst2;
        lst2.push_back(99);

        lst2 = lst1;

        CHECK(lst2.size() == 2);
        CHECK(lst1.size() == 2);
        CHECK(lst2.front() == 10);
        CHECK(lst2.back() == 20);
    }

    SUBCASE("Move constructor") {
        fl::list<int> lst1;
        lst1.push_back(10);
        lst1.push_back(20);
        lst1.push_back(30);

        fl::list<int> lst2(fl::move(lst1));

        CHECK(lst2.size() == 3);
        CHECK(lst1.size() == 0); // lst1 should be empty after move
        CHECK(lst1.empty());

        int expected[] = {10, 20, 30};
        int i = 0;
        for (int value : lst2) {
            CHECK(value == expected[i++]);
        }
    }

    SUBCASE("Move assignment") {
        fl::list<int> lst1;
        lst1.push_back(10);
        lst1.push_back(20);

        fl::list<int> lst2;
        lst2.push_back(99);

        lst2 = fl::move(lst1);

        CHECK(lst2.size() == 2);
        CHECK(lst1.size() == 0);
        CHECK(lst1.empty());
        CHECK(lst2.front() == 10);
        CHECK(lst2.back() == 20);
    }
}

TEST_CASE("list swap") {
    fl::list<int> lst1;
    lst1.push_back(10);
    lst1.push_back(20);

    fl::list<int> lst2;
    lst2.push_back(30);
    lst2.push_back(40);
    lst2.push_back(50);

    lst1.swap(lst2);

    CHECK(lst1.size() == 3);
    CHECK(lst2.size() == 2);

    CHECK(lst1.front() == 30);
    CHECK(lst2.front() == 10);
}

TEST_CASE("list initializer list constructor") {
    SUBCASE("Basic initializer list") {
        fl::list<int> lst{10, 20, 30, 40};

        CHECK(lst.size() == 4);
        CHECK(lst.front() == 10);
        CHECK(lst.back() == 40);

        int expected[] = {10, 20, 30, 40};
        int i = 0;
        for (int value : lst) {
            CHECK(value == expected[i++]);
        }
    }

    SUBCASE("Empty initializer list") {
        fl::list<int> lst{};

        CHECK(lst.size() == 0);
        CHECK(lst.empty());
    }
}

TEST_CASE("list count constructor") {
    SUBCASE("Construct with count and value") {
        fl::list<int> lst(5, 42);

        CHECK(lst.size() == 5);
        for (int value : lst) {
            CHECK(value == 42);
        }
    }

    SUBCASE("Construct with count only (default value)") {
        fl::list<int> lst(3);

        CHECK(lst.size() == 3);
        for (int value : lst) {
            CHECK(value == 0);
        }
    }
}

TEST_CASE("list has and find") {
    fl::list<int> lst;
    lst.push_back(10);
    lst.push_back(20);
    lst.push_back(30);

    SUBCASE("Find existing element") {
        auto it = lst.find(20);
        CHECK(it != lst.end());
        CHECK(*it == 20);
    }

    SUBCASE("Find non-existing element") {
        auto it = lst.find(99);
        CHECK(it == lst.end());
    }

    SUBCASE("Has existing element") {
        CHECK(lst.has(20));
    }

    SUBCASE("Has non-existing element") {
        CHECK_FALSE(lst.has(99));
    }
}
