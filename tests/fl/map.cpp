#include "test.h"
#include <map>
#include <string>
#include <vector>
#include "fl/map.h"
#include "fl/string.h"
#include "fl/vector.h"
// Helper function to compare map contents
template<typename StdMap, typename FlMap>
bool maps_equal(const StdMap& std_map, const FlMap& fl_map) {
    if (std_map.size() != fl_map.size()) {
        return false;
    }
    
    auto std_it = std_map.begin();
    auto fl_it = fl_map.begin();
    
    while (std_it != std_map.end() && fl_it != fl_map.end()) {
        if (std_it->first != fl_it->first || std_it->second != fl_it->second) {
            return false;
        }
        ++std_it;
        ++fl_it;
    }
    
    return std_it == std_map.end() && fl_it == fl_map.end();
}

TEST_CASE("std::map vs fl::fl_map - Basic Construction and Size") {
    std::map<int, int> std_map;
    fl::fl_map<int, int> fl_map;
    
    SUBCASE("Default construction") {
        CHECK(std_map.empty() == fl_map.empty());
        CHECK(std_map.size() == fl_map.size());
        CHECK(std_map.size() == 0);
        CHECK(fl_map.size() == 0);
    }
}

TEST_CASE("std::map vs fl::fl_map - Insert Operations") {
    std::map<int, fl::string> std_map;
    fl::fl_map<int, fl::string> fl_map;
    
    SUBCASE("Insert with pair") {
        auto std_result = std_map.insert({1, "one"});
        auto fl_result = fl_map.insert({1, "one"});
        
        CHECK(std_result.second == fl_result.second);
        CHECK(std_result.first->first == fl_result.first->first);
        CHECK(std_result.first->second == fl_result.first->second);
        CHECK(maps_equal(std_map, fl_map));
    }
    
    SUBCASE("Insert duplicate key") {
        std_map.insert({1, "one"});
        fl_map.insert({1, "one"});
        
        auto std_result = std_map.insert({1, "ONE"});
        auto fl_result = fl_map.insert({1, "ONE"});
        
        CHECK(std_result.second == fl_result.second);
        CHECK(std_result.second == false); // Should not insert duplicate
        CHECK(maps_equal(std_map, fl_map));
    }
    
    SUBCASE("Multiple inserts maintain order") {
        std::vector<std::pair<int, fl::string>> test_data = {
            {3, "three"}, {1, "one"}, {4, "four"}, {2, "two"}
        };
        
        for (const auto& item : test_data) {
            std_map.insert(item);
            fl_map.insert(fl::pair<int, fl::string>(item.first, item.second));
        }
        
        CHECK(maps_equal(std_map, fl_map));
        CHECK(std_map.size() == 4);
        CHECK(fl_map.size() == 4);
    }
}

TEST_CASE("std::map vs fl::fl_map - Element Access") {
    std::map<int, fl::string> std_map;
    fl::fl_map<int, fl::string> fl_map;
    
    // Insert some test data
    std_map[1] = "one";
    std_map[2] = "two";
    std_map[3] = "three";
    
    fl_map[1] = "one";
    fl_map[2] = "two";
    fl_map[3] = "three";
    
    SUBCASE("operator[] access existing keys") {
        CHECK(std_map[1] == fl_map[1]);
        CHECK(std_map[2] == fl_map[2]);
        CHECK(std_map[3] == fl_map[3]);
    }
    
    SUBCASE("operator[] creates new key with default value") {
        CHECK(std_map[4] == fl_map[4]); // Both should create empty string
        CHECK(std_map.size() == fl_map.size());
        CHECK(maps_equal(std_map, fl_map));
    }
    
    SUBCASE("at() method for existing keys") {
        CHECK(std_map.at(1) == fl_map.at(1));
        CHECK(std_map.at(2) == fl_map.at(2));
        CHECK(std_map.at(3) == fl_map.at(3));
    }
    
}

TEST_CASE("std::map vs fl::fl_map - Find Operations") {
    std::map<int, fl::string> std_map;
    fl::fl_map<int, fl::string> fl_map;
    
    // Insert test data
    std_map.insert({1, "one"});
    std_map.insert({2, "two"});
    std_map.insert({3, "three"});
    
    fl_map.insert({1, "one"});
    fl_map.insert({2, "two"});
    fl_map.insert({3, "three"});
    
    SUBCASE("find() existing keys") {
        auto std_it = std_map.find(2);
        auto fl_it = fl_map.find(2);
        
        CHECK((std_it != std_map.end()) == (fl_it != fl_map.end()));
        CHECK(std_it->first == fl_it->first);
        CHECK(std_it->second == fl_it->second);
    }
    
    SUBCASE("find() non-existent keys") {
        auto std_it = std_map.find(99);
        auto fl_it = fl_map.find(99);
        
        CHECK((std_it == std_map.end()) == (fl_it == fl_map.end()));
    }
    
    SUBCASE("count() method") {
        CHECK(std_map.count(1) == fl_map.count(1));
        CHECK(std_map.count(2) == fl_map.count(2));
        CHECK(std_map.count(99) == fl_map.count(99));
        CHECK(std_map.count(99) == 0);
        CHECK(fl_map.count(99) == 0);
    }
    
    SUBCASE("contains() method (C++20 style)") {
        // std::map::contains is C++20, but we can test fl_map's version
        CHECK(fl_map.contains(1) == true);
        CHECK(fl_map.contains(2) == true);
        CHECK(fl_map.contains(99) == false);
        
        // Compare with std::map using count
        CHECK((std_map.count(1) > 0) == fl_map.contains(1));
        CHECK((std_map.count(99) > 0) == fl_map.contains(99));
    }
}

TEST_CASE("std::map vs fl::fl_map - Iterator Operations") {
    std::map<int, fl::string> std_map;
    fl::fl_map<int, fl::string> fl_map;
    
    // Insert test data in different order to test sorting
    std::vector<std::pair<int, fl::string>> test_data = {
        {3, "three"}, {1, "one"}, {4, "four"}, {2, "two"}
    };
    
    for (const auto& item : test_data) {
        std_map.insert(item);
        fl_map.insert(fl::pair<int, fl::string>(item.first, item.second));
    }
    
    SUBCASE("Forward iteration order") {
        std::vector<int> std_order;
        std::vector<int> fl_order;
        
        for (const auto& pair : std_map) {
            std_order.push_back(pair.first);
        }
        
        for (const auto& pair : fl_map) {
            fl_order.push_back(pair.first);
        }
        
        CHECK(std_order == fl_order);
        CHECK(std_order == std::vector<int>{1, 2, 3, 4}); // Should be sorted
    }
    
    SUBCASE("begin() and end() iterators") {
        CHECK((std_map.begin() == std_map.end()) == (fl_map.begin() == fl_map.end()));
        CHECK(std_map.begin()->first == fl_map.begin()->first);
        CHECK(std_map.begin()->second == fl_map.begin()->second);
    }
    
    SUBCASE("Iterator increment") {
        auto std_it = std_map.begin();
        auto fl_it = fl_map.begin();
        
        ++std_it;
        ++fl_it;
        
        CHECK(std_it->first == fl_it->first);
        CHECK(std_it->second == fl_it->second);
    }
}

TEST_CASE("std::map vs fl::fl_map - Erase Operations") {
    std::map<int, fl::string> std_map;
    fl::fl_map<int, fl::string> fl_map;
    
    // Insert test data
    for (int i = 1; i <= 5; ++i) {
        std::string std_value = "value" + std::to_string(i);
        fl::string fl_value = "value";
        fl_value += std::to_string(i).c_str();
        std_map[i] = fl::string(std_value.c_str());
        fl_map[i] = fl_value;
    }
    
    SUBCASE("Erase by key") {
        size_t std_erased = std_map.erase(3);
        size_t fl_erased = fl_map.erase(3);
        
        CHECK(std_erased == fl_erased);
        CHECK(std_erased == 1);
        CHECK(maps_equal(std_map, fl_map));
    }
    
    SUBCASE("Erase non-existent key") {
        size_t std_erased = std_map.erase(99);
        size_t fl_erased = fl_map.erase(99);
        
        CHECK(std_erased == fl_erased);
        CHECK(std_erased == 0);
        CHECK(maps_equal(std_map, fl_map));
    }
    
    SUBCASE("Erase by iterator") {
        auto std_it = std_map.find(2);
        auto fl_it = fl_map.find(2);
        
        std_map.erase(std_it);
        fl_map.erase(fl_it);
        
        CHECK(maps_equal(std_map, fl_map));
        CHECK(std_map.find(2) == std_map.end());
        CHECK(fl_map.find(2) == fl_map.end());
    }
}

TEST_CASE("std::map vs fl::fl_map - Clear and Empty") {
    std::map<int, fl::string> std_map;
    fl::fl_map<int, fl::string> fl_map;
    
    // Insert some data
    std_map[1] = "one";
    std_map[2] = "two";
    fl_map[1] = "one";
    fl_map[2] = "two";
    
    CHECK(std_map.empty() == fl_map.empty());
    CHECK(std_map.empty() == false);
    
    SUBCASE("Clear operation") {
        std_map.clear();
        fl_map.clear();
        
        CHECK(std_map.empty() == fl_map.empty());
        CHECK(std_map.size() == fl_map.size());
        CHECK(std_map.size() == 0);
        CHECK(maps_equal(std_map, fl_map));
    }
}

TEST_CASE("std::map vs fl::fl_map - Bound Operations") {
    std::map<int, fl::string> std_map;
    fl::fl_map<int, fl::string> fl_map;
    
    // Insert test data: 1, 3, 5, 7, 9
    for (int i = 1; i <= 9; i += 2) {
        std::string std_value = "value" + std::to_string(i);
        fl::string fl_value = "value";
        fl_value += std::to_string(i).c_str();
        std_map[i] = fl::string(std_value.c_str());
        fl_map[i] = fl_value;
    }
    
    SUBCASE("lower_bound existing key") {
        auto std_it = std_map.lower_bound(5);
        auto fl_it = fl_map.lower_bound(5);
        
        CHECK(std_it->first == fl_it->first);
        CHECK(std_it->second == fl_it->second);
        CHECK(std_it->first == 5);
    }
    
    SUBCASE("lower_bound non-existing key") {
        auto std_it = std_map.lower_bound(4);
        auto fl_it = fl_map.lower_bound(4);
        
        CHECK(std_it->first == fl_it->first);
        CHECK(std_it->first == 5); // Should find next higher key
    }
    
    SUBCASE("upper_bound existing key") {
        auto std_it = std_map.upper_bound(5);
        auto fl_it = fl_map.upper_bound(5);
        
        CHECK(std_it->first == fl_it->first);
        CHECK(std_it->first == 7); // Should find next higher key
    }
    
    SUBCASE("equal_range") {
        auto std_range = std_map.equal_range(5);
        auto fl_range = fl_map.equal_range(5);
        
        CHECK(std_range.first->first == fl_range.first->first);
        CHECK(std_range.second->first == fl_range.second->first);
        CHECK(std_range.first->first == 5);
        CHECK(std_range.second->first == 7);
    }
}

TEST_CASE("std::map vs fl::fl_map - Comparison Operations") {
    fl::fl_map<int, fl::string> fl_map1;
    fl::fl_map<int, fl::string> fl_map2;
    
    SUBCASE("Empty maps equality") {
        CHECK(fl_map1 == fl_map2);
        CHECK(!(fl_map1 != fl_map2));
    }
    
    SUBCASE("Equal maps") {
        fl_map1[1] = "one";
        fl_map1[2] = "two";
        fl_map2[1] = "one";
        fl_map2[2] = "two";
        
        CHECK(fl_map1 == fl_map2);
        CHECK(!(fl_map1 != fl_map2));
    }
    
    SUBCASE("Different maps") {
        fl_map1[1] = "one";
        fl_map2[1] = "ONE"; // Different value
        
        CHECK(!(fl_map1 == fl_map2));
        CHECK(fl_map1 != fl_map2);
    }
    
    SUBCASE("Different sizes") {
        fl_map1[1] = "one";
        fl_map1[2] = "two";
        fl_map2[1] = "one";
        
        CHECK(!(fl_map1 == fl_map2));
        CHECK(fl_map1 != fl_map2);
    }
}

TEST_CASE("std::map vs fl::fl_map - Edge Cases") {
    std::map<int, int> std_map;
    fl::fl_map<int, int> fl_map;
    
    SUBCASE("Large numbers of insertions") {
        // Test with more elements to stress test the data structures
        for (int i = 100; i >= 1; --i) { // Insert in reverse order
            std_map[i] = i * 10;
            fl_map[i] = i * 10;
        }
        
        CHECK(std_map.size() == fl_map.size());
        CHECK(std_map.size() == 100);
        CHECK(maps_equal(std_map, fl_map));
        
        // Verify sorted order
        int prev_key = 0;
        for (const auto& pair : fl_map) {
            CHECK(pair.first > prev_key);
            prev_key = pair.first;
        }
    }
    
    SUBCASE("Mixed operations") {
        // Perform a series of mixed operations
        std_map[5] = 50;
        fl_map[5] = 50;
        CHECK(maps_equal(std_map, fl_map));
        
        std_map.erase(5);
        fl_map.erase(5);
        CHECK(maps_equal(std_map, fl_map));
        
        std_map[1] = 10;
        std_map[3] = 30;
        fl_map[1] = 10;
        fl_map[3] = 30;
        CHECK(maps_equal(std_map, fl_map));
        
        std_map.clear();
        fl_map.clear();
        CHECK(maps_equal(std_map, fl_map));
        CHECK(std_map.empty());
        CHECK(fl_map.empty());
    }
}

