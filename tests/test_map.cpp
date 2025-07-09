#include "test.h"
#include <map>
#include <string>
#include "fl/map.h"
#include "fl/string.h"
#include "fl/namespace.h"

using namespace fl;

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
