// g++ --std=c++11 test_fixed_map.cpp -I../src

#include "test.h"
#include "test.h"
#include "fl/map.h"
#include "fl/vector.h"
#include "fl/string.h"

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
        fl::SortedHeapMap<int, fl::string, Less> map;
        
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

TEST_CASE("fl::map - std::map drop-in replacement") {
    fl::map<int, fl::string> map;

    SUBCASE("Basic insertion and access") {
        // Test operator[]
        map[1] = "one";
        map[3] = "three";
        map[2] = "two";
        
        CHECK(map.size() == 3);
        CHECK(map[1] == "one");
        CHECK(map[2] == "two");
        CHECK(map[3] == "three");
    }

    SUBCASE("Insert with pair") {
        auto result1 = map.insert({1, "one"});
        CHECK(result1.second == true);  // Successfully inserted
        CHECK(result1.first.key() == 1);
        CHECK(result1.first.value() == "one");
        
        auto result2 = map.insert({1, "ONE"});  // Duplicate key
        CHECK(result2.second == false);  // Not inserted
        CHECK(result2.first.value() == "one");  // Original value preserved
        
        CHECK(map.size() == 1);
    }

    SUBCASE("Iterator-based insertion") {
        fl::map<int, fl::string> other_map;
        other_map[1] = "one";
        other_map[2] = "two";
        other_map[3] = "three";
        
        map.insert(other_map.begin(), other_map.end());
        
        CHECK(map.size() == 3);
        CHECK(map[1] == "one");
        CHECK(map[2] == "two");
        CHECK(map[3] == "three");
    }

    SUBCASE("Find operations") {
        map[1] = "one";
        map[2] = "two";
        map[3] = "three";
        
        auto it = map.find(2);
        CHECK(it != map.end());
        CHECK(it.key() == 2);
        CHECK(it.value() == "two");
        
        auto not_found = map.find(99);
        CHECK(not_found == map.end());
    }

    SUBCASE("Count operations") {
        map[1] = "one";
        
        CHECK(map.count(1) == 1);
        CHECK(map.count(99) == 0);
    }

    SUBCASE("Erase operations") {
        map[1] = "one";
        map[2] = "two";
        map[3] = "three";
        map[4] = "four";
        
        // Erase by key
        size_t erased = map.erase(2);
        CHECK(erased == 1);
        CHECK(map.size() == 3);
        CHECK(map.find(2) == map.end());
        CHECK(map.count(2) == 0);
        
        // Erase by iterator
        auto it = map.find(3);
        auto next_it = map.erase(it);
        CHECK(map.size() == 2);
        CHECK(map.find(3) == map.end());
        if (next_it != map.end()) {
            CHECK(next_it.key() == 4);  // Points to next element
        }
    }

    SUBCASE("Iterator range erase") {
        map[1] = "one";
        map[2] = "two";
        map[3] = "three";
        map[4] = "four";
        map[5] = "five";
        
        auto first = map.find(2);
        auto last = map.find(4);
        ++last;  // Make it past-the-end for range [2, 4]
        
        auto result = map.erase(first, last);
        CHECK(map.size() == 2);
        CHECK(map.find(2) == map.end());
        CHECK(map.find(3) == map.end());
        CHECK(map.find(4) == map.end());
        CHECK(map[1] == "one");
        CHECK(map[5] == "five");
        
        if (result != map.end()) {
            CHECK(result.key() == 5);  // Points to element after erased range
        }
    }

    SUBCASE("Lower/upper bound operations") {
        map[1] = "one";
        map[3] = "three";
        map[5] = "five";
        map[7] = "seven";
        
        // Lower bound
        auto lb2 = map.lower_bound(2);
        CHECK(lb2.key() == 3);  // First element >= 2
        
        auto lb3 = map.lower_bound(3);
        CHECK(lb3.key() == 3);  // Exact match
        
        auto lb10 = map.lower_bound(10);
        CHECK(lb10 == map.end());  // No element >= 10
        
        // Upper bound
        auto ub3 = map.upper_bound(3);
        CHECK(ub3.key() == 5);  // First element > 3
        
        auto ub7 = map.upper_bound(7);
        CHECK(ub7 == map.end());  // No element > 7
    }

    SUBCASE("Equal range operations") {
        map[1] = "one";
        map[3] = "three";
        map[5] = "five";
        
        auto range = map.equal_range(3);
        CHECK(range.first.key() == 3);
        CHECK(range.second.key() == 5);  // Past the element
        
        auto not_found_range = map.equal_range(4);
        CHECK(not_found_range.first == not_found_range.second);
        if (not_found_range.first != map.end()) {
            CHECK(not_found_range.first.key() == 5);  // Both point to first element > 4
        }
    }

    SUBCASE("Iterator consistency and order") {
        map[5] = "five";
        map[2] = "two";
        map[8] = "eight";
        map[1] = "one";
        map[3] = "three";
        
        // Verify sorted order
        fl::vector<int> keys;
        for (const auto& pair : map) {
            keys.push_back(pair.first);
        }
        
        CHECK(keys.size() == 5);
        CHECK(keys[0] == 1);
        CHECK(keys[1] == 2);
        CHECK(keys[2] == 3);
        CHECK(keys[3] == 5);
        CHECK(keys[4] == 8);
    }

    SUBCASE("At method with bounds checking") {
        map[1] = "one";
        
        CHECK(map.at(1) == "one");
        
        // Note: at() with non-existent key will assert in debug builds
        // This matches the embedded nature of FastLED - we use asserts instead of exceptions
    }

    SUBCASE("Capacity and sizing") {
        CHECK(map.empty());
        CHECK(map.size() == 0);
        
        map[1] = "one";
        CHECK(!map.empty());
        CHECK(map.size() == 1);
        
        map.clear();
        CHECK(map.empty());
        CHECK(map.size() == 0);
    }

    SUBCASE("Swap operation") {
        fl::map<int, fl::string> map1, map2;
        map1[1] = "one";
        map1[2] = "two";
        map2[3] = "three";
        
        map1.swap(map2);
        
        CHECK(map1.size() == 1);
        CHECK(map1[3] == "three");
        CHECK(map2.size() == 2);
        CHECK(map2[1] == "one");
        CHECK(map2[2] == "two");
    }

    SUBCASE("Comparison operators") {
        fl::map<int, fl::string> map1, map2;
        map1[1] = "one";
        map1[2] = "two";
        
        map2[1] = "one";
        map2[2] = "two";
        
        CHECK(map1 == map2);
        CHECK(!(map1 != map2));
        
        map2[3] = "three";
        CHECK(map1 != map2);
        CHECK(!(map1 == map2));
    }

    SUBCASE("FastLED-specific compatibility methods") {
        map[1] = "one";
        map[2] = "two";
        
        // Test FastLED-style get method
        fl::string value;
        CHECK(map.get(1, &value) == true);
        CHECK(value == "one");
        CHECK(map.get(99, &value) == false);
        
        // Test has/contains methods
        CHECK(map.has(1) == true);
        CHECK(map.contains(1) == true);
        CHECK(map.has(99) == false);
        CHECK(map.contains(99) == false);
        
        // Test capacity methods
        map.setMaxSize(100);
        CHECK(map.capacity() >= 100);
        CHECK(!map.full());
    }
}

TEST_CASE("fl::map with custom comparator") {
    struct ReverseCompare {
        bool operator()(int a, int b) const { return a > b; }
    };
    
    fl::map<int, fl::string, ReverseCompare> map;
    
    map[1] = "one";
    map[3] = "three";
    map[2] = "two";
    
    // Should be sorted in reverse order
    fl::vector<int> keys;
    for (const auto& pair : map) {
        keys.push_back(pair.first);
    }
    
    CHECK(keys.size() == 3);
    CHECK(keys[0] == 3);  // Largest first
    CHECK(keys[1] == 2);
    CHECK(keys[2] == 1);
}