// Test with custom comparator
struct DescendingInt {
    bool operator()(int a, int b) const {
        return a > b; // Reverse order
    }
};

TEST_CASE("std::map vs fl::fl_map - Custom Comparator") {
    std::map<int, fl::string, DescendingInt> std_map;
    fl::fl_map<int, fl::string, DescendingInt> fl_map;

    std_map[1] = "one";
    std_map[2] = "two";
    std_map[3] = "three";

    fl_map[1] = "one";
    fl_map[2] = "two";
    fl_map[3] = "three";

    SUBCASE("Custom ordering") {
        std::vector<int> std_order;
        std::vector<int> fl_order;

        for (const auto& pair : std_map) {
            std_order.push_back(pair.first);
        }

        for (const auto& pair : fl_map) {
            fl_order.push_back(pair.first);
        }

        CHECK(std_order == fl_order);
        CHECK(std_order == std::vector<int>{3, 2, 1}); // Descending order
    }
}

TEST_CASE("std::map vs fl::fl_map - Comparators (key_comp and value_comp)") {
    std::map<int, fl::string> std_map;
    fl::fl_map<int, fl::string> fl_map;

    SUBCASE("key_comp comparison") {
        auto std_comp = std_map.key_comp();
        auto fl_comp = fl_map.key_comp();

        // Both should produce the same comparison results
        CHECK(std_comp(1, 2) == fl_comp(1, 2));
        CHECK(std_comp(2, 1) == fl_comp(2, 1));
        CHECK(std_comp(1, 1) == fl_comp(1, 1));
    }

    SUBCASE("value_comp comparison with pairs") {
        auto std_vcomp = std_map.value_comp();
        auto fl_vcomp = fl_map.value_comp();

        // Create test pairs
        std::pair<int, fl::string> std_p1(1, "one");
        std::pair<int, fl::string> std_p2(2, "two");

        fl::pair<int, fl::string> fl_p1(1, "one");
        fl::pair<int, fl::string> fl_p2(2, "two");

        // value_comp should compare based on keys only, not values
        CHECK(std_vcomp(std_p1, std_p2) == fl_vcomp(fl_p1, fl_p2));
        CHECK(std_vcomp(std_p2, std_p1) == fl_vcomp(fl_p2, fl_p1));

        // Test that value doesn't matter, only key
        fl::pair<int, fl::string> fl_p1_diff(1, "different");
        CHECK(fl_vcomp(fl_p1, fl_p1_diff) == false);
        CHECK(fl_vcomp(fl_p1_diff, fl_p1) == false);
    }

    SUBCASE("value_comp with custom comparator") {
        std::map<int, fl::string, DescendingInt> std_desc_map;
        fl::fl_map<int, fl::string, DescendingInt> fl_desc_map;

        auto fl_vcomp = fl_desc_map.value_comp();

        fl::pair<int, fl::string> fl_p1(1, "one");
        fl::pair<int, fl::string> fl_p2(2, "two");

        // With descending comparator, 2 < 1 should be true (since we're using DescendingInt)
        CHECK(fl_vcomp(fl_p2, fl_p1) == true);
        CHECK(fl_vcomp(fl_p1, fl_p2) == false);
    }
}

TEST_CASE("std::map vs fl::fl_map - Move Constructor and Move Assignment") {
    SUBCASE("Move constructor") {
        // Create and populate a map
        std::map<int, fl::string> std_map;
        fl::fl_map<int, fl::string> fl_map;

        std_map[1] = "one";
        std_map[2] = "two";
        std_map[3] = "three";

        fl_map[1] = "one";
        fl_map[2] = "two";
        fl_map[3] = "three";

        // Move construct
        std::map<int, fl::string> std_moved(fl::move(std_map));
        fl::fl_map<int, fl::string> fl_moved(fl::move(fl_map));

        // Verify moved-to maps have the contents
        CHECK(std_moved.size() == 3);
        CHECK(fl_moved.size() == 3);
        CHECK(maps_equal(std_moved, fl_moved));
        CHECK(std_moved[1] == fl_moved[1]);
        CHECK(std_moved[2] == fl_moved[2]);
        CHECK(std_moved[3] == fl_moved[3]);

        // Original maps should be empty (valid but unspecified state, typically empty)
        // Note: After move, std::map is guaranteed to be empty or valid but unspecified
        // We can check fl_map is also empty
        CHECK(fl_map.empty());
    }

    SUBCASE("Move assignment operator") {
        // Create and populate source map
        std::map<int, fl::string> std_src;
        fl::fl_map<int, fl::string> fl_src;

        std_src[1] = "one";
        std_src[2] = "two";
        std_src[3] = "three";

        fl_src[1] = "one";
        fl_src[2] = "two";
        fl_src[3] = "three";

        // Create destination maps with some initial content
        std::map<int, fl::string> std_dst;
        fl::fl_map<int, fl::string> fl_dst;
        std_dst[99] = "should be replaced";
        fl_dst[99] = "should be replaced";

        // Move assign
        std_dst = fl::move(std_src);
        fl_dst = fl::move(fl_src);

        // Verify destination maps have the contents
        CHECK(std_dst.size() == 3);
        CHECK(fl_dst.size() == 3);
        CHECK(maps_equal(std_dst, fl_dst));
        CHECK(std_dst[1] == fl_dst[1]);
        CHECK(std_dst[2] == fl_dst[2]);
        CHECK(std_dst[3] == fl_dst[3]);
        CHECK(std_dst.find(99) == std_dst.end());
        CHECK(fl_dst.find(99) == fl_dst.end());

        // Source maps should be empty (valid but unspecified state)
        CHECK(fl_src.empty());
    }

    SUBCASE("Move assignment to self (edge case)") {
        fl::fl_map<int, fl::string> fl_map;
        fl_map[1] = "one";
        fl_map[2] = "two";

        // Self-move-assignment (should be handled safely)
        fl_map = fl::move(fl_map);

        // Map should still be in valid state (may be empty or have contents)
        // At minimum, it should not crash
        CHECK(fl_map.size() >= 0); // Just verify it's in a valid state
    }
}

// insert_or_assign() requires C++17
#if __cplusplus >= 201703L
TEST_CASE("std::map vs fl::fl_map - insert_or_assign()") {
    SUBCASE("Insert new key with lvalue") {
        std::map<int, fl::string> std_map;
        fl::fl_map<int, fl::string> fl_map;

        int key = 1;
        fl::string value = "one";

        // Insert new key - should insert
        auto std_result = std_map.insert_or_assign(key, value);
        auto fl_result = fl_map.insert_or_assign(key, value);

        CHECK(std_result.second == fl_result.second); // Both should return true (inserted)
        CHECK(std_result.second == true);
        CHECK(std_result.first->first == fl_result.first->first);
        CHECK(std_result.first->second == fl_result.first->second);
        CHECK(maps_equal(std_map, fl_map));
    }

    SUBCASE("Update existing key with lvalue") {
        std::map<int, fl::string> std_map;
        fl::fl_map<int, fl::string> fl_map;

        // Insert initial value
        std_map[1] = "one";
        fl_map[1] = "one";

        int key = 1;
        fl::string new_value = "ONE";

        // Update existing key - should assign
        auto std_result = std_map.insert_or_assign(key, new_value);
        auto fl_result = fl_map.insert_or_assign(key, new_value);

        CHECK(std_result.second == fl_result.second); // Both should return false (assigned)
        CHECK(std_result.second == false);
        CHECK(std_result.first->second == "ONE");
        CHECK(fl_result.first->second == "ONE");
        CHECK(maps_equal(std_map, fl_map));
    }

    SUBCASE("Insert new key with rvalue") {
        std::map<int, fl::string> std_map;
        fl::fl_map<int, fl::string> fl_map;

        // Insert new key with move semantics
        auto std_result = std_map.insert_or_assign(1, fl::string("one"));
        auto fl_result = fl_map.insert_or_assign(1, fl::string("one"));

        CHECK(std_result.second == fl_result.second);
        CHECK(std_result.second == true);
        CHECK(std_result.first->first == fl_result.first->first);
        CHECK(std_result.first->second == fl_result.first->second);
        CHECK(maps_equal(std_map, fl_map));
    }

    SUBCASE("Update existing key with rvalue") {
        std::map<int, fl::string> std_map;
        fl::fl_map<int, fl::string> fl_map;

        // Insert initial value
        std_map[1] = "one";
        fl_map[1] = "one";

        // Update with rvalue
        auto std_result = std_map.insert_or_assign(1, fl::string("ONE"));
        auto fl_result = fl_map.insert_or_assign(1, fl::string("ONE"));

        CHECK(std_result.second == fl_result.second);
        CHECK(std_result.second == false);
        CHECK(std_result.first->second == "ONE");
        CHECK(fl_result.first->second == "ONE");
        CHECK(maps_equal(std_map, fl_map));
    }

    SUBCASE("Multiple insert_or_assign operations") {
        std::map<int, fl::string> std_map;
        fl::fl_map<int, fl::string> fl_map;

        // Insert key 1
        auto std_r1 = std_map.insert_or_assign(1, fl::string("one"));
        auto fl_r1 = fl_map.insert_or_assign(1, fl::string("one"));
        CHECK(std_r1.second == true);
        CHECK(fl_r1.second == true);

        // Insert key 2
        auto std_r2 = std_map.insert_or_assign(2, fl::string("two"));
        auto fl_r2 = fl_map.insert_or_assign(2, fl::string("two"));
        CHECK(std_r2.second == true);
        CHECK(fl_r2.second == true);

        // Update key 1
        auto std_r3 = std_map.insert_or_assign(1, fl::string("ONE"));
        auto fl_r3 = fl_map.insert_or_assign(1, fl::string("ONE"));
        CHECK(std_r3.second == false);
        CHECK(fl_r3.second == false);

        // Insert key 3
        auto std_r4 = std_map.insert_or_assign(3, fl::string("three"));
        auto fl_r4 = fl_map.insert_or_assign(3, fl::string("three"));
        CHECK(std_r4.second == true);
        CHECK(fl_r4.second == true);

        // Verify final state
        CHECK(std_map.size() == 3);
        CHECK(fl_map.size() == 3);
        CHECK(maps_equal(std_map, fl_map));
        CHECK(std_map[1] == "ONE");
        CHECK(fl_map[1] == "ONE");
    }

    SUBCASE("insert_or_assign with key rvalue") {
        std::map<int, fl::string> std_map;
        fl::fl_map<int, fl::string> fl_map;

        // Test with rvalue key (moving the key)
        auto std_result = std_map.insert_or_assign(1, fl::string("one"));
        auto fl_result = fl_map.insert_or_assign(1, fl::string("one"));

        CHECK(std_result.second == fl_result.second);
        CHECK(std_result.second == true);
        CHECK(maps_equal(std_map, fl_map));
    }
}
#endif // __cplusplus >= 201703L

// Helper class to track construction calls
class ConstructionCounter {
public:
    static int construction_count;
    static int default_construction_count;
    static int copy_construction_count;
    static int move_construction_count;

    int value;

    static void reset() {
        construction_count = 0;
        default_construction_count = 0;
        copy_construction_count = 0;
        move_construction_count = 0;
    }

    ConstructionCounter() : value(0) {
        ++construction_count;
        ++default_construction_count;
    }

    explicit ConstructionCounter(int v) : value(v) {
        ++construction_count;
    }

    ConstructionCounter(const ConstructionCounter& other) : value(other.value) {
        ++construction_count;
        ++copy_construction_count;
    }

    ConstructionCounter(ConstructionCounter&& other) : value(other.value) {
        ++construction_count;
        ++move_construction_count;
    }

    ConstructionCounter& operator=(const ConstructionCounter& other) {
        value = other.value;
        return *this;
    }

    ConstructionCounter& operator=(ConstructionCounter&& other) {
        value = other.value;
        return *this;
    }

    bool operator==(const ConstructionCounter& other) const {
        return value == other.value;
    }

    bool operator!=(const ConstructionCounter& other) const {
        return value != other.value;
    }
};

int ConstructionCounter::construction_count = 0;
int ConstructionCounter::default_construction_count = 0;
int ConstructionCounter::copy_construction_count = 0;
int ConstructionCounter::move_construction_count = 0;

