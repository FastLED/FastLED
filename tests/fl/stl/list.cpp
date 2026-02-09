#include "fl/stl/list.h"
#include "fl/stl/new.h"
#include "test.h"
#include "fl/stl/move.h"

using namespace fl;

FL_TEST_CASE("fl::list - default constructor") {
    list<int> lst;

    FL_CHECK(lst.empty());
    FL_CHECK_EQ(lst.size(), 0);
    FL_CHECK(lst.begin() == lst.end());
}

FL_TEST_CASE("fl::list - constructor with count and value") {
    FL_SUBCASE("create with count") {
        list<int> lst(5, 42);

        FL_CHECK_FALSE(lst.empty());
        FL_CHECK_EQ(lst.size(), 5);

        for (const auto& val : lst) {
            FL_CHECK_EQ(val, 42);
        }
    }

    FL_SUBCASE("create with zero count") {
        list<int> lst(0, 10);
        FL_CHECK(lst.empty());
        FL_CHECK_EQ(lst.size(), 0);
    }
}

FL_TEST_CASE("fl::list - initializer list constructor") {
    list<int> lst = {1, 2, 3, 4, 5};

    FL_CHECK_EQ(lst.size(), 5);
    FL_CHECK_EQ(lst.front(), 1);
    FL_CHECK_EQ(lst.back(), 5);

    auto it = lst.begin();
    FL_CHECK_EQ(*it++, 1);
    FL_CHECK_EQ(*it++, 2);
    FL_CHECK_EQ(*it++, 3);
    FL_CHECK_EQ(*it++, 4);
    FL_CHECK_EQ(*it++, 5);
    FL_CHECK(it == lst.end());
}

FL_TEST_CASE("fl::list - copy constructor") {
    list<int> original = {10, 20, 30};
    list<int> copy(original);

    FL_CHECK_EQ(copy.size(), original.size());
    FL_CHECK_EQ(copy.front(), original.front());
    FL_CHECK_EQ(copy.back(), original.back());

    auto it1 = original.begin();
    auto it2 = copy.begin();
    while (it1 != original.end()) {
        FL_CHECK_EQ(*it1, *it2);
        ++it1;
        ++it2;
    }
}

FL_TEST_CASE("fl::list - move constructor") {
    list<int> original = {10, 20, 30};
    list<int> moved(fl::move(original));

    FL_CHECK_EQ(moved.size(), 3);
    FL_CHECK_EQ(moved.front(), 10);
    FL_CHECK_EQ(moved.back(), 30);
}

FL_TEST_CASE("fl::list - copy assignment") {
    list<int> original = {1, 2, 3};
    list<int> assigned;

    assigned = original;

    FL_CHECK_EQ(assigned.size(), original.size());
    auto it1 = original.begin();
    auto it2 = assigned.begin();
    while (it1 != original.end()) {
        FL_CHECK_EQ(*it1, *it2);
        ++it1;
        ++it2;
    }
}

FL_TEST_CASE("fl::list - move assignment") {
    list<int> original = {1, 2, 3};
    list<int> assigned;

    assigned = fl::move(original);

    FL_CHECK_EQ(assigned.size(), 3);
    FL_CHECK_EQ(assigned.front(), 1);
    FL_CHECK_EQ(assigned.back(), 3);
}

FL_TEST_CASE("fl::list - push_back and push_front") {
    list<int> lst;

    lst.push_back(10);
    FL_CHECK_EQ(lst.size(), 1);
    FL_CHECK_EQ(lst.front(), 10);
    FL_CHECK_EQ(lst.back(), 10);

    lst.push_back(20);
    FL_CHECK_EQ(lst.size(), 2);
    FL_CHECK_EQ(lst.front(), 10);
    FL_CHECK_EQ(lst.back(), 20);

    lst.push_front(5);
    FL_CHECK_EQ(lst.size(), 3);
    FL_CHECK_EQ(lst.front(), 5);
    FL_CHECK_EQ(lst.back(), 20);

    // Check order: 5, 10, 20
    auto it = lst.begin();
    FL_CHECK_EQ(*it++, 5);
    FL_CHECK_EQ(*it++, 10);
    FL_CHECK_EQ(*it++, 20);
}

