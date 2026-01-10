// Tests for fl/set.h
// Combined from test_fixed_set.cpp and test_set_inlined.cpp

#include "fl/stl/set.h"
#include "fl/stl/bit_cast.h"
#include "fl/stl/new.h"
#include "doctest.h"
#include "fl/log.h"
#include "fl/stl/move.h"
#include "fl/stl/pair.h"
#include "fl/stl/strstream.h"
#include "fl/stl/type_traits.h"
#include "fl/int.h"

using namespace fl;

// ========================================
// Tests from test_fixed_set.cpp
// ========================================

TEST_CASE("FixedSet operations") {
    fl::FixedSet<int, 5> set;

    SUBCASE("Insert and find") {
        CHECK(set.insert(1));
        CHECK(set.insert(2));
        CHECK(set.insert(3));
        CHECK(set.find(1) != set.end());
        CHECK(set.find(2) != set.end());
        CHECK(set.find(3) != set.end());
        CHECK(set.find(4) == set.end());
        CHECK_FALSE(set.insert(1)); // Duplicate insert should fail
    }

    SUBCASE("Erase") {
        CHECK(set.insert(1));
        CHECK(set.insert(2));
        CHECK(set.erase(1));
        CHECK(set.find(1) == set.end());
        CHECK(set.find(2) != set.end());
        CHECK_FALSE(set.erase(3)); // Erasing non-existent element should fail
    }

    SUBCASE("Next and prev") {
        CHECK(set.insert(1));
        CHECK(set.insert(2));
        CHECK(set.insert(3));

        int next_value;
        CHECK(set.next(1, &next_value));
        CHECK(next_value == 2);
        CHECK(set.next(3, &next_value, true));
        CHECK(next_value == 1);

        int prev_value;
        CHECK(set.prev(3, &prev_value));
        CHECK(prev_value == 2);
        CHECK(set.prev(1, &prev_value, true));
        CHECK(prev_value == 3);
    }

    SUBCASE("Size and capacity") {
        CHECK(set.size() == 0);
        CHECK(set.capacity() == 5);
        CHECK(set.empty());

        set.insert(1);
        set.insert(2);
        CHECK(set.size() == 2);
        CHECK_FALSE(set.empty());

        set.clear();
        CHECK(set.size() == 0);
        CHECK(set.empty());
    }

    SUBCASE("Iterators") {
        set.insert(1);
        set.insert(2);
        set.insert(3);

        int sum = 0;
        for (const auto& value : set) {
            sum += value;
        }
        CHECK(sum == 6);

        auto it = set.begin();
        CHECK(*it == 1);
        ++it;
        CHECK(*it == 2);
        ++it;
        CHECK(*it == 3);
        ++it;
        CHECK(it == set.end());
    }

    SUBCASE("Front and back") {
        set.insert(1);
        set.insert(2);
        set.insert(3);

        CHECK(set.front() == 1);
        CHECK(set.back() == 3);
    }
}

// ========================================
// Tests from test_set_inlined.cpp
// ========================================