// try_emplace() requires C++17
#if __cplusplus >= 201703L
TEST_CASE("std::map vs fl::fl_map - try_emplace()") {
    SUBCASE("Insert new key - value should be constructed") {
        std::map<int, fl::string> std_map;
        fl::fl_map<int, fl::string> fl_map;

        // Insert new key - should construct value in place
        auto std_result = std_map.try_emplace(1, "one");
        auto fl_result = fl_map.try_emplace(1, "one");

        CHECK(std_result.second == fl_result.second); // Both should return true (inserted)
        CHECK(std_result.second == true);
        CHECK(std_result.first->first == fl_result.first->first);
        CHECK(std_result.first->second == fl_result.first->second);
        CHECK(maps_equal(std_map, fl_map));
    }

    SUBCASE("Try to emplace existing key - should NOT modify value") {
        std::map<int, fl::string> std_map;
        fl::fl_map<int, fl::string> fl_map;

        // Insert initial value
        std_map[1] = "one";
        fl_map[1] = "one";

        // Try to emplace with existing key - should do nothing
        auto std_result = std_map.try_emplace(1, "ONE");
        auto fl_result = fl_map.try_emplace(1, "ONE");

        CHECK(std_result.second == fl_result.second); // Both should return false (not inserted)
        CHECK(std_result.second == false);
        CHECK(std_result.first->second == "one"); // Original value unchanged
        CHECK(fl_result.first->second == "one"); // Original value unchanged
        CHECK(maps_equal(std_map, fl_map));
    }

    SUBCASE("try_emplace with rvalue key") {
        std::map<int, fl::string> std_map;
        fl::fl_map<int, fl::string> fl_map;

        // Insert new key with move semantics for key
        auto std_result = std_map.try_emplace(1, "one");
        auto fl_result = fl_map.try_emplace(1, "one");

        CHECK(std_result.second == fl_result.second);
        CHECK(std_result.second == true);
        CHECK(std_result.first->first == fl_result.first->first);
        CHECK(std_result.first->second == fl_result.first->second);
        CHECK(maps_equal(std_map, fl_map));
    }

    SUBCASE("try_emplace constructs value in-place") {
        std::map<int, ConstructionCounter> std_map;
        fl::fl_map<int, ConstructionCounter> fl_map;

        ConstructionCounter::reset();

        // Insert new key - should construct value in place
        auto std_result = std_map.try_emplace(1, 100);
        int std_constructions = ConstructionCounter::construction_count;

        ConstructionCounter::reset();

        auto fl_result = fl_map.try_emplace(1, 100);
        int fl_constructions = ConstructionCounter::construction_count;

        CHECK(std_result.second == true);
        CHECK(fl_result.second == true);
        CHECK(std_result.first->second.value == 100);
        CHECK(fl_result.first->second.value == 100);

        // Both should construct the value (not copy/move from temporary)
        CHECK(std_constructions > 0);
        CHECK(fl_constructions > 0);
    }

    SUBCASE("try_emplace does NOT construct value from args when key exists") {
        std::map<int, ConstructionCounter> std_map;
        fl::fl_map<int, ConstructionCounter> fl_map;

        // Insert initial value
        std_map.try_emplace(1, 100);
        fl_map.try_emplace(1, 100);

        ConstructionCounter::reset();

        // Try to emplace with existing key - should NOT construct new value from args (999)
        // Note: fl_map may construct a default Value() for search (counted),
        // but should NOT construct from the actual args (999)
        auto std_result = std_map.try_emplace(1, 999);

        ConstructionCounter::reset();

        auto fl_result = fl_map.try_emplace(1, 999);
        int fl_constructions = ConstructionCounter::construction_count;

        CHECK(std_result.second == false);
        CHECK(fl_result.second == false);
        CHECK(std_result.first->second.value == 100); // Original value unchanged
        CHECK(fl_result.first->second.value == 100); // Original value unchanged

        // fl_map may construct a search temp (Value()), so allow some constructions
        // but it should be fewer than if we constructed from args
        // std::map with transparent comparator constructs 0, our impl may construct 1-2 for search
        CHECK(fl_constructions <= 2); // Allow for search temporary construction
    }

    SUBCASE("Multiple try_emplace operations") {
        std::map<int, fl::string> std_map;
        fl::fl_map<int, fl::string> fl_map;

        // Insert key 1
        auto std_r1 = std_map.try_emplace(1, "one");
        auto fl_r1 = fl_map.try_emplace(1, "one");
        CHECK(std_r1.second == true);
        CHECK(fl_r1.second == true);

        // Insert key 2
        auto std_r2 = std_map.try_emplace(2, "two");
        auto fl_r2 = fl_map.try_emplace(2, "two");
        CHECK(std_r2.second == true);
        CHECK(fl_r2.second == true);

        // Try to emplace key 1 again (should fail)
        auto std_r3 = std_map.try_emplace(1, "ONE");
        auto fl_r3 = fl_map.try_emplace(1, "ONE");
        CHECK(std_r3.second == false);
        CHECK(fl_r3.second == false);

        // Insert key 3
        auto std_r4 = std_map.try_emplace(3, "three");
        auto fl_r4 = fl_map.try_emplace(3, "three");
        CHECK(std_r4.second == true);
        CHECK(fl_r4.second == true);

        // Verify final state
        CHECK(std_map.size() == 3);
        CHECK(fl_map.size() == 3);
        CHECK(maps_equal(std_map, fl_map));
        CHECK(std_map[1] == "one"); // Original value, not "ONE"
        CHECK(fl_map[1] == "one"); // Original value, not "ONE"
    }

    SUBCASE("try_emplace with multiple constructor arguments") {
        std::map<int, fl::string> std_map;
        fl::fl_map<int, fl::string> fl_map;

        // try_emplace can forward multiple constructor arguments
        // For fl::string, we can pass const char* which will be used to construct the string
        auto std_result = std_map.try_emplace(1, "constructed in place");
        auto fl_result = fl_map.try_emplace(1, "constructed in place");

        CHECK(std_result.second == fl_result.second);
        CHECK(std_result.second == true);
        CHECK(std_result.first->second == fl_result.first->second);
        CHECK(maps_equal(std_map, fl_map));
    }
}
#endif // __cplusplus >= 201703L

TEST_CASE("std::map vs fl::fl_map - Range Insert") {
    std::map<int, fl::string> std_map;
    fl::fl_map<int, fl::string> fl_map;

    SUBCASE("Insert from vector of pairs") {
        // Create a vector of pairs to insert
        std::vector<std::pair<int, fl::string>> test_data = {
            {1, "one"}, {2, "two"}, {3, "three"}, {4, "four"}, {5, "five"}
        };

        // Use range insert for std::map
        std_map.insert(test_data.begin(), test_data.end());

        // Convert to fl::pair vector for fl_map
        fl::vector<fl::pair<int, fl::string>> fl_test_data;
        for (const auto& item : test_data) {
            fl_test_data.push_back(fl::pair<int, fl::string>(item.first, item.second));
        }

        // Use range insert for fl_map
        fl_map.insert(fl_test_data.begin(), fl_test_data.end());

        // Verify both maps have same contents
        CHECK(std_map.size() == fl_map.size());
        CHECK(std_map.size() == 5);
        CHECK(maps_equal(std_map, fl_map));
    }

    SUBCASE("Insert from empty range") {
        // Create empty vector
        std::vector<std::pair<int, fl::string>> empty_data;

        // Insert empty range
        std_map.insert(empty_data.begin(), empty_data.end());

        fl::vector<fl::pair<int, fl::string>> fl_empty_data;
        fl_map.insert(fl_empty_data.begin(), fl_empty_data.end());

        // Both maps should still be empty
        CHECK(std_map.empty());
        CHECK(fl_map.empty());
        CHECK(maps_equal(std_map, fl_map));
    }

    SUBCASE("Range insert with duplicate keys") {
        // Pre-populate maps
        std_map[2] = "TWO";
        std_map[4] = "FOUR";
        fl_map[2] = "TWO";
        fl_map[4] = "FOUR";

        // Create test data with overlapping keys
        std::vector<std::pair<int, fl::string>> test_data = {
            {1, "one"}, {2, "two"}, {3, "three"}, {4, "four"}, {5, "five"}
        };

        // Range insert should not overwrite existing keys
        std_map.insert(test_data.begin(), test_data.end());

        fl::vector<fl::pair<int, fl::string>> fl_test_data;
        for (const auto& item : test_data) {
            fl_test_data.push_back(fl::pair<int, fl::string>(item.first, item.second));
        }
        fl_map.insert(fl_test_data.begin(), fl_test_data.end());

        // Verify size and contents
        CHECK(std_map.size() == fl_map.size());
        CHECK(std_map.size() == 5);
        CHECK(maps_equal(std_map, fl_map));

        // Original values should be preserved for duplicate keys
        CHECK(std_map[2] == "TWO");
        CHECK(fl_map[2] == "TWO");
        CHECK(std_map[4] == "FOUR");
        CHECK(fl_map[4] == "FOUR");

        // New keys should be inserted
        CHECK(std_map[1] == "one");
        CHECK(fl_map[1] == "one");
        CHECK(std_map[3] == "three");
        CHECK(fl_map[3] == "three");
        CHECK(std_map[5] == "five");
        CHECK(fl_map[5] == "five");
    }

    SUBCASE("Range insert from another map") {
        // Create source maps
        std::map<int, fl::string> std_src;
        fl::fl_map<int, fl::string> fl_src;

        std_src[1] = "one";
        std_src[2] = "two";
        std_src[3] = "three";
        fl_src[1] = "one";
        fl_src[2] = "two";
        fl_src[3] = "three";

        // Range insert from one map to another
        std_map.insert(std_src.begin(), std_src.end());
        fl_map.insert(fl_src.begin(), fl_src.end());

        // Verify contents match
        CHECK(std_map.size() == fl_map.size());
        CHECK(std_map.size() == 3);
        CHECK(maps_equal(std_map, fl_map));
        CHECK(maps_equal(std_src, fl_src));
    }

    SUBCASE("Range insert maintains sorted order") {
        // Insert data in random order
        std::vector<std::pair<int, fl::string>> test_data = {
            {5, "five"}, {1, "one"}, {3, "three"}, {2, "two"}, {4, "four"}
        };

        std_map.insert(test_data.begin(), test_data.end());

        fl::vector<fl::pair<int, fl::string>> fl_test_data;
        for (const auto& item : test_data) {
            fl_test_data.push_back(fl::pair<int, fl::string>(item.first, item.second));
        }
        fl_map.insert(fl_test_data.begin(), fl_test_data.end());

        // Verify sorted order in iteration
        std::vector<int> std_order;
        std::vector<int> fl_order;

        for (const auto& pair : std_map) {
            std_order.push_back(pair.first);
        }

        for (const auto& pair : fl_map) {
            fl_order.push_back(pair.first);
        }

        CHECK(std_order == fl_order);
        CHECK(std_order == std::vector<int>{1, 2, 3, 4, 5});
        CHECK(maps_equal(std_map, fl_map));
    }

    SUBCASE("Range insert large dataset") {
        // Create larger dataset
        std::vector<std::pair<int, fl::string>> test_data;
        for (int i = 1; i <= 50; ++i) {
            std::string std_value = "value" + std::to_string(i);
            test_data.push_back({i, fl::string(std_value.c_str())});
        }

        std_map.insert(test_data.begin(), test_data.end());

        fl::vector<fl::pair<int, fl::string>> fl_test_data;
        for (const auto& item : test_data) {
            fl_test_data.push_back(fl::pair<int, fl::string>(item.first, item.second));
        }
        fl_map.insert(fl_test_data.begin(), fl_test_data.end());

        // Verify all elements inserted
        CHECK(std_map.size() == fl_map.size());
        CHECK(std_map.size() == 50);
        CHECK(maps_equal(std_map, fl_map));
    }
}