FL_TEST_CASE("fl::list - pop_back and pop_front") {
    list<int> lst = {1, 2, 3, 4, 5};

    lst.pop_back();
    FL_CHECK_EQ(lst.size(), 4);
    FL_CHECK_EQ(lst.back(), 4);

    lst.pop_front();
    FL_CHECK_EQ(lst.size(), 3);
    FL_CHECK_EQ(lst.front(), 2);

    // Order: 2, 3, 4
    auto it = lst.begin();
    FL_CHECK_EQ(*it++, 2);
    FL_CHECK_EQ(*it++, 3);
    FL_CHECK_EQ(*it++, 4);
}

FL_TEST_CASE("fl::list - pop on empty list") {
    list<int> lst;

    lst.pop_back();  // Should not crash
    FL_CHECK(lst.empty());

    lst.pop_front();  // Should not crash
    FL_CHECK(lst.empty());
}

FL_TEST_CASE("fl::list - insert") {
    list<int> lst = {1, 3, 5};

    FL_SUBCASE("insert in middle") {
        auto it = lst.begin();
        ++it;  // Points to 3
        lst.insert(it, 2);

        FL_CHECK_EQ(lst.size(), 4);
        auto check = lst.begin();
        FL_CHECK_EQ(*check++, 1);
        FL_CHECK_EQ(*check++, 2);
        FL_CHECK_EQ(*check++, 3);
        FL_CHECK_EQ(*check++, 5);
    }

    FL_SUBCASE("insert at beginning") {
        lst.insert(lst.begin(), 0);
        FL_CHECK_EQ(lst.size(), 4);
        FL_CHECK_EQ(lst.front(), 0);
    }

    FL_SUBCASE("insert at end") {
        lst.insert(lst.end(), 10);
        FL_CHECK_EQ(lst.size(), 4);
        FL_CHECK_EQ(lst.back(), 10);
    }
}

FL_TEST_CASE("fl::list - erase") {
    FL_SUBCASE("erase single element") {
        list<int> lst = {1, 2, 3, 4, 5};
        auto it = lst.begin();
        ++it;
        ++it;  // Points to 3

        auto next = lst.erase(it);
        FL_CHECK_EQ(lst.size(), 4);
        FL_CHECK_EQ(*next, 4);

        auto check = lst.begin();
        FL_CHECK_EQ(*check++, 1);
        FL_CHECK_EQ(*check++, 2);
        FL_CHECK_EQ(*check++, 4);
        FL_CHECK_EQ(*check++, 5);
    }

    FL_SUBCASE("erase range") {
        list<int> lst = {1, 2, 3, 4, 5};
        auto first = lst.begin();
        ++first;  // Points to 2
        auto last = first;
        ++last;
        ++last;
        ++last;  // Points to 5

        lst.erase(first, last);
        FL_CHECK_EQ(lst.size(), 2);

        auto check = lst.begin();
        FL_CHECK_EQ(*check++, 1);
        FL_CHECK_EQ(*check++, 5);
    }
}

FL_TEST_CASE("fl::list - clear") {
    list<int> lst = {1, 2, 3, 4, 5};

    lst.clear();
    FL_CHECK(lst.empty());
    FL_CHECK_EQ(lst.size(), 0);
    FL_CHECK(lst.begin() == lst.end());
}

FL_TEST_CASE("fl::list - front and back") {
    list<int> lst = {10, 20, 30};

    FL_CHECK_EQ(lst.front(), 10);
    FL_CHECK_EQ(lst.back(), 30);

    lst.front() = 15;
    lst.back() = 35;

    FL_CHECK_EQ(lst.front(), 15);
    FL_CHECK_EQ(lst.back(), 35);
}

