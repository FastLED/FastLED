// g++ --std=c++11 test_fixed_map.cpp -I../src

#include "test.h"
#include "test.h"
#include "fl/set.h"

#include "fl/namespace.h"
FASTLED_USING_NAMESPACE


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