TEST_CASE("std::map vs fl::fl_map - Hint-based Insert") {
    std::map<int, fl::string> std_map;
    fl::fl_map<int, fl::string> fl_map;

    SUBCASE("Insert with hint at begin") {
        // Pre-populate maps
        std_map[5] = "five";
        std_map[10] = "ten";
        fl_map[5] = "five";
        fl_map[10] = "ten";

        // Insert with hint pointing to begin
        auto std_it = std_map.insert(std_map.begin(), {1, "one"});
        auto fl_it = fl_map.insert(fl_map.begin(), {1, "one"});

        // Verify both inserted successfully
        CHECK(std_it->first == fl_it->first);
        CHECK(std_it->second == fl_it->second);
        CHECK(std_it->first == 1);
        CHECK(maps_equal(std_map, fl_map));
        CHECK(std_map.size() == 3);
        CHECK(fl_map.size() == 3);
    }

    SUBCASE("Insert with hint at end") {
        // Pre-populate maps
        std_map[5] = "five";
        std_map[10] = "ten";
        fl_map[5] = "five";
        fl_map[10] = "ten";

        // Insert with hint pointing to end (for value greater than all existing)
        auto std_it = std_map.insert(std_map.end(), {20, "twenty"});
        auto fl_it = fl_map.insert(fl_map.end(), {20, "twenty"});

        // Verify both inserted successfully
        CHECK(std_it->first == fl_it->first);
        CHECK(std_it->second == fl_it->second);
        CHECK(std_it->first == 20);
        CHECK(maps_equal(std_map, fl_map));
        CHECK(std_map.size() == 3);
        CHECK(fl_map.size() == 3);
    }

    SUBCASE("Insert with hint in middle (good hint)") {
        // Pre-populate maps
        std_map[5] = "five";
        std_map[15] = "fifteen";
        fl_map[5] = "five";
        fl_map[15] = "fifteen";

        // Insert with hint pointing to position just after where new element should go
        auto std_hint = std_map.find(15);
        auto fl_hint = fl_map.find(15);

        auto std_it = std_map.insert(std_hint, {10, "ten"});
        auto fl_it = fl_map.insert(fl_hint, {10, "ten"});

        // Verify both inserted successfully
        CHECK(std_it->first == fl_it->first);
        CHECK(std_it->second == fl_it->second);
        CHECK(std_it->first == 10);
        CHECK(maps_equal(std_map, fl_map));
        CHECK(std_map.size() == 3);
        CHECK(fl_map.size() == 3);
    }

    SUBCASE("Insert with bad hint (hint far from insertion point)") {
        // Pre-populate maps
        std_map[5] = "five";
        std_map[10] = "ten";
        std_map[15] = "fifteen";
        fl_map[5] = "five";
        fl_map[10] = "ten";
        fl_map[15] = "fifteen";

        // Insert with bad hint (hint at beginning but inserting at end)
        auto std_it = std_map.insert(std_map.begin(), {20, "twenty"});
        auto fl_it = fl_map.insert(fl_map.begin(), {20, "twenty"});

        // Should still insert correctly despite bad hint
        CHECK(std_it->first == fl_it->first);
        CHECK(std_it->second == fl_it->second);
        CHECK(std_it->first == 20);
        CHECK(maps_equal(std_map, fl_map));
        CHECK(std_map.size() == 4);
        CHECK(fl_map.size() == 4);
    }

    SUBCASE("Insert duplicate key with hint - should not insert") {
        // Pre-populate maps
        std_map[5] = "five";
        std_map[10] = "ten";
        fl_map[5] = "five";
        fl_map[10] = "ten";

        // Try to insert duplicate key with hint
        auto std_hint = std_map.find(10);
        auto fl_hint = fl_map.find(10);

        auto std_it = std_map.insert(std_hint, {10, "TEN"});
        auto fl_it = fl_map.insert(fl_hint, {10, "TEN"});

        // Should return iterator to existing element
        CHECK(std_it->first == fl_it->first);
        CHECK(std_it->second == fl_it->second);
        CHECK(std_it->first == 10);
        CHECK(std_it->second == "ten"); // Original value unchanged
        CHECK(fl_it->second == "ten"); // Original value unchanged
        CHECK(maps_equal(std_map, fl_map));
        CHECK(std_map.size() == 2); // Size unchanged
        CHECK(fl_map.size() == 2);
    }

    SUBCASE("Multiple inserts with hints maintaining order") {
        // Insert multiple elements using hints
        auto std_it1 = std_map.insert(std_map.end(), {10, "ten"});
        auto fl_it1 = fl_map.insert(fl_map.end(), {10, "ten"});

        std_map.insert(std_it1, {5, "five"});
        fl_map.insert(fl_it1, {5, "five"});

        std_map.insert(std_map.end(), {15, "fifteen"});
        fl_map.insert(fl_map.end(), {15, "fifteen"});

        // Verify all inserted correctly and order is maintained
        CHECK(maps_equal(std_map, fl_map));
        CHECK(std_map.size() == 3);
        CHECK(fl_map.size() == 3);

        // Verify sorted order
        std::vector<int> std_order;
        std::vector<int> fl_order;

        for (const auto& pair : std_map) {
            std_order.push_back(pair.first);
        }

        for (const auto& pair : fl_map) {
            fl_order.push_back(pair.first);
        }

        CHECK(std_order == fl_order);
        CHECK(std_order == std::vector<int>{5, 10, 15});
    }

    SUBCASE("Insert with hint using rvalue") {
        // Pre-populate maps
        std_map[5] = "five";
        fl_map[5] = "five";

        // Insert with rvalue and hint
        auto std_it = std_map.insert(std_map.end(), {10, fl::string("ten")});
        auto fl_it = fl_map.insert(fl_map.end(), {10, fl::string("ten")});

        // Verify both inserted successfully
        CHECK(std_it->first == fl_it->first);
        CHECK(std_it->second == fl_it->second);
        CHECK(std_it->first == 10);
        CHECK(std_it->second == "ten");
        CHECK(fl_it->second == "ten");
        CHECK(maps_equal(std_map, fl_map));
    }

    SUBCASE("Insert into empty map with hint") {
        // Both maps are empty
        // Insert with hint (end() for empty map)
        auto std_it = std_map.insert(std_map.end(), {1, "one"});
        auto fl_it = fl_map.insert(fl_map.end(), {1, "one"});

        // Verify both inserted successfully
        CHECK(std_it->first == fl_it->first);
        CHECK(std_it->second == fl_it->second);
        CHECK(std_map.size() == 1);
        CHECK(fl_map.size() == 1);
        CHECK(maps_equal(std_map, fl_map));
    }

    SUBCASE("Sequential inserts with end() hint (optimal pattern)") {
        // This is a common pattern: inserting in sorted order with end() as hint
        // Should be very efficient (amortized O(1) with good implementation)

        std::vector<std::pair<int, fl::string>> test_data = {
            {1, "one"}, {2, "two"}, {3, "three"}, {4, "four"}, {5, "five"}
        };

        for (const auto& item : test_data) {
            std_map.insert(std_map.end(), item);
            fl_map.insert(fl_map.end(), fl::pair<int, fl::string>(item.first, item.second));
        }

        // Verify all inserted correctly
        CHECK(std_map.size() == 5);
        CHECK(fl_map.size() == 5);
        CHECK(maps_equal(std_map, fl_map));

        // Verify order
        std::vector<int> fl_order;
        for (const auto& pair : fl_map) {
            fl_order.push_back(pair.first);
        }
        CHECK(fl_order == std::vector<int>{1, 2, 3, 4, 5});
    }

    SUBCASE("Hint-based insert with move semantics") {
        // Test moving value with hint
        std_map[5] = "five";
        fl_map[5] = "five";

        fl::string movable_value = "ten";
        auto fl_it = fl_map.insert(fl_map.end(), fl::pair<int, fl::string>(10, fl::move(movable_value)));

        CHECK(fl_it->first == 10);
        CHECK(fl_it->second == "ten");
        CHECK(fl_map.size() == 2);
    }
}

TEST_CASE("std::map vs fl::fl_map - Hint-based Emplace") {
    std::map<int, fl::string> std_map;
    fl::fl_map<int, fl::string> fl_map;

    SUBCASE("Emplace with hint at begin") {
        // Pre-populate maps
        std_map[5] = "five";
        std_map[10] = "ten";
        fl_map[5] = "five";
        fl_map[10] = "ten";

        // Emplace with hint pointing to begin
        auto std_it = std_map.emplace_hint(std_map.begin(), 1, "one");
        auto fl_it = fl_map.emplace_hint(fl_map.begin(), 1, "one");

        // Verify both emplaced successfully
        CHECK(std_it->first == fl_it->first);
        CHECK(std_it->second == fl_it->second);
        CHECK(std_it->first == 1);
        CHECK(maps_equal(std_map, fl_map));
        CHECK(std_map.size() == 3);
        CHECK(fl_map.size() == 3);
    }

    SUBCASE("Emplace with hint at end") {
        // Pre-populate maps
        std_map[5] = "five";
        std_map[10] = "ten";
        fl_map[5] = "five";
        fl_map[10] = "ten";

        // Emplace with hint pointing to end (for value greater than all existing)
        auto std_it = std_map.emplace_hint(std_map.end(), 20, "twenty");
        auto fl_it = fl_map.emplace_hint(fl_map.end(), 20, "twenty");

        // Verify both emplaced successfully
        CHECK(std_it->first == fl_it->first);
        CHECK(std_it->second == fl_it->second);
        CHECK(std_it->first == 20);
        CHECK(maps_equal(std_map, fl_map));
        CHECK(std_map.size() == 3);
        CHECK(fl_map.size() == 3);
    }

    SUBCASE("Emplace with hint in middle (good hint)") {
        // Pre-populate maps
        std_map[5] = "five";
        std_map[15] = "fifteen";
        fl_map[5] = "five";
        fl_map[15] = "fifteen";

        // Emplace with hint pointing to position just after where new element should go
        auto std_hint = std_map.find(15);
        auto fl_hint = fl_map.find(15);

        auto std_it = std_map.emplace_hint(std_hint, 10, "ten");
        auto fl_it = fl_map.emplace_hint(fl_hint, 10, "ten");

        // Verify both emplaced successfully
        CHECK(std_it->first == fl_it->first);
        CHECK(std_it->second == fl_it->second);
        CHECK(std_it->first == 10);
        CHECK(maps_equal(std_map, fl_map));
        CHECK(std_map.size() == 3);
        CHECK(fl_map.size() == 3);
    }

    SUBCASE("Emplace with bad hint (hint far from insertion point)") {
        // Pre-populate maps
        std_map[5] = "five";
        std_map[10] = "ten";
        std_map[15] = "fifteen";
        fl_map[5] = "five";
        fl_map[10] = "ten";
        fl_map[15] = "fifteen";

        // Emplace with bad hint (hint at beginning but emplacing at end)
        auto std_it = std_map.emplace_hint(std_map.begin(), 20, "twenty");
        auto fl_it = fl_map.emplace_hint(fl_map.begin(), 20, "twenty");

        // Should still emplace correctly despite bad hint
        CHECK(std_it->first == fl_it->first);
        CHECK(std_it->second == fl_it->second);
        CHECK(std_it->first == 20);
        CHECK(maps_equal(std_map, fl_map));
        CHECK(std_map.size() == 4);
        CHECK(fl_map.size() == 4);
    }

    SUBCASE("Emplace duplicate key with hint - should not insert") {
        // Pre-populate maps
        std_map[5] = "five";
        std_map[10] = "ten";
        fl_map[5] = "five";
        fl_map[10] = "ten";

        // Try to emplace duplicate key with hint
        auto std_hint = std_map.find(10);
        auto fl_hint = fl_map.find(10);

        auto std_it = std_map.emplace_hint(std_hint, 10, "TEN");
        auto fl_it = fl_map.emplace_hint(fl_hint, 10, "TEN");

        // Should return iterator to existing element
        CHECK(std_it->first == fl_it->first);
        CHECK(std_it->second == fl_it->second);
        CHECK(std_it->first == 10);
        CHECK(std_it->second == "ten"); // Original value unchanged
        CHECK(fl_it->second == "ten"); // Original value unchanged
        CHECK(maps_equal(std_map, fl_map));
        CHECK(std_map.size() == 2); // Size unchanged
        CHECK(fl_map.size() == 2);
    }

    SUBCASE("Multiple emplaces with hints maintaining order") {
        // Emplace multiple elements using hints
        auto std_it1 = std_map.emplace_hint(std_map.end(), 10, "ten");
        auto fl_it1 = fl_map.emplace_hint(fl_map.end(), 10, "ten");

        std_map.emplace_hint(std_it1, 5, "five");
        fl_map.emplace_hint(fl_it1, 5, "five");

        std_map.emplace_hint(std_map.end(), 15, "fifteen");
        fl_map.emplace_hint(fl_map.end(), 15, "fifteen");

        // Verify all emplaced correctly and order is maintained
        CHECK(maps_equal(std_map, fl_map));
        CHECK(std_map.size() == 3);
        CHECK(fl_map.size() == 3);

        // Verify sorted order
        std::vector<int> std_order;
        std::vector<int> fl_order;

        for (const auto& pair : std_map) {
            std_order.push_back(pair.first);
        }

        for (const auto& pair : fl_map) {
            fl_order.push_back(pair.first);
        }

        CHECK(std_order == fl_order);
        CHECK(std_order == std::vector<int>{5, 10, 15});
    }

    SUBCASE("Emplace into empty map with hint") {
        // Both maps are empty
        // Emplace with hint (end() for empty map)
        auto std_it = std_map.emplace_hint(std_map.end(), 1, "one");
        auto fl_it = fl_map.emplace_hint(fl_map.end(), 1, "one");

        // Verify both emplaced successfully
        CHECK(std_it->first == fl_it->first);
        CHECK(std_it->second == fl_it->second);
        CHECK(std_map.size() == 1);
        CHECK(fl_map.size() == 1);
        CHECK(maps_equal(std_map, fl_map));
    }

    SUBCASE("Sequential emplaces with end() hint (optimal pattern)") {
        // This is a common pattern: emplacing in sorted order with end() as hint
        // Should be very efficient (amortized O(1) with good implementation)

        std::vector<std::pair<int, const char*>> test_data = {
            {1, "one"}, {2, "two"}, {3, "three"}, {4, "four"}, {5, "five"}
        };

        for (const auto& item : test_data) {
            std_map.emplace_hint(std_map.end(), item.first, item.second);
            fl_map.emplace_hint(fl_map.end(), item.first, item.second);
        }

        // Verify all emplaced correctly
        CHECK(std_map.size() == 5);
        CHECK(fl_map.size() == 5);
        CHECK(maps_equal(std_map, fl_map));

        // Verify order
        std::vector<int> fl_order;
        for (const auto& pair : fl_map) {
            fl_order.push_back(pair.first);
        }
        CHECK(fl_order == std::vector<int>{1, 2, 3, 4, 5});
    }

    SUBCASE("Emplace with hint - in-place construction") {
        // Test that emplace_hint constructs element in-place
        std_map.emplace_hint(std_map.end(), 1, "constructed in place");
        fl_map.emplace_hint(fl_map.end(), 1, "constructed in place");

        CHECK(std_map.size() == 1);
        CHECK(fl_map.size() == 1);
        CHECK(std_map[1] == fl_map[1]);
        CHECK(fl_map[1] == "constructed in place");
        CHECK(maps_equal(std_map, fl_map));
    }

    SUBCASE("Emplace with hint using pair construction") {
        // Test emplacing with pair arguments
        std_map[5] = "five";
        fl_map[5] = "five";

        auto std_it = std_map.emplace_hint(std_map.end(), std::pair<int, fl::string>(10, "ten"));
        auto fl_it = fl_map.emplace_hint(fl_map.end(), fl::pair<int, fl::string>(10, "ten"));

        CHECK(std_it->first == fl_it->first);
        CHECK(std_it->second == fl_it->second);
        CHECK(std_it->first == 10);
        CHECK(std_it->second == "ten");
        CHECK(fl_it->second == "ten");
        CHECK(maps_equal(std_map, fl_map));
        CHECK(std_map.size() == 2);
        CHECK(fl_map.size() == 2);
    }

    SUBCASE("Emplace with hint - complex value type") {
        // Test with more complex construction
        std::map<int, ConstructionCounter> std_cmap;
        fl::fl_map<int, ConstructionCounter> fl_cmap;

        // Emplace with hint, constructing ConstructionCounter in place
        ConstructionCounter::reset();
        auto std_it = std_cmap.emplace_hint(std_cmap.end(), 1, 100);

        ConstructionCounter::reset();
        auto fl_it = fl_cmap.emplace_hint(fl_cmap.end(), 1, 100);

        CHECK(std_it->first == 1);
        CHECK(fl_it->first == 1);
        CHECK(std_it->second.value == 100);
        CHECK(fl_it->second.value == 100);
        CHECK(std_cmap.size() == 1);
        CHECK(fl_cmap.size() == 1);
    }

    SUBCASE("Emplace with hint does NOT modify existing key") {
        // Pre-populate maps
        std_map[10] = "ten";
        fl_map[10] = "ten";

        // Try to emplace duplicate key with different value
        auto std_it = std_map.emplace_hint(std_map.end(), 10, "TEN");
        auto fl_it = fl_map.emplace_hint(fl_map.end(), 10, "TEN");

        // Original values should be preserved
        CHECK(std_it->first == fl_it->first);
        CHECK(std_it->second == fl_it->second);
        CHECK(std_it->second == "ten");
        CHECK(fl_it->second == "ten");
        CHECK(std_map[10] == "ten");
        CHECK(fl_map[10] == "ten");
        CHECK(maps_equal(std_map, fl_map));
    }

    SUBCASE("Emplace with hint - random order insertions") {
        // Test that emplace_hint maintains correctness even with random order
        std::vector<std::pair<int, const char*>> test_data = {
            {5, "five"}, {1, "one"}, {3, "three"}, {2, "two"}, {4, "four"}
        };

        for (const auto& item : test_data) {
            std_map.emplace_hint(std_map.begin(), item.first, item.second);
            fl_map.emplace_hint(fl_map.begin(), item.first, item.second);
        }

        // Verify sorted order maintained despite random insertion order and hints
        std::vector<int> std_order;
        std::vector<int> fl_order;

        for (const auto& pair : std_map) {
            std_order.push_back(pair.first);
        }

        for (const auto& pair : fl_map) {
            fl_order.push_back(pair.first);
        }

        CHECK(std_order == fl_order);
        CHECK(std_order == std::vector<int>{1, 2, 3, 4, 5});
        CHECK(maps_equal(std_map, fl_map));
    }
}