FL_TEST_CASE("fl::list - iterators") {
    list<int> lst = {1, 2, 3, 4, 5};

    FL_SUBCASE("forward iteration") {
        int expected = 1;
        for (auto it = lst.begin(); it != lst.end(); ++it) {
            FL_CHECK_EQ(*it, expected++);
        }
    }

    FL_SUBCASE("const forward iteration") {
        const list<int>& const_lst = lst;
        int expected = 1;
        for (auto it = const_lst.begin(); it != const_lst.end(); ++it) {
            FL_CHECK_EQ(*it, expected++);
        }
    }

    FL_SUBCASE("iterator increment and decrement") {
        auto it = lst.begin();
        FL_CHECK_EQ(*it, 1);

        ++it;
        FL_CHECK_EQ(*it, 2);

        ++it;
        FL_CHECK_EQ(*it, 3);

        --it;
        FL_CHECK_EQ(*it, 2);
    }

    FL_SUBCASE("post-increment") {
        auto it = lst.begin();
        auto old = it++;
        FL_CHECK_EQ(*old, 1);
        FL_CHECK_EQ(*it, 2);
    }

    FL_SUBCASE("post-decrement") {
        auto it = lst.end();
        --it;
        auto old = it--;
        FL_CHECK_EQ(*old, 5);
        FL_CHECK_EQ(*it, 4);
    }
}

FL_TEST_CASE("fl::list - resize") {
    FL_SUBCASE("resize larger") {
        list<int> lst = {1, 2, 3};
        lst.resize(5);

        FL_CHECK_EQ(lst.size(), 5);
        auto it = lst.begin();
        FL_CHECK_EQ(*it++, 1);
        FL_CHECK_EQ(*it++, 2);
        FL_CHECK_EQ(*it++, 3);
        FL_CHECK_EQ(*it++, 0);  // Default constructed
        FL_CHECK_EQ(*it++, 0);  // Default constructed
    }

    FL_SUBCASE("resize larger with value") {
        list<int> lst = {1, 2, 3};
        lst.resize(5, 42);

        FL_CHECK_EQ(lst.size(), 5);
        auto it = lst.begin();
        FL_CHECK_EQ(*it++, 1);
        FL_CHECK_EQ(*it++, 2);
        FL_CHECK_EQ(*it++, 3);
        FL_CHECK_EQ(*it++, 42);
        FL_CHECK_EQ(*it++, 42);
    }

    FL_SUBCASE("resize smaller") {
        list<int> lst = {1, 2, 3, 4, 5};
        lst.resize(3);

        FL_CHECK_EQ(lst.size(), 3);
        auto it = lst.begin();
        FL_CHECK_EQ(*it++, 1);
        FL_CHECK_EQ(*it++, 2);
        FL_CHECK_EQ(*it++, 3);
        FL_CHECK(it == lst.end());
    }
}

FL_TEST_CASE("fl::list - swap") {
    list<int> lst1 = {1, 2, 3};
    list<int> lst2 = {10, 20};

    lst1.swap(lst2);

    FL_CHECK_EQ(lst1.size(), 2);
    FL_CHECK_EQ(lst1.front(), 10);
    FL_CHECK_EQ(lst1.back(), 20);

    FL_CHECK_EQ(lst2.size(), 3);
    FL_CHECK_EQ(lst2.front(), 1);
    FL_CHECK_EQ(lst2.back(), 3);
}

FL_TEST_CASE("fl::list - remove") {
    list<int> lst = {1, 2, 3, 2, 4, 2, 5};

    lst.remove(2);

    FL_CHECK_EQ(lst.size(), 4);
    auto it = lst.begin();
    FL_CHECK_EQ(*it++, 1);
    FL_CHECK_EQ(*it++, 3);
    FL_CHECK_EQ(*it++, 4);
    FL_CHECK_EQ(*it++, 5);
}

FL_TEST_CASE("fl::list - remove_if") {
    list<int> lst = {1, 2, 3, 4, 5, 6, 7, 8};

    // Remove all even numbers
    lst.remove_if([](int x) { return x % 2 == 0; });

    FL_CHECK_EQ(lst.size(), 4);
    auto it = lst.begin();
    FL_CHECK_EQ(*it++, 1);
    FL_CHECK_EQ(*it++, 3);
    FL_CHECK_EQ(*it++, 5);
    FL_CHECK_EQ(*it++, 7);
}