TEST_CASE("fl::set_inlined - Basic functionality") {

    SUBCASE("Empty set") {
        fl::set_inlined<int, 5> set;

        CHECK(set.empty());
        CHECK(set.size() == 0);
    }

    SUBCASE("Set has inlined elements") {
        fl::set_inlined<int, 5> set;
        fl::uptr ptr_begin = fl::ptr_to_int(&set);
        fl::uptr ptr_end = ptr_begin + sizeof(set);

        set.insert(1);
        set.insert(2);
        set.insert(3);
        set.insert(4);
        set.insert(5);

        // now make sure that the element addresses are in the right place
        for (auto it = set.begin(); it != set.end(); ++it) {
            fl::uptr ptr = fl::ptr_to_int(&*it);
            CHECK_GE(ptr, ptr_begin);
            CHECK_LT(ptr, ptr_end);
        }
    }

    SUBCASE("Single element insertion") {
        fl::set_inlined<int, 5> set;
        auto result = set.insert(42);

        CHECK(result.second); // Insertion successful
        CHECK(set.size() == 1);
        CHECK(set.contains(42));
    }

    SUBCASE("Multiple elements within inlined size") {
        fl::set_inlined<int, 5> set;

        // Insert exactly 5 elements (the inlined size)
        CHECK(set.insert(1).second);
        CHECK(set.insert(2).second);
        CHECK(set.insert(3).second);
        CHECK(set.insert(4).second);
        CHECK(set.insert(5).second);

        CHECK(set.size() == 5);
        CHECK(set.contains(1));
        CHECK(set.contains(2));
        CHECK(set.contains(3));
        CHECK(set.contains(4));
        CHECK(set.contains(5));
    }

    SUBCASE("Duplicate insertions") {
        fl::set_inlined<int, 3> set;

        CHECK(set.insert(10).second);
        CHECK(set.insert(20).second);
        CHECK_FALSE(set.insert(10).second); // Duplicate should fail

        CHECK(set.size() == 2); // Only unique elements
        CHECK(set.contains(10));
        CHECK(set.contains(20));
    }

    SUBCASE("Element removal") {
        fl::set_inlined<int, 4> set;

        set.insert(100);
        set.insert(200);
        set.insert(300);

        CHECK(set.size() == 3);

        CHECK(set.erase(200) == 1);

        CHECK(set.size() == 2);
        CHECK(set.contains(100));
        CHECK_FALSE(set.contains(200));
        CHECK(set.contains(300));
    }

    SUBCASE("Clear operation") {
        fl::set_inlined<int, 3> set;

        set.insert(1);
        set.insert(2);
        set.insert(3);

        CHECK(set.size() == 3);

        set.clear();

        CHECK(set.empty());
        CHECK(set.size() == 0);
    }

    SUBCASE("Emplace operation") {
        fl::set_inlined<int, 3> set;

        CHECK(set.emplace(42).second);
        CHECK(set.emplace(100).second);
        CHECK(set.emplace(200).second);

        CHECK(set.size() == 3);
        CHECK(set.contains(42));
        CHECK(set.contains(100));
        CHECK(set.contains(200));
    }

    SUBCASE("Iterator operations") {
        fl::set_inlined<int, 3> set;
        set.insert(1);
        set.insert(2);
        set.insert(3);

        // Test iteration
        int count = 0;
        for (auto it = set.begin(); it != set.end(); ++it) {
            count++;
        }
        CHECK(count == 3);

        // Test const iteration
        const auto& const_set = set;
        count = 0;
        for (auto it = const_set.begin(); it != const_set.end(); ++it) {
            count++;
        }
        CHECK(count == 3);
    }

    SUBCASE("Find operations") {
        fl::set_inlined<int, 3> set;
        set.insert(10);
        set.insert(20);
        set.insert(30);

        auto it1 = set.find(20);
        CHECK(it1 != set.end());
        CHECK(*it1 == 20);

        auto it2 = set.find(99);
        CHECK(it2 == set.end());
    }

    SUBCASE("Count operations") {
        fl::set_inlined<int, 3> set;
        set.insert(1);
        set.insert(2);
        set.insert(3);

        CHECK(set.count(1) == 1);
        CHECK(set.count(2) == 1);
        CHECK(set.count(3) == 1);
        CHECK(set.count(99) == 0);
    }

    SUBCASE("Contains operations") {
        fl::set_inlined<int, 3> set;
        set.insert(1);
        set.insert(2);
        set.insert(3);

        CHECK(set.contains(1));
        CHECK(set.contains(2));
        CHECK(set.contains(3));
        CHECK_FALSE(set.contains(99));
    }

    SUBCASE("Custom type with inlined storage") {
        struct TestStruct {
            int value;
            TestStruct(int v = 0) : value(v) {}
            bool operator<(const TestStruct& other) const { return value < other.value; }
            bool operator==(const TestStruct& other) const { return value == other.value; }
        };

        fl::set_inlined<TestStruct, 3> set;

        CHECK(set.insert(TestStruct(1)).second);
        CHECK(set.insert(TestStruct(2)).second);
        CHECK(set.insert(TestStruct(3)).second);

        CHECK(set.size() == 3);
        CHECK(set.contains(TestStruct(1)));
        CHECK(set.contains(TestStruct(2)));
        CHECK(set.contains(TestStruct(3)));
    }
}

TEST_CASE("fl::set_inlined - Exceeding inlined size") {

    SUBCASE("Exceeding inlined size") {
        fl::set_inlined<int, 2> set;

        // Insert within inlined size
        CHECK(set.insert(1).second);
        CHECK(set.insert(2).second);

        // Insert beyond inlined size
        CHECK(set.insert(3).second);

        CHECK(set.size() == 3);
        CHECK(set.contains(1));
        CHECK(set.contains(2));
        CHECK(set.contains(3));
    }

    SUBCASE("Heap overflow") {
        fl::set_inlined<int, 3> set;

        // Insert more than inlined capacity but not too many
        for (int i = 0; i < 5; ++i) {
            auto result = set.insert(i);
            FL_WARN("Insert " << i << ": success=" << result.second << ", size=" << set.size());
        }

        CHECK(set.size() == 5);

        // Debug: print all elements in the set
        FL_WARN("Elements in set:");
        for (auto it = set.begin(); it != set.end(); ++it) {
            FL_WARN("  " << *it);
        }

        // Verify all elements are present
        for (int i = 0; i < 5; ++i) {
            bool contains = set.contains(i);
            FL_WARN("Contains " << i << ": " << (contains ? "true" : "false"));
            CHECK(contains);
        }
    }
}