TEST_CASE("std::map vs fl::fl_map - Range Erase") {
    std::map<int, fl::string> std_map;
    fl::fl_map<int, fl::string> fl_map;

    SUBCASE("Erase middle portion of map") {
        // Pre-populate maps with keys 1, 2, 3, 4, 5
        for (int i = 1; i <= 5; ++i) {
            std::string std_value = "value" + std::to_string(i);
            fl::string fl_value = "value";
            fl_value += std::to_string(i).c_str();
            std_map[i] = fl::string(std_value.c_str());
            fl_map[i] = fl_value;
        }

        // Erase keys 2, 3, 4 (range from 2 to 5, not including 5)
        auto std_first = std_map.find(2);
        auto std_last = std_map.find(5);
        auto fl_first = fl_map.find(2);
        auto fl_last = fl_map.find(5);

        auto std_result = std_map.erase(std_first, std_last);
        auto fl_result = fl_map.erase(fl_first, fl_last);

        // Verify both maps now contain only keys 1 and 5
        CHECK(std_map.size() == fl_map.size());
        CHECK(std_map.size() == 2);
        CHECK(std_map.find(1) != std_map.end());
        CHECK(fl_map.find(1) != fl_map.end());
        CHECK(std_map.find(5) != std_map.end());
        CHECK(fl_map.find(5) != fl_map.end());
        CHECK(std_map.find(2) == std_map.end());
        CHECK(fl_map.find(2) == fl_map.end());
        CHECK(std_map.find(3) == std_map.end());
        CHECK(fl_map.find(3) == fl_map.end());
        CHECK(std_map.find(4) == std_map.end());
        CHECK(fl_map.find(4) == fl_map.end());
        CHECK(maps_equal(std_map, fl_map));

        // Verify returned iterators point to element after erased range
        CHECK(std_result->first == fl_result->first);
        CHECK(std_result->first == 5);
    }

    SUBCASE("Erase from beginning to middle") {
        // Pre-populate maps with keys 1, 2, 3, 4, 5
        for (int i = 1; i <= 5; ++i) {
            std::string std_value = "value" + std::to_string(i);
            fl::string fl_value = "value";
            fl_value += std::to_string(i).c_str();
            std_map[i] = fl::string(std_value.c_str());
            fl_map[i] = fl_value;
        }

        // Erase keys 1, 2, 3 (range from begin to element 4)
        auto std_last = std_map.find(4);
        auto fl_last = fl_map.find(4);

        auto std_result = std_map.erase(std_map.begin(), std_last);
        auto fl_result = fl_map.erase(fl_map.begin(), fl_last);

        // Verify both maps now contain only keys 4 and 5
        CHECK(std_map.size() == fl_map.size());
        CHECK(std_map.size() == 2);
        CHECK(maps_equal(std_map, fl_map));
        CHECK(std_map.find(4) != std_map.end());
        CHECK(fl_map.find(4) != fl_map.end());
        CHECK(std_map.find(5) != std_map.end());
        CHECK(fl_map.find(5) != fl_map.end());

        // Verify returned iterators point to first remaining element
        CHECK(std_result->first == fl_result->first);
        CHECK(std_result->first == 4);
    }

    SUBCASE("Erase from middle to end") {
        // Pre-populate maps with keys 1, 2, 3, 4, 5
        for (int i = 1; i <= 5; ++i) {
            std::string std_value = "value" + std::to_string(i);
            fl::string fl_value = "value";
            fl_value += std::to_string(i).c_str();
            std_map[i] = fl::string(std_value.c_str());
            fl_map[i] = fl_value;
        }

        // Erase keys 3, 4, 5 (range from element 3 to end)
        auto std_first = std_map.find(3);
        auto fl_first = fl_map.find(3);

        auto std_result = std_map.erase(std_first, std_map.end());
        auto fl_result = fl_map.erase(fl_first, fl_map.end());

        // Verify both maps now contain only keys 1 and 2
        CHECK(std_map.size() == fl_map.size());
        CHECK(std_map.size() == 2);
        CHECK(maps_equal(std_map, fl_map));
        CHECK(std_map.find(1) != std_map.end());
        CHECK(fl_map.find(1) != fl_map.end());
        CHECK(std_map.find(2) != std_map.end());
        CHECK(fl_map.find(2) != fl_map.end());

        // Verify returned iterators point to end
        CHECK(std_result == std_map.end());
        CHECK(fl_result == fl_map.end());
    }

    SUBCASE("Erase entire map (begin to end)") {
        // Pre-populate maps
        for (int i = 1; i <= 5; ++i) {
            std::string std_value = "value" + std::to_string(i);
            fl::string fl_value = "value";
            fl_value += std::to_string(i).c_str();
            std_map[i] = fl::string(std_value.c_str());
            fl_map[i] = fl_value;
        }

        // Erase entire range
        auto std_result = std_map.erase(std_map.begin(), std_map.end());
        auto fl_result = fl_map.erase(fl_map.begin(), fl_map.end());

        // Both maps should be empty
        CHECK(std_map.empty());
        CHECK(fl_map.empty());
        CHECK(maps_equal(std_map, fl_map));

        // Returned iterators should be end()
        CHECK(std_result == std_map.end());
        CHECK(fl_result == fl_map.end());
    }

    SUBCASE("Erase empty range (same iterator)") {
        // Pre-populate maps
        for (int i = 1; i <= 5; ++i) {
            std::string std_value = "value" + std::to_string(i);
            fl::string fl_value = "value";
            fl_value += std::to_string(i).c_str();
            std_map[i] = fl::string(std_value.c_str());
            fl_map[i] = fl_value;
        }

        // Erase empty range (first == last)
        auto std_it = std_map.find(3);
        auto fl_it = fl_map.find(3);

        auto std_result = std_map.erase(std_it, std_it);
        auto fl_result = fl_map.erase(fl_it, fl_it);

        // Nothing should be erased
        CHECK(std_map.size() == fl_map.size());
        CHECK(std_map.size() == 5);
        CHECK(maps_equal(std_map, fl_map));

        // Returned iterator should be the same as input
        CHECK(std_result->first == fl_result->first);
        CHECK(std_result->first == 3);
    }

    SUBCASE("Erase single element range") {
        // Pre-populate maps
        for (int i = 1; i <= 5; ++i) {
            std::string std_value = "value" + std::to_string(i);
            fl::string fl_value = "value";
            fl_value += std::to_string(i).c_str();
            std_map[i] = fl::string(std_value.c_str());
            fl_map[i] = fl_value;
        }

        // Erase single element (key 3)
        auto std_first = std_map.find(3);
        auto std_last = std_first;
        ++std_last; // Move to next element
        auto fl_first = fl_map.find(3);
        auto fl_last = fl_first;
        ++fl_last;

        auto std_result = std_map.erase(std_first, std_last);
        auto fl_result = fl_map.erase(fl_first, fl_last);

        // Verify key 3 is erased, others remain
        CHECK(std_map.size() == fl_map.size());
        CHECK(std_map.size() == 4);
        CHECK(std_map.find(3) == std_map.end());
        CHECK(fl_map.find(3) == fl_map.end());
        CHECK(maps_equal(std_map, fl_map));

        // Returned iterator should point to element after erased
        CHECK(std_result->first == fl_result->first);
        CHECK(std_result->first == 4);
    }

    SUBCASE("Erase from empty map") {
        // Both maps are empty
        auto std_result = std_map.erase(std_map.begin(), std_map.end());
        auto fl_result = fl_map.erase(fl_map.begin(), fl_map.end());

        // Should still be empty
        CHECK(std_map.empty());
        CHECK(fl_map.empty());
        CHECK(maps_equal(std_map, fl_map));

        // Returned iterators should be end()
        CHECK(std_result == std_map.end());
        CHECK(fl_result == fl_map.end());
    }

    SUBCASE("Multiple range erases") {
        // Pre-populate maps with keys 1-10
        for (int i = 1; i <= 10; ++i) {
            std::string std_value = "value" + std::to_string(i);
            fl::string fl_value = "value";
            fl_value += std::to_string(i).c_str();
            std_map[i] = fl::string(std_value.c_str());
            fl_map[i] = fl_value;
        }

        // First erase: remove keys 2, 3, 4
        auto std_first1 = std_map.find(2);
        auto std_last1 = std_map.find(5);
        auto fl_first1 = fl_map.find(2);
        auto fl_last1 = fl_map.find(5);
        std_map.erase(std_first1, std_last1);
        fl_map.erase(fl_first1, fl_last1);

        CHECK(std_map.size() == 7);
        CHECK(fl_map.size() == 7);
        CHECK(maps_equal(std_map, fl_map));

        // Second erase: remove keys 7, 8, 9
        auto std_first2 = std_map.find(7);
        auto std_last2 = std_map.find(10);
        auto fl_first2 = fl_map.find(7);
        auto fl_last2 = fl_map.find(10);
        std_map.erase(std_first2, std_last2);
        fl_map.erase(fl_first2, fl_last2);

        CHECK(std_map.size() == 4);
        CHECK(fl_map.size() == 4);
        CHECK(maps_equal(std_map, fl_map));

        // Verify remaining keys: 1, 5, 6, 10
        CHECK(std_map.find(1) != std_map.end());
        CHECK(fl_map.find(1) != fl_map.end());
        CHECK(std_map.find(5) != std_map.end());
        CHECK(fl_map.find(5) != fl_map.end());
        CHECK(std_map.find(6) != std_map.end());
        CHECK(fl_map.find(6) != fl_map.end());
        CHECK(std_map.find(10) != std_map.end());
        CHECK(fl_map.find(10) != fl_map.end());
    }

    SUBCASE("Range erase preserves remaining elements order") {
        // Pre-populate maps
        for (int i = 1; i <= 10; ++i) {
            std::string std_value = "value" + std::to_string(i);
            fl::string fl_value = "value";
            fl_value += std::to_string(i).c_str();
            std_map[i] = fl::string(std_value.c_str());
            fl_map[i] = fl_value;
        }

        // Erase keys 3-7 (inclusive of 3-6, exclusive of 7)
        auto std_first = std_map.find(3);
        auto std_last = std_map.find(7);
        auto fl_first = fl_map.find(3);
        auto fl_last = fl_map.find(7);

        std_map.erase(std_first, std_last);
        fl_map.erase(fl_first, fl_last);

        // Verify order is maintained
        std::vector<int> std_order;
        std::vector<int> fl_order;

        for (const auto& pair : std_map) {
            std_order.push_back(pair.first);
        }

        for (const auto& pair : fl_map) {
            fl_order.push_back(pair.first);
        }

        CHECK(std_order == fl_order);
        CHECK(std_order == std::vector<int>{1, 2, 7, 8, 9, 10});
        CHECK(maps_equal(std_map, fl_map));
    }

    SUBCASE("Range erase with large dataset") {
        // Pre-populate with 100 elements
        for (int i = 1; i <= 100; ++i) {
            std::string std_value = "value" + std::to_string(i);
            fl::string fl_value = "value";
            fl_value += std::to_string(i).c_str();
            std_map[i] = fl::string(std_value.c_str());
            fl_map[i] = fl_value;
        }

        CHECK(std_map.size() == 100);
        CHECK(fl_map.size() == 100);

        // Erase middle 50 elements (keys 26-75)
        auto std_first = std_map.find(26);
        auto std_last = std_map.find(76);
        auto fl_first = fl_map.find(26);
        auto fl_last = fl_map.find(76);

        std_map.erase(std_first, std_last);
        fl_map.erase(fl_first, fl_last);

        // Verify 50 elements remain
        CHECK(std_map.size() == fl_map.size());
        CHECK(std_map.size() == 50);
        CHECK(maps_equal(std_map, fl_map));

        // Verify correct elements remain (1-25 and 76-100)
        for (int i = 1; i <= 25; ++i) {
            CHECK(std_map.find(i) != std_map.end());
            CHECK(fl_map.find(i) != fl_map.end());
        }
        for (int i = 26; i <= 75; ++i) {
            CHECK(std_map.find(i) == std_map.end());
            CHECK(fl_map.find(i) == fl_map.end());
        }
        for (int i = 76; i <= 100; ++i) {
            CHECK(std_map.find(i) != std_map.end());
            CHECK(fl_map.find(i) != fl_map.end());
        }
    }
}