FL_TEST_CASE("fl::list - reverse") {
    FL_SUBCASE("reverse multiple elements") {
        list<int> lst = {1, 2, 3, 4, 5};
        lst.reverse();

        FL_CHECK_EQ(lst.size(), 5);
        auto it = lst.begin();
        FL_CHECK_EQ(*it++, 5);
        FL_CHECK_EQ(*it++, 4);
        FL_CHECK_EQ(*it++, 3);
        FL_CHECK_EQ(*it++, 2);
        FL_CHECK_EQ(*it++, 1);
    }

    FL_SUBCASE("reverse single element") {
        list<int> lst = {42};
        lst.reverse();
        FL_CHECK_EQ(lst.size(), 1);
        FL_CHECK_EQ(lst.front(), 42);
    }

    FL_SUBCASE("reverse empty list") {
        list<int> lst;
        lst.reverse();
        FL_CHECK(lst.empty());
    }
}

FL_TEST_CASE("fl::list - unique") {
    FL_SUBCASE("remove consecutive duplicates") {
        list<int> lst = {1, 1, 2, 2, 2, 3, 3, 4, 5, 5};
        lst.unique();

        FL_CHECK_EQ(lst.size(), 5);
        auto it = lst.begin();
        FL_CHECK_EQ(*it++, 1);
        FL_CHECK_EQ(*it++, 2);
        FL_CHECK_EQ(*it++, 3);
        FL_CHECK_EQ(*it++, 4);
        FL_CHECK_EQ(*it++, 5);
    }

    FL_SUBCASE("no duplicates") {
        list<int> lst = {1, 2, 3, 4, 5};
        lst.unique();
        FL_CHECK_EQ(lst.size(), 5);
    }

    FL_SUBCASE("all duplicates") {
        list<int> lst = {7, 7, 7, 7};
        lst.unique();
        FL_CHECK_EQ(lst.size(), 1);
        FL_CHECK_EQ(lst.front(), 7);
    }
}

FL_TEST_CASE("fl::list - sort") {
    FL_SUBCASE("sort unsorted list") {
        list<int> lst = {5, 2, 8, 1, 9, 3};
        lst.sort();

        FL_CHECK_EQ(lst.size(), 6);
        auto it = lst.begin();
        FL_CHECK_EQ(*it++, 1);
        FL_CHECK_EQ(*it++, 2);
        FL_CHECK_EQ(*it++, 3);
        FL_CHECK_EQ(*it++, 5);
        FL_CHECK_EQ(*it++, 8);
        FL_CHECK_EQ(*it++, 9);
    }

    FL_SUBCASE("sort with custom comparator") {
        list<int> lst = {1, 5, 3, 9, 2};
        lst.sort([](int a, int b) { return a > b; });  // Descending order

        auto it = lst.begin();
        FL_CHECK_EQ(*it++, 9);
        FL_CHECK_EQ(*it++, 5);
        FL_CHECK_EQ(*it++, 3);
        FL_CHECK_EQ(*it++, 2);
        FL_CHECK_EQ(*it++, 1);
    }

    FL_SUBCASE("sort already sorted") {
        list<int> lst = {1, 2, 3, 4, 5};
        lst.sort();

        auto it = lst.begin();
        FL_CHECK_EQ(*it++, 1);
        FL_CHECK_EQ(*it++, 2);
        FL_CHECK_EQ(*it++, 3);
        FL_CHECK_EQ(*it++, 4);
        FL_CHECK_EQ(*it++, 5);
    }
}

FL_TEST_CASE("fl::list - splice") {
    FL_SUBCASE("splice entire list") {
        list<int> lst1 = {1, 2, 3};
        list<int> lst2 = {10, 20, 30};

        auto it = lst1.begin();
        ++it;  // Points to 2
        lst1.splice(it, lst2);

        FL_CHECK_EQ(lst1.size(), 6);
        FL_CHECK_EQ(lst2.size(), 0);

        auto check = lst1.begin();
        FL_CHECK_EQ(*check++, 1);
        FL_CHECK_EQ(*check++, 10);
        FL_CHECK_EQ(*check++, 20);
        FL_CHECK_EQ(*check++, 30);
        FL_CHECK_EQ(*check++, 2);
        FL_CHECK_EQ(*check++, 3);
    }

    FL_SUBCASE("splice single element") {
        list<int> lst1 = {1, 2, 3};
        list<int> lst2 = {10, 20, 30};

        auto src = lst2.begin();
        ++src;  // Points to 20

        lst1.splice(lst1.end(), lst2, src);

        FL_CHECK_EQ(lst1.size(), 4);
        FL_CHECK_EQ(lst2.size(), 2);

        FL_CHECK_EQ(lst1.back(), 20);
    }

    FL_SUBCASE("splice range") {
        list<int> lst1 = {1, 2, 3};
        list<int> lst2 = {10, 20, 30, 40};

        auto first = lst2.begin();
        ++first;  // Points to 20
        auto last = first;
        ++last;
        ++last;  // Points to 40

        lst1.splice(lst1.end(), lst2, first, last);

        FL_CHECK_EQ(lst1.size(), 5);
        FL_CHECK_EQ(lst2.size(), 2);

        auto check = lst1.begin();
        FL_CHECK_EQ(*check++, 1);
        FL_CHECK_EQ(*check++, 2);
        FL_CHECK_EQ(*check++, 3);
        FL_CHECK_EQ(*check++, 20);
        FL_CHECK_EQ(*check++, 30);
    }
}

