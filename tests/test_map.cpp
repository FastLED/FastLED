// g++ --std=c++11 test_fixed_map.cpp -I../src

#include "test.h"
#include "test.h"
#include "fl/map.h"
#include <string>

#include "fl/namespace.h"
FASTLED_USING_NAMESPACE

TEST_CASE("fl::FixedMap operations") {
    fl::FixedMap<int, int, 5> map;

    SUBCASE("Insert and find") {
        CHECK(map.insert(1, 10).first);
        CHECK(map.insert(2, 20).first);
        CHECK(map.insert(3, 30).first);

        int value;
        CHECK(map.get(1, &value));
        CHECK(value == 10);
        CHECK(map.get(2, &value));
        CHECK(value == 20);
        CHECK(map.get(3, &value));
        CHECK(value == 30);
        CHECK_FALSE(map.get(4, &value));
    }

    SUBCASE("Update") {
        CHECK(map.insert(1, 10).first);
        CHECK(map.update(1, 15));
        int value;
        CHECK(map.get(1, &value));
        CHECK(value == 15);

        CHECK(map.update(2, 20, true));  // Insert if missing
        CHECK(map.get(2, &value));
        CHECK(value == 20);

        CHECK_FALSE(map.update(3, 30, false));  // Don't insert if missing
        CHECK_FALSE(map.get(3, &value));
    }

    SUBCASE("Next and prev") {
        CHECK(map.insert(1, 10).first);
        CHECK(map.insert(2, 20).first);
        CHECK(map.insert(3, 30).first);

        int next_key;
        CHECK(map.next(1, &next_key));
        CHECK(next_key == 2);
        CHECK(map.next(2, &next_key));
        CHECK(next_key == 3);
        CHECK_FALSE(map.next(3, &next_key));
        CHECK(map.next(3, &next_key, true));  // With rollover
        CHECK(next_key == 1);

        int prev_key;
        CHECK(map.prev(3, &prev_key));
        CHECK(prev_key == 2);
        CHECK(map.prev(2, &prev_key));
        CHECK(prev_key == 1);
        CHECK_FALSE(map.prev(1, &prev_key));
        CHECK(map.prev(1, &prev_key, true));  // With rollover
        CHECK(prev_key == 3);

        // Additional test for prev on first element with rollover
        CHECK(map.prev(1, &prev_key, true));
        CHECK(prev_key == 3);
    }

    SUBCASE("Size and capacity") {
        CHECK(map.size() == 0);
        CHECK(map.capacity() == 5);
        CHECK(map.empty());

        CHECK(map.insert(1, 10).first);
        CHECK(map.insert(2, 20).first);
        CHECK(map.size() == 2);
        CHECK_FALSE(map.empty());

        map.clear();
        CHECK(map.size() == 0);
        CHECK(map.empty());
    }

    SUBCASE("Iterators") {
        CHECK(map.insert(1, 10).first);
        CHECK(map.insert(2, 20).first);
        CHECK(map.insert(3, 30).first);

        int sum = 0;
        for (const auto& pair : map) {
            sum += pair.second;
        }
        CHECK(sum == 60);
    }

    SUBCASE("Operator[]") {
        CHECK(map[1] == 0);  // Default value
        CHECK(!map.insert(1, 10).first);
        CHECK(map[1] == 0);
        CHECK(map[2] == 0);  // Default value
    }
}

TEST_CASE("SortedHeapMap operations") {
    struct Less {
        bool operator()(int a, int b) const { return a < b; }
    };

    SUBCASE("Insert maintains key order") {
        fl::SortedHeapMap<int, std::string, Less> map;
        
        map.insert(3, "three");
        map.insert(1, "one");
        map.insert(4, "four");
        map.insert(2, "two");

        CHECK(map.size() == 4);
        CHECK(map.has(1));
        CHECK(map.has(2));
        CHECK(map.has(3));
        CHECK(map.has(4));
        CHECK_FALSE(map.has(5));

        // Verify order by iterating
        auto it = map.begin();
        CHECK(it->first == 1);
        ++it;
        CHECK(it->first == 2);
        ++it;
        CHECK(it->first == 3);
        ++it;
        CHECK(it->first == 4);
        ++it;
        CHECK(it == map.end());
    }
}