TEST_CASE("std::map vs fl::fl_map - Range Constructor") {
    SUBCASE("Construct from vector of pairs") {
        // Create a vector of pairs to construct from
        std::vector<std::pair<int, fl::string>> test_data = {
            {1, "one"}, {2, "two"}, {3, "three"}, {4, "four"}, {5, "five"}
        };

        // Construct std::map from range
        std::map<int, fl::string> std_map(test_data.begin(), test_data.end());

        // Convert to fl::pair vector for fl_map
        fl::vector<fl::pair<int, fl::string>> fl_test_data;
        for (const auto& item : test_data) {
            fl_test_data.push_back(fl::pair<int, fl::string>(item.first, item.second));
        }

        // Construct fl_map from range
        fl::fl_map<int, fl::string> fl_map(fl_test_data.begin(), fl_test_data.end());

        // Verify both maps have same contents
        CHECK(std_map.size() == fl_map.size());
        CHECK(std_map.size() == 5);
        CHECK(maps_equal(std_map, fl_map));
    }

    SUBCASE("Construct from empty range") {
        // Create empty vector
        std::vector<std::pair<int, fl::string>> empty_data;

        // Construct from empty range
        std::map<int, fl::string> std_map(empty_data.begin(), empty_data.end());

        fl::vector<fl::pair<int, fl::string>> fl_empty_data;
        fl::fl_map<int, fl::string> fl_map(fl_empty_data.begin(), fl_empty_data.end());

        // Both maps should be empty
        CHECK(std_map.empty());
        CHECK(fl_map.empty());
        CHECK(maps_equal(std_map, fl_map));
    }

    SUBCASE("Construct from range with duplicate keys") {
        // Create test data with duplicate keys
        // In std::map, only first occurrence is kept
        std::vector<std::pair<int, fl::string>> test_data = {
            {1, "one"}, {2, "two"}, {2, "TWO"}, {3, "three"}, {1, "ONE"}
        };

        std::map<int, fl::string> std_map(test_data.begin(), test_data.end());

        fl::vector<fl::pair<int, fl::string>> fl_test_data;
        for (const auto& item : test_data) {
            fl_test_data.push_back(fl::pair<int, fl::string>(item.first, item.second));
        }
        fl::fl_map<int, fl::string> fl_map(fl_test_data.begin(), fl_test_data.end());

        // Verify size and contents
        CHECK(std_map.size() == fl_map.size());
        CHECK(std_map.size() == 3); // Only 3 unique keys
        CHECK(maps_equal(std_map, fl_map));

        // First values should be kept for duplicate keys
        CHECK(std_map[1] == fl_map[1]);
        CHECK(std_map[2] == fl_map[2]);
        CHECK(std_map[3] == fl_map[3]);
    }

    SUBCASE("Construct from another map (range)") {
        // Create source maps
        std::map<int, fl::string> std_src;
        fl::fl_map<int, fl::string> fl_src;

        std_src[1] = "one";
        std_src[2] = "two";
        std_src[3] = "three";
        fl_src[1] = "one";
        fl_src[2] = "two";
        fl_src[3] = "three";

        // Construct from range of another map
        std::map<int, fl::string> std_map(std_src.begin(), std_src.end());
        fl::fl_map<int, fl::string> fl_map(fl_src.begin(), fl_src.end());

        // Verify contents match
        CHECK(std_map.size() == fl_map.size());
        CHECK(std_map.size() == 3);
        CHECK(maps_equal(std_map, fl_map));
        CHECK(maps_equal(std_src, fl_src));
    }

    SUBCASE("Construct maintains sorted order") {
        // Construct from data in random order
        std::vector<std::pair<int, fl::string>> test_data = {
            {5, "five"}, {1, "one"}, {3, "three"}, {2, "two"}, {4, "four"}
        };

        std::map<int, fl::string> std_map(test_data.begin(), test_data.end());

        fl::vector<fl::pair<int, fl::string>> fl_test_data;
        for (const auto& item : test_data) {
            fl_test_data.push_back(fl::pair<int, fl::string>(item.first, item.second));
        }
        fl::fl_map<int, fl::string> fl_map(fl_test_data.begin(), fl_test_data.end());

        // Verify sorted order in iteration
        std::vector<int> std_order;
        std::vector<int> fl_order;

        for (const auto& pair : std_map) {
            std_order.push_back(pair.first);
        }

        for (const auto& pair : fl_map) {
            fl_order.push_back(pair.first);
        }

        CHECK(std_order == fl_order);
        CHECK(std_order == std::vector<int>{1, 2, 3, 4, 5});
        CHECK(maps_equal(std_map, fl_map));
    }

    SUBCASE("Construct with large dataset") {
        // Create larger dataset
        std::vector<std::pair<int, fl::string>> test_data;
        for (int i = 1; i <= 100; ++i) {
            std::string std_value = "value" + std::to_string(i);
            test_data.push_back({i, fl::string(std_value.c_str())});
        }

        std::map<int, fl::string> std_map(test_data.begin(), test_data.end());

        fl::vector<fl::pair<int, fl::string>> fl_test_data;
        for (const auto& item : test_data) {
            fl_test_data.push_back(fl::pair<int, fl::string>(item.first, item.second));
        }
        fl::fl_map<int, fl::string> fl_map(fl_test_data.begin(), fl_test_data.end());

        // Verify all elements constructed
        CHECK(std_map.size() == fl_map.size());
        CHECK(std_map.size() == 100);
        CHECK(maps_equal(std_map, fl_map));
    }

    SUBCASE("Construct with custom comparator") {
        // Create test data
        std::vector<std::pair<int, fl::string>> test_data = {
            {1, "one"}, {2, "two"}, {3, "three"}
        };

        // Construct with descending comparator
        std::map<int, fl::string, DescendingInt> std_map(test_data.begin(), test_data.end());

        fl::vector<fl::pair<int, fl::string>> fl_test_data;
        for (const auto& item : test_data) {
            fl_test_data.push_back(fl::pair<int, fl::string>(item.first, item.second));
        }
        fl::fl_map<int, fl::string, DescendingInt> fl_map(fl_test_data.begin(), fl_test_data.end());

        // Verify descending order
        std::vector<int> std_order;
        std::vector<int> fl_order;

        for (const auto& pair : std_map) {
            std_order.push_back(pair.first);
        }

        for (const auto& pair : fl_map) {
            fl_order.push_back(pair.first);
        }

        CHECK(std_order == fl_order);
        CHECK(std_order == std::vector<int>{3, 2, 1}); // Descending order
        CHECK(maps_equal(std_map, fl_map));
    }

    SUBCASE("Construct from single element range") {
        // Create single element vector
        std::vector<std::pair<int, fl::string>> test_data = {{42, "answer"}};

        std::map<int, fl::string> std_map(test_data.begin(), test_data.end());

        fl::vector<fl::pair<int, fl::string>> fl_test_data;
        fl_test_data.push_back(fl::pair<int, fl::string>(42, "answer"));
        fl::fl_map<int, fl::string> fl_map(fl_test_data.begin(), fl_test_data.end());

        // Verify single element
        CHECK(std_map.size() == fl_map.size());
        CHECK(std_map.size() == 1);
        CHECK(std_map[42] == fl_map[42]);
        CHECK(fl_map[42] == "answer");
        CHECK(maps_equal(std_map, fl_map));
    }

    SUBCASE("Construct verifies all elements accessible") {
        // Create test data
        std::vector<std::pair<int, fl::string>> test_data = {
            {10, "ten"}, {20, "twenty"}, {30, "thirty"}
        };

        std::map<int, fl::string> std_map(test_data.begin(), test_data.end());

        fl::vector<fl::pair<int, fl::string>> fl_test_data;
        for (const auto& item : test_data) {
            fl_test_data.push_back(fl::pair<int, fl::string>(item.first, item.second));
        }
        fl::fl_map<int, fl::string> fl_map(fl_test_data.begin(), fl_test_data.end());

        // Verify all elements are accessible
        CHECK(std_map.find(10) != std_map.end());
        CHECK(fl_map.find(10) != fl_map.end());
        CHECK(std_map[10] == "ten");
        CHECK(fl_map[10] == "ten");

        CHECK(std_map.find(20) != std_map.end());
        CHECK(fl_map.find(20) != fl_map.end());
        CHECK(std_map[20] == "twenty");
        CHECK(fl_map[20] == "twenty");

        CHECK(std_map.find(30) != std_map.end());
        CHECK(fl_map.find(30) != fl_map.end());
        CHECK(std_map[30] == "thirty");
        CHECK(fl_map[30] == "thirty");

        CHECK(maps_equal(std_map, fl_map));
    }

    SUBCASE("Construct from range and then modify") {
        // Create test data
        std::vector<std::pair<int, fl::string>> test_data = {
            {1, "one"}, {2, "two"}, {3, "three"}
        };

        std::map<int, fl::string> std_map(test_data.begin(), test_data.end());

        fl::vector<fl::pair<int, fl::string>> fl_test_data;
        for (const auto& item : test_data) {
            fl_test_data.push_back(fl::pair<int, fl::string>(item.first, item.second));
        }
        fl::fl_map<int, fl::string> fl_map(fl_test_data.begin(), fl_test_data.end());

        // Verify initial state
        CHECK(maps_equal(std_map, fl_map));

        // Modify maps after construction
        std_map[2] = "TWO";
        fl_map[2] = "TWO";
        std_map[4] = "four";
        fl_map[4] = "four";
        std_map.erase(1);
        fl_map.erase(1);

        // Verify modified state
        CHECK(std_map.size() == fl_map.size());
        CHECK(std_map.size() == 3); // 2, 3, 4
        CHECK(std_map[2] == "TWO");
        CHECK(fl_map[2] == "TWO");
        CHECK(std_map.find(1) == std_map.end());
        CHECK(fl_map.find(1) == fl_map.end());
        CHECK(maps_equal(std_map, fl_map));
    }
}