FL_TEST_CASE("fl::list - find") {
    list<int> lst = {10, 20, 30, 40, 50};

    FL_SUBCASE("find existing element") {
        auto it = lst.find(30);
        FL_CHECK(it != lst.end());
        FL_CHECK_EQ(*it, 30);
    }

    FL_SUBCASE("find non-existing element") {
        auto it = lst.find(99);
        FL_CHECK(it == lst.end());
    }

    FL_SUBCASE("find first element") {
        auto it = lst.find(10);
        FL_CHECK(it != lst.end());
        FL_CHECK_EQ(*it, 10);
        FL_CHECK(it == lst.begin());
    }

    FL_SUBCASE("find last element") {
        auto it = lst.find(50);
        FL_CHECK(it != lst.end());
        FL_CHECK_EQ(*it, 50);
    }
}

FL_TEST_CASE("fl::list - has") {
    list<int> lst = {10, 20, 30};

    FL_CHECK(lst.has(10));
    FL_CHECK(lst.has(20));
    FL_CHECK(lst.has(30));
    FL_CHECK_FALSE(lst.has(40));
    FL_CHECK_FALSE(lst.has(0));
}

FL_TEST_CASE("fl::list - with non-POD types") {
    struct TestStruct {
        int value;
        TestStruct(int v) : value(v) {}
        TestStruct(const TestStruct&) = default;
        TestStruct& operator=(const TestStruct&) = default;
        bool operator==(const TestStruct& other) const {
            return value == other.value;
        }
    };

    list<TestStruct> lst;
    lst.push_back(TestStruct(10));
    lst.push_back(TestStruct(20));
    lst.push_back(TestStruct(30));

    FL_CHECK_EQ(lst.size(), 3);
    FL_CHECK_EQ(lst.front().value, 10);
    FL_CHECK_EQ(lst.back().value, 30);

    FL_CHECK(lst.has(TestStruct(20)));
    FL_CHECK_FALSE(lst.has(TestStruct(99)));
}

FL_TEST_CASE("fl::list - range-based for loop") {
    list<int> lst = {1, 2, 3, 4, 5};

    int sum = 0;
    for (int val : lst) {
        sum += val;
    }
    FL_CHECK_EQ(sum, 15);

    // Modify through non-const reference
    for (int& val : lst) {
        val *= 2;
    }

    auto it = lst.begin();
    FL_CHECK_EQ(*it++, 2);
    FL_CHECK_EQ(*it++, 4);
    FL_CHECK_EQ(*it++, 6);
    FL_CHECK_EQ(*it++, 8);
    FL_CHECK_EQ(*it++, 10);
}

FL_TEST_CASE("fl::list - large list operations") {
    list<int> lst;
    const int count = 1000;

    // Push many elements
    for (int i = 0; i < count; ++i) {
        lst.push_back(i);
    }

    FL_CHECK_EQ(lst.size(), count);
    FL_CHECK_EQ(lst.front(), 0);
    FL_CHECK_EQ(lst.back(), count - 1);

    // Remove half of them
    lst.remove_if([](int x) { return x % 2 == 0; });
    FL_CHECK_EQ(lst.size(), count / 2);

    // Clear all
    lst.clear();
    FL_CHECK(lst.empty());
}