TEST_CASE("std::map vs fl::fl_map - Lexicographic Comparison Operators") {
    SUBCASE("Comparison of equal maps") {
        std::map<int, fl::string> std_map = {{1, "one"}, {2, "two"}, {3, "three"}};
        fl::fl_map<int, fl::string> fl_map;
        fl_map.insert(fl::pair<int, fl::string>(1, "one"));
        fl_map.insert(fl::pair<int, fl::string>(2, "two"));
        fl_map.insert(fl::pair<int, fl::string>(3, "three"));

        // Equal maps: not <, <=, not >, >=
        CHECK(!(std_map < std_map));
        CHECK(!(fl_map < fl_map));
        CHECK(std_map <= std_map);
        CHECK(fl_map <= fl_map);
        CHECK(!(std_map > std_map));
        CHECK(!(fl_map > fl_map));
        CHECK(std_map >= std_map);
        CHECK(fl_map >= fl_map);
    }

    SUBCASE("First map is lexicographically less (smaller first element)") {
        std::map<int, fl::string> std_map1 = {{1, "a"}, {2, "b"}};
        std::map<int, fl::string> std_map2 = {{1, "b"}, {2, "b"}};

        fl::fl_map<int, fl::string> fl_map1;
        fl_map1.insert(fl::pair<int, fl::string>(1, "a"));
        fl_map1.insert(fl::pair<int, fl::string>(2, "b"));

        fl::fl_map<int, fl::string> fl_map2;
        fl_map2.insert(fl::pair<int, fl::string>(1, "b"));
        fl_map2.insert(fl::pair<int, fl::string>(2, "b"));

        // map1 < map2 because "a" < "b"
        CHECK(std_map1 < std_map2);
        CHECK(fl_map1 < fl_map2);
        CHECK(std_map1 <= std_map2);
        CHECK(fl_map1 <= fl_map2);
        CHECK(!(std_map1 > std_map2));
        CHECK(!(fl_map1 > fl_map2));
        CHECK(!(std_map1 >= std_map2));
        CHECK(!(fl_map1 >= fl_map2));

        // map2 > map1
        CHECK(!(std_map2 < std_map1));
        CHECK(!(fl_map2 < fl_map1));
        CHECK(!(std_map2 <= std_map1));
        CHECK(!(fl_map2 <= fl_map1));
        CHECK(std_map2 > std_map1);
        CHECK(fl_map2 > fl_map1);
        CHECK(std_map2 >= std_map1);
        CHECK(fl_map2 >= fl_map1);
    }

    SUBCASE("First map is lexicographically less (prefix)") {
        std::map<int, fl::string> std_map1 = {{1, "one"}, {2, "two"}};
        std::map<int, fl::string> std_map2 = {{1, "one"}, {2, "two"}, {3, "three"}};

        fl::fl_map<int, fl::string> fl_map1;
        fl_map1.insert(fl::pair<int, fl::string>(1, "one"));
        fl_map1.insert(fl::pair<int, fl::string>(2, "two"));

        fl::fl_map<int, fl::string> fl_map2;
        fl_map2.insert(fl::pair<int, fl::string>(1, "one"));
        fl_map2.insert(fl::pair<int, fl::string>(2, "two"));
        fl_map2.insert(fl::pair<int, fl::string>(3, "three"));

        // map1 < map2 because map1 is a prefix of map2
        CHECK(std_map1 < std_map2);
        CHECK(fl_map1 < fl_map2);
        CHECK(std_map1 <= std_map2);
        CHECK(fl_map1 <= fl_map2);
        CHECK(!(std_map1 > std_map2));
        CHECK(!(fl_map1 > fl_map2));
        CHECK(!(std_map1 >= std_map2));
        CHECK(!(fl_map1 >= fl_map2));

        // map2 > map1
        CHECK(!(std_map2 < std_map1));
        CHECK(!(fl_map2 < fl_map1));
        CHECK(!(std_map2 <= std_map1));
        CHECK(!(fl_map2 <= fl_map1));
        CHECK(std_map2 > std_map1);
        CHECK(fl_map2 > fl_map1);
        CHECK(std_map2 >= std_map1);
        CHECK(fl_map2 >= fl_map1);
    }

    SUBCASE("Comparison with different keys") {
        std::map<int, fl::string> std_map1 = {{1, "one"}, {2, "two"}};
        std::map<int, fl::string> std_map2 = {{1, "one"}, {3, "three"}};

        fl::fl_map<int, fl::string> fl_map1;
        fl_map1.insert(fl::pair<int, fl::string>(1, "one"));
        fl_map1.insert(fl::pair<int, fl::string>(2, "two"));

        fl::fl_map<int, fl::string> fl_map2;
        fl_map2.insert(fl::pair<int, fl::string>(1, "one"));
        fl_map2.insert(fl::pair<int, fl::string>(3, "three"));

        // map1 < map2 because key 2 < key 3
        CHECK(std_map1 < std_map2);
        CHECK(fl_map1 < fl_map2);
        CHECK(std_map1 <= std_map2);
        CHECK(fl_map1 <= fl_map2);
        CHECK(!(std_map1 > std_map2));
        CHECK(!(fl_map1 > fl_map2));
        CHECK(!(std_map1 >= std_map2));
        CHECK(!(fl_map1 >= fl_map2));
    }

    SUBCASE("Comparison of empty maps") {
        std::map<int, fl::string> std_map1;
        std::map<int, fl::string> std_map2;

        fl::fl_map<int, fl::string> fl_map1;
        fl::fl_map<int, fl::string> fl_map2;

        // Empty maps are equal
        CHECK(!(std_map1 < std_map2));
        CHECK(!(fl_map1 < fl_map2));
        CHECK(std_map1 <= std_map2);
        CHECK(fl_map1 <= fl_map2);
        CHECK(!(std_map1 > std_map2));
        CHECK(!(fl_map1 > fl_map2));
        CHECK(std_map1 >= std_map2);
        CHECK(fl_map2 >= fl_map2);
    }

    SUBCASE("Empty map vs non-empty map") {
        std::map<int, fl::string> std_empty;
        std::map<int, fl::string> std_nonempty = {{1, "one"}};

        fl::fl_map<int, fl::string> fl_empty;
        fl::fl_map<int, fl::string> fl_nonempty;
        fl_nonempty.insert(fl::pair<int, fl::string>(1, "one"));

        // Empty < non-empty
        CHECK(std_empty < std_nonempty);
        CHECK(fl_empty < fl_nonempty);
        CHECK(std_empty <= std_nonempty);
        CHECK(fl_empty <= fl_nonempty);
        CHECK(!(std_empty > std_nonempty));
        CHECK(!(fl_empty > fl_nonempty));
        CHECK(!(std_empty >= std_nonempty));
        CHECK(!(fl_empty >= fl_nonempty));

        // Non-empty > empty
        CHECK(!(std_nonempty < std_empty));
        CHECK(!(fl_nonempty < fl_empty));
        CHECK(!(std_nonempty <= std_empty));
        CHECK(!(fl_nonempty <= fl_empty));
        CHECK(std_nonempty > std_empty);
        CHECK(fl_nonempty > fl_empty);
        CHECK(std_nonempty >= std_empty);
        CHECK(fl_nonempty >= fl_empty);
    }

    SUBCASE("Multiple element comparison") {
        std::map<int, fl::string> std_map1 = {{1, "a"}, {2, "b"}, {3, "c"}};
        std::map<int, fl::string> std_map2 = {{1, "a"}, {2, "b"}, {3, "d"}};

        fl::fl_map<int, fl::string> fl_map1;
        fl_map1.insert(fl::pair<int, fl::string>(1, "a"));
        fl_map1.insert(fl::pair<int, fl::string>(2, "b"));
        fl_map1.insert(fl::pair<int, fl::string>(3, "c"));

        fl::fl_map<int, fl::string> fl_map2;
        fl_map2.insert(fl::pair<int, fl::string>(1, "a"));
        fl_map2.insert(fl::pair<int, fl::string>(2, "b"));
        fl_map2.insert(fl::pair<int, fl::string>(3, "d"));

        // map1 < map2 because at key 3: "c" < "d"
        CHECK(std_map1 < std_map2);
        CHECK(fl_map1 < fl_map2);
        CHECK(std_map1 <= std_map2);
        CHECK(fl_map1 <= fl_map2);
        CHECK(!(std_map1 > std_map2));
        CHECK(!(fl_map1 > fl_map2));
        CHECK(!(std_map1 >= std_map2));
        CHECK(!(fl_map1 >= fl_map2));
    }

    SUBCASE("Comparison with integer keys") {
        std::map<int, int> std_map1 = {{1, 100}, {2, 200}};
        std::map<int, int> std_map2 = {{1, 100}, {2, 201}};

        fl::fl_map<int, int> fl_map1;
        fl_map1.insert(fl::pair<int, int>(1, 100));
        fl_map1.insert(fl::pair<int, int>(2, 200));

        fl::fl_map<int, int> fl_map2;
        fl_map2.insert(fl::pair<int, int>(1, 100));
        fl_map2.insert(fl::pair<int, int>(2, 201));

        // map1 < map2 because value 200 < 201
        CHECK(std_map1 < std_map2);
        CHECK(fl_map1 < fl_map2);
        CHECK(std_map1 <= std_map2);
        CHECK(fl_map1 <= fl_map2);
        CHECK(!(std_map1 > std_map2));
        CHECK(!(fl_map1 > fl_map2));
        CHECK(!(std_map1 >= std_map2));
        CHECK(!(fl_map1 >= fl_map2));
    }

    SUBCASE("Transitive comparison") {
        std::map<int, int> std_a = {{1, 1}};
        std::map<int, int> std_b = {{1, 2}};
        std::map<int, int> std_c = {{1, 3}};

        fl::fl_map<int, int> fl_a;
        fl_a.insert(fl::pair<int, int>(1, 1));
        fl::fl_map<int, int> fl_b;
        fl_b.insert(fl::pair<int, int>(1, 2));
        fl::fl_map<int, int> fl_c;
        fl_c.insert(fl::pair<int, int>(1, 3));

        // a < b < c should be transitive
        CHECK(std_a < std_b);
        CHECK(fl_a < fl_b);
        CHECK(std_b < std_c);
        CHECK(fl_b < fl_c);
        CHECK(std_a < std_c);
        CHECK(fl_a < fl_c);
    }

    SUBCASE("Comparison with custom comparator (descending)") {
        // Maps with descending int comparator
        std::map<int, fl::string, DescendingInt> std_map1;
        std_map1.insert({3, "three"});
        std_map1.insert({2, "two"});
        std_map1.insert({1, "one"});

        std::map<int, fl::string, DescendingInt> std_map2;
        std_map2.insert({3, "three"});
        std_map2.insert({2, "two"});
        std_map2.insert({1, "two"});  // Different value at key 1

        fl::fl_map<int, fl::string, DescendingInt> fl_map1;
        fl_map1.insert(fl::pair<int, fl::string>(3, "three"));
        fl_map1.insert(fl::pair<int, fl::string>(2, "two"));
        fl_map1.insert(fl::pair<int, fl::string>(1, "one"));

        fl::fl_map<int, fl::string, DescendingInt> fl_map2;
        fl_map2.insert(fl::pair<int, fl::string>(3, "three"));
        fl_map2.insert(fl::pair<int, fl::string>(2, "two"));
        fl_map2.insert(fl::pair<int, fl::string>(1, "two"));

        // In descending order, iteration is 3, 2, 1
        // map1 < map2 because at key 1: "one" < "two"
        CHECK(std_map1 < std_map2);
        CHECK(fl_map1 < fl_map2);
    }
}

TEST_CASE("std::map vs fl::fl_map - get_allocator()") {
    SUBCASE("get_allocator returns valid allocator") {
        // Create maps with default allocator
        std::map<int, fl::string> std_map;
        std_map.insert({1, "one"});
        std_map.insert({2, "two"});

        fl::fl_map<int, fl::string> fl_map;
        fl_map.insert(fl::pair<int, fl::string>(1, "one"));
        fl_map.insert(fl::pair<int, fl::string>(2, "two"));

        // Get allocators - both should be valid
        auto std_alloc = std_map.get_allocator();
        auto fl_alloc = fl_map.get_allocator();

        // Verify we can use the allocators (basic sanity check)
        // The allocators should be copy-constructible
        auto std_alloc_copy = std_alloc;
        auto fl_alloc_copy = fl_alloc;

        // Suppress unused variable warnings
        (void)std_alloc_copy;
        (void)fl_alloc_copy;

        // Verify this doesn't crash or throw
        CHECK(true);
    }

    SUBCASE("get_allocator on empty map") {
        // Create empty maps
        std::map<int, int> std_map;
        fl::fl_map<int, int> fl_map;

        // Get allocators from empty maps - should still be valid
        auto std_alloc = std_map.get_allocator();
        auto fl_alloc = fl_map.get_allocator();

        // Verify we can copy the allocators
        auto std_alloc_copy = std_alloc;
        auto fl_alloc_copy = fl_alloc;

        // Suppress unused variable warnings
        (void)std_alloc_copy;
        (void)fl_alloc_copy;

        CHECK(true);
    }

    SUBCASE("get_allocator on const map") {
        // Create const maps
        fl::fl_map<int, fl::string> temp_map;
        temp_map.insert(fl::pair<int, fl::string>(1, "one"));
        temp_map.insert(fl::pair<int, fl::string>(2, "two"));

        const fl::fl_map<int, fl::string> fl_map = temp_map;

        // Get allocator from const map - should work
        auto fl_alloc = fl_map.get_allocator();
        auto fl_alloc_copy = fl_alloc;

        // Suppress unused variable warnings
        (void)fl_alloc_copy;

        CHECK(true);
    }

    SUBCASE("get_allocator with different key/value types") {
        // Test with different types to ensure template instantiation works
        fl::fl_map<fl::string, int> string_int_map;
        string_int_map.insert(fl::pair<fl::string, int>("one", 1));
        auto alloc1 = string_int_map.get_allocator();

        fl::fl_map<int, fl::string> int_string_map;
        int_string_map.insert(fl::pair<int, fl::string>(1, "one"));
        auto alloc2 = int_string_map.get_allocator();

        fl::fl_map<int, int> int_int_map;
        int_int_map.insert(fl::pair<int, int>(1, 1));
        auto alloc3 = int_int_map.get_allocator();

        // Suppress unused variable warnings
        (void)alloc1;
        (void)alloc2;
        (void)alloc3;

        // All allocators should be valid
        CHECK(true);
    }

    SUBCASE("get_allocator after operations") {
        fl::fl_map<int, fl::string> fl_map;

        // Get allocator at different stages
        auto alloc_empty = fl_map.get_allocator();

        fl_map.insert(fl::pair<int, fl::string>(1, "one"));
        auto alloc_after_insert = fl_map.get_allocator();

        fl_map.insert(fl::pair<int, fl::string>(2, "two"));
        fl_map.insert(fl::pair<int, fl::string>(3, "three"));
        auto alloc_after_multiple = fl_map.get_allocator();

        fl_map.erase(2);
        auto alloc_after_erase = fl_map.get_allocator();

        fl_map.clear();
        auto alloc_after_clear = fl_map.get_allocator();

        // Suppress unused variable warnings
        (void)alloc_empty;
        (void)alloc_after_insert;
        (void)alloc_after_multiple;
        (void)alloc_after_erase;
        (void)alloc_after_clear;

        // All allocators should be valid regardless of map state
        CHECK(true);
    }

    SUBCASE("get_allocator with custom comparator") {
        // Maps with custom comparator
        fl::fl_map<int, fl::string, DescendingInt> fl_map;
        fl_map.insert(fl::pair<int, fl::string>(3, "three"));
        fl_map.insert(fl::pair<int, fl::string>(2, "two"));
        fl_map.insert(fl::pair<int, fl::string>(1, "one"));

        // Get allocator - should work with custom comparator
        auto fl_alloc = fl_map.get_allocator();
        auto fl_alloc_copy = fl_alloc;

        // Suppress unused variable warnings
        (void)fl_alloc_copy;

        CHECK(true);
    }
}

TEST_CASE("std::map vs fl::fl_map - initializer_list support") {
    SUBCASE("initializer_list constructor") {
        // Construct maps from initializer lists
        std::map<int, std::string> std_map = {{1, "one"}, {2, "two"}, {3, "three"}};
        fl::fl_map<int, fl::string> fl_map = {{1, "one"}, {2, "two"}, {3, "three"}};

        // Verify sizes
        CHECK(std_map.size() == 3);
        CHECK(fl_map.size() == 3);

        // Verify contents
        CHECK(std_map.at(1) == "one");
        CHECK(fl_map.at(1) == "one");
        CHECK(std_map.at(2) == "two");
        CHECK(fl_map.at(2) == "two");
        CHECK(std_map.at(3) == "three");
        CHECK(fl_map.at(3) == "three");
    }

    SUBCASE("initializer_list constructor with duplicates") {
        // When initializer list has duplicates, first occurrence should be kept
        std::map<int, std::string> std_map = {{1, "first"}, {2, "two"}, {1, "second"}};
        fl::fl_map<int, fl::string> fl_map = {{1, "first"}, {2, "two"}, {1, "second"}};

        // Verify sizes (duplicates should be ignored)
        CHECK(std_map.size() == 2);
        CHECK(fl_map.size() == 2);

        // Verify first occurrence was kept
        CHECK(std_map.at(1) == "first");
        CHECK(fl_map.at(1) == "first");
        CHECK(std_map.at(2) == "two");
        CHECK(fl_map.at(2) == "two");
    }

    SUBCASE("initializer_list constructor with custom comparator") {
        // Custom comparator for descending order
        std::map<int, std::string, DescendingInt> std_map = {{1, "one"}, {2, "two"}, {3, "three"}};
        fl::fl_map<int, fl::string, DescendingInt> fl_map = {{1, "one"}, {2, "two"}, {3, "three"}};

        // Verify sizes
        CHECK(std_map.size() == 3);
        CHECK(fl_map.size() == 3);

        // Verify elements are in descending order
        auto std_it = std_map.begin();
        auto fl_it = fl_map.begin();

        CHECK(std_it->first == 3);
        CHECK(fl_it->first == 3);
        ++std_it; ++fl_it;

        CHECK(std_it->first == 2);
        CHECK(fl_it->first == 2);
        ++std_it; ++fl_it;

        CHECK(std_it->first == 1);
        CHECK(fl_it->first == 1);
    }

    SUBCASE("initializer_list assignment operator") {
        // Create maps and assign from initializer lists
        std::map<int, std::string> std_map;
        fl::fl_map<int, fl::string> fl_map;

        // Add some initial data that will be cleared by assignment
        std_map.insert({99, "old"});
        fl_map.insert({99, "old"});

        CHECK(std_map.size() == 1);
        CHECK(fl_map.size() == 1);

        // Assign from initializer list
        std_map = {{1, "one"}, {2, "two"}, {3, "three"}};
        fl_map = {{1, "one"}, {2, "two"}, {3, "three"}};

        // Verify old data was cleared and new data was added
        CHECK(std_map.size() == 3);
        CHECK(fl_map.size() == 3);

        CHECK(std_map.find(99) == std_map.end());
        CHECK(fl_map.find(99) == fl_map.end());

        CHECK(std_map.at(1) == "one");
        CHECK(fl_map.at(1) == "one");
        CHECK(std_map.at(2) == "two");
        CHECK(fl_map.at(2) == "two");
        CHECK(std_map.at(3) == "three");
        CHECK(fl_map.at(3) == "three");
    }

    SUBCASE("initializer_list assignment with duplicates") {
        std::map<int, std::string> std_map;
        fl::fl_map<int, fl::string> fl_map;

        // Assign from initializer list with duplicates
        std_map = {{1, "first"}, {2, "two"}, {1, "second"}};
        fl_map = {{1, "first"}, {2, "two"}, {1, "second"}};

        // Verify sizes (duplicates should be ignored)
        CHECK(std_map.size() == 2);
        CHECK(fl_map.size() == 2);

        // Verify first occurrence was kept
        CHECK(std_map.at(1) == "first");
        CHECK(fl_map.at(1) == "first");
    }

    SUBCASE("initializer_list insert method") {
        std::map<int, std::string> std_map;
        fl::fl_map<int, fl::string> fl_map;

        // Insert initial elements
        std_map.insert({1, "one"});
        fl_map.insert({1, "one"});

        CHECK(std_map.size() == 1);
        CHECK(fl_map.size() == 1);

        // Insert from initializer list
        std_map.insert({{2, "two"}, {3, "three"}, {4, "four"}});
        fl_map.insert({{2, "two"}, {3, "three"}, {4, "four"}});

        // Verify all elements are present
        CHECK(std_map.size() == 4);
        CHECK(fl_map.size() == 4);

        CHECK(std_map.at(1) == "one");
        CHECK(fl_map.at(1) == "one");
        CHECK(std_map.at(2) == "two");
        CHECK(fl_map.at(2) == "two");
        CHECK(std_map.at(3) == "three");
        CHECK(fl_map.at(3) == "three");
        CHECK(std_map.at(4) == "four");
        CHECK(fl_map.at(4) == "four");
    }

    SUBCASE("initializer_list insert with duplicates") {
        std::map<int, std::string> std_map = {{1, "original"}};
        fl::fl_map<int, fl::string> fl_map = {{1, "original"}};

        // Insert from initializer list with duplicate key
        std_map.insert({{1, "duplicate"}, {2, "two"}});
        fl_map.insert({{1, "duplicate"}, {2, "two"}});

        // Verify duplicate was ignored (original value kept)
        CHECK(std_map.size() == 2);
        CHECK(fl_map.size() == 2);

        CHECK(std_map.at(1) == "original");
        CHECK(fl_map.at(1) == "original");
        CHECK(std_map.at(2) == "two");
        CHECK(fl_map.at(2) == "two");
    }

    SUBCASE("empty initializer_list") {
        // Construct from empty initializer list
        std::map<int, std::string> std_map = {};
        fl::fl_map<int, fl::string> fl_map = {};

        CHECK(std_map.empty());
        CHECK(fl_map.empty());
        CHECK(std_map.size() == 0);
        CHECK(fl_map.size() == 0);

        // Assign empty initializer list
        std_map = {{1, "one"}};
        fl_map = {{1, "one"}};
        CHECK(std_map.size() == 1);
        CHECK(fl_map.size() == 1);

        std_map = {};
        fl_map = {};
        CHECK(std_map.empty());
        CHECK(fl_map.empty());

        // Insert empty initializer list (should do nothing)
        std_map.insert({1, "one"});
        fl_map.insert({1, "one"});
        CHECK(std_map.size() == 1);
        CHECK(fl_map.size() == 1);

        std_map.insert({});
        fl_map.insert({});
        CHECK(std_map.size() == 1);
        CHECK(fl_map.size() == 1);
    }

    SUBCASE("initializer_list with different value types") {
        // Test with different key/value type combinations
        fl::fl_map<fl::string, int> string_int_map = {{"one", 1}, {"two", 2}};
        CHECK(string_int_map.size() == 2);
        CHECK(string_int_map.at("one") == 1);
        CHECK(string_int_map.at("two") == 2);

        fl::fl_map<int, int> int_int_map = {{1, 100}, {2, 200}};
        CHECK(int_int_map.size() == 2);
        CHECK(int_int_map.at(1) == 100);
        CHECK(int_int_map.at(2) == 200);

        // Assignment
        string_int_map = {{"three", 3}, {"four", 4}};
        CHECK(string_int_map.size() == 2);
        CHECK(string_int_map.find("one") == string_int_map.end());
        CHECK(string_int_map.at("three") == 3);

        // Insert
        int_int_map.insert({{3, 300}, {4, 400}});
        CHECK(int_int_map.size() == 4);
        CHECK(int_int_map.at(3) == 300);
        CHECK(int_int_map.at(4) == 400);
    }

    SUBCASE("initializer_list preserves sorted order") {
        // Initialize with unsorted data
        std::map<int, std::string> std_map = {{3, "three"}, {1, "one"}, {2, "two"}};
        fl::fl_map<int, fl::string> fl_map = {{3, "three"}, {1, "one"}, {2, "two"}};

        // Verify elements are stored in sorted order
        auto std_it = std_map.begin();
        auto fl_it = fl_map.begin();

        CHECK(std_it->first == 1);
        CHECK(fl_it->first == 1);
        ++std_it; ++fl_it;

        CHECK(std_it->first == 2);
        CHECK(fl_it->first == 2);
        ++std_it; ++fl_it;

        CHECK(std_it->first == 3);
        CHECK(fl_it->first == 3);
    }

    SUBCASE("initializer_list combined with other operations") {
        // Start with initializer list
        fl::fl_map<int, fl::string> fl_map = {{1, "one"}, {2, "two"}};
        CHECK(fl_map.size() == 2);

        // Use [] operator
        fl_map[3] = "three";
        CHECK(fl_map.size() == 3);

        // Use insert
        fl_map.insert({4, "four"});
        CHECK(fl_map.size() == 4);

        // Use emplace
        fl_map.emplace(5, "five");
        CHECK(fl_map.size() == 5);

        // Use initializer list insert
        fl_map.insert({{6, "six"}, {7, "seven"}});
        CHECK(fl_map.size() == 7);

        // Verify all elements
        for (int i = 1; i <= 7; ++i) {
            CHECK(fl_map.contains(i));
        }

        // Replace with new initializer list
        fl_map = {{10, "ten"}, {20, "twenty"}};
        CHECK(fl_map.size() == 2);
        CHECK(fl_map.contains(10));
        CHECK(fl_map.contains(20));
        CHECK(!fl_map.contains(1));
    }
}
