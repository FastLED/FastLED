#include "test.h"
#include <map>
#include <string>
#include <vector>
#include <random>
#include <algorithm>
#include <climits>
#include "fl/rbtree.h"
#include "fl/string.h"
#include "fl/namespace.h"

using namespace fl;

// Helper function to compare red-black tree with std::map
template<typename StdMap, typename RBTree>
bool maps_equal(const StdMap& std_map, const RBTree& rb_tree) {
    if (std_map.size() != rb_tree.size()) {
        return false;
    }
    
    auto std_it = std_map.begin();
    auto rb_it = rb_tree.begin();
    
    while (std_it != std_map.end() && rb_it != rb_tree.end()) {
        if (std_it->first != rb_it->first || std_it->second != rb_it->second) {
            return false;
        }
        ++std_it;
        ++rb_it;
    }
    
    return std_it == std_map.end() && rb_it == rb_tree.end();
}

// Helper function to validate red-black tree properties
template<typename Key, typename Value, typename Compare>
bool validate_red_black_properties(const fl::MapRedBlackTree<Key, Value, Compare>& tree) {
    // For now, we'll just check that the tree works as expected
    // A full red-black tree validation would require access to internal structure
    
    // Basic checks: 
    // 1. Size consistency
    size_t count = 0;
    for (auto it = tree.begin(); it != tree.end(); ++it) {
        count++;
    }
    
    if (count != tree.size()) {
        return false;
    }
    
    // 2. Ordering property using the tree's comparator
    if (!tree.empty()) {
        auto prev = tree.begin();
        auto current = prev;
        ++current;
        
        while (current != tree.end()) {
            // If current comes before prev in the tree's ordering, it's invalid
            if (tree.key_comp()(current->first, prev->first)) {
                return false;
            }
            prev = current;
            ++current;
        }
    }
    
    return true;
}

TEST_CASE("MapRedBlackTree - Basic Construction and Properties") {
    fl::MapRedBlackTree<int, int> rb_tree;
    std::map<int, int> std_map;
    
    SUBCASE("Default construction") {
        CHECK(rb_tree.empty() == std_map.empty());
        CHECK(rb_tree.size() == std_map.size());
        CHECK(rb_tree.size() == 0);
        CHECK(validate_red_black_properties(rb_tree));
    }
    
    SUBCASE("Copy construction") {
        rb_tree[1] = 10;
        rb_tree[2] = 20;
        
        fl::MapRedBlackTree<int, int> rb_copy(rb_tree);
        CHECK(rb_copy.size() == 2);
        CHECK(rb_copy[1] == 10);
        CHECK(rb_copy[2] == 20);
        CHECK(validate_red_black_properties(rb_copy));
    }
    
    SUBCASE("Assignment operator") {
        rb_tree[1] = 10;
        rb_tree[2] = 20;
        
        fl::MapRedBlackTree<int, int> rb_assigned;
        rb_assigned = rb_tree;
        CHECK(rb_assigned.size() == 2);
        CHECK(rb_assigned[1] == 10);
        CHECK(rb_assigned[2] == 20);
        CHECK(validate_red_black_properties(rb_assigned));
    }
}

TEST_CASE("MapRedBlackTree vs std::map - Insert Operations") {
    std::map<int, fl::string> std_map;
    fl::MapRedBlackTree<int, fl::string> rb_tree;
    
    SUBCASE("Insert with pair") {
        auto std_result = std_map.insert({1, "one"});
        auto rb_result = rb_tree.insert({1, "one"});
        
        CHECK(std_result.second == rb_result.second);
        CHECK(std_result.first->first == rb_result.first->first);
        CHECK(std_result.first->second == rb_result.first->second);
        CHECK(maps_equal(std_map, rb_tree));
        CHECK(validate_red_black_properties(rb_tree));
    }
    
    SUBCASE("Insert duplicate key") {
        std_map.insert({1, "one"});
        rb_tree.insert({1, "one"});
        
        auto std_result = std_map.insert({1, "ONE"});
        auto rb_result = rb_tree.insert({1, "ONE"});
        
        CHECK(std_result.second == rb_result.second);
        CHECK(std_result.second == false); // Should not insert duplicate
        CHECK(maps_equal(std_map, rb_tree));
        CHECK(validate_red_black_properties(rb_tree));
    }
    
    SUBCASE("Multiple inserts maintain order") {
        std::vector<std::pair<int, fl::string>> test_data = {
            {3, "three"}, {1, "one"}, {4, "four"}, {2, "two"}, {5, "five"}
        };
        
        for (const auto& item : test_data) {
            std_map.insert(item);
            rb_tree.insert(fl::pair<int, fl::string>(item.first, item.second));
        }
        
        CHECK(maps_equal(std_map, rb_tree));
        CHECK(std_map.size() == 5);
        CHECK(rb_tree.size() == 5);
        CHECK(validate_red_black_properties(rb_tree));
    }
    
    SUBCASE("Large number of sequential inserts") {
        // Test tree balancing with sequential data - use int,int for this test
        std::map<int, int> std_map_int;
        fl::MapRedBlackTree<int, int> rb_tree_int;
        
        for (int i = 1; i <= 100; ++i) {
            std_map_int[i] = i * 10;
            rb_tree_int[i] = i * 10;
        }
        
        CHECK(maps_equal(std_map_int, rb_tree_int));
        CHECK(std_map_int.size() == 100);
        CHECK(rb_tree_int.size() == 100);
        CHECK(validate_red_black_properties(rb_tree_int));
    }
    
    SUBCASE("Large number of reverse sequential inserts") {
        // Test tree balancing with reverse sequential data - use int,int for this test
        std::map<int, int> std_map_int;
        fl::MapRedBlackTree<int, int> rb_tree_int;
        
        for (int i = 100; i >= 1; --i) {
            std_map_int[i] = i * 10;
            rb_tree_int[i] = i * 10;
        }
        
        CHECK(maps_equal(std_map_int, rb_tree_int));
        CHECK(std_map_int.size() == 100);
        CHECK(rb_tree_int.size() == 100);
        CHECK(validate_red_black_properties(rb_tree_int));
    }
}

TEST_CASE("MapRedBlackTree vs std::map - Element Access") {
    std::map<int, fl::string> std_map;
    fl::MapRedBlackTree<int, fl::string> rb_tree;
    
    // Insert test data
    std_map[1] = "one";
    std_map[2] = "two";
    std_map[3] = "three";
    
    rb_tree[1] = "one";
    rb_tree[2] = "two";
    rb_tree[3] = "three";
    
    SUBCASE("operator[] access existing keys") {
        CHECK(std_map[1] == rb_tree[1]);
        CHECK(std_map[2] == rb_tree[2]);
        CHECK(std_map[3] == rb_tree[3]);
        CHECK(validate_red_black_properties(rb_tree));
    }
    
    SUBCASE("operator[] creates new key with default value") {
        CHECK(std_map[4] == rb_tree[4]); // Both should create empty string
        CHECK(std_map.size() == rb_tree.size());
        CHECK(maps_equal(std_map, rb_tree));
        CHECK(validate_red_black_properties(rb_tree));
    }
    
    SUBCASE("at() method for existing keys") {
        CHECK(std_map.at(1) == rb_tree.at(1));
        CHECK(std_map.at(2) == rb_tree.at(2));
        CHECK(std_map.at(3) == rb_tree.at(3));
        CHECK(validate_red_black_properties(rb_tree));
    }
    
    SUBCASE("at() method for non-existent keys") {
        // Note: Both should fail on non-existent keys
        CHECK(std_map.find(99) == std_map.end());
        CHECK(rb_tree.find(99) == rb_tree.end());
        CHECK(validate_red_black_properties(rb_tree));
    }
}

TEST_CASE("MapRedBlackTree vs std::map - Find Operations") {
    std::map<int, fl::string> std_map;
    fl::MapRedBlackTree<int, fl::string> rb_tree;
    
    // Insert test data
    std_map.insert({1, "one"});
    std_map.insert({2, "two"});
    std_map.insert({3, "three"});
    
    rb_tree.insert({1, "one"});
    rb_tree.insert({2, "two"});
    rb_tree.insert({3, "three"});
    
    SUBCASE("find() existing keys") {
        auto std_it = std_map.find(2);
        auto rb_it = rb_tree.find(2);
        
        CHECK((std_it != std_map.end()) == (rb_it != rb_tree.end()));
        CHECK(std_it->first == rb_it->first);
        CHECK(std_it->second == rb_it->second);
        CHECK(validate_red_black_properties(rb_tree));
    }
    
    SUBCASE("find() non-existent keys") {
        auto std_it = std_map.find(99);
        auto rb_it = rb_tree.find(99);
        
        CHECK((std_it == std_map.end()) == (rb_it == rb_tree.end()));
        CHECK(validate_red_black_properties(rb_tree));
    }
    
    SUBCASE("count() method") {
        CHECK(std_map.count(1) == rb_tree.count(1));
        CHECK(std_map.count(2) == rb_tree.count(2));
        CHECK(std_map.count(99) == rb_tree.count(99));
        CHECK(std_map.count(99) == 0);
        CHECK(rb_tree.count(99) == 0);
        CHECK(validate_red_black_properties(rb_tree));
    }
    
    SUBCASE("contains() method") {
        CHECK(rb_tree.contains(1) == true);
        CHECK(rb_tree.contains(2) == true);
        CHECK(rb_tree.contains(99) == false);
        
        // Compare with std::map using count
        CHECK((std_map.count(1) > 0) == rb_tree.contains(1));
        CHECK((std_map.count(99) > 0) == rb_tree.contains(99));
        CHECK(validate_red_black_properties(rb_tree));
    }
}

TEST_CASE("MapRedBlackTree vs std::map - Iterator Operations") {
    std::map<int, fl::string> std_map;
    fl::MapRedBlackTree<int, fl::string> rb_tree;
    
    // Insert test data in different order to test sorting
    std::vector<std::pair<int, fl::string>> test_data = {
        {3, "three"}, {1, "one"}, {4, "four"}, {2, "two"}
    };
    
    for (const auto& item : test_data) {
        std_map.insert(item);
        rb_tree.insert(fl::pair<int, fl::string>(item.first, item.second));
    }
    
    SUBCASE("Forward iteration order") {
        std::vector<int> std_order;
        std::vector<int> rb_order;
        
        for (const auto& pair : std_map) {
            std_order.push_back(pair.first);
        }
        
        for (const auto& pair : rb_tree) {
            rb_order.push_back(pair.first);
        }
        
        CHECK(std_order == rb_order);
        CHECK(std_order == std::vector<int>{1, 2, 3, 4}); // Should be sorted
        CHECK(validate_red_black_properties(rb_tree));
    }
    
    SUBCASE("begin() and end() iterators") {
        CHECK((std_map.begin() == std_map.end()) == (rb_tree.begin() == rb_tree.end()));
        if (!std_map.empty()) {
            CHECK(std_map.begin()->first == rb_tree.begin()->first);
            CHECK(std_map.begin()->second == rb_tree.begin()->second);
        }
        CHECK(validate_red_black_properties(rb_tree));
    }
    
    SUBCASE("Iterator increment") {
        auto std_it = std_map.begin();
        auto rb_it = rb_tree.begin();
        
        ++std_it;
        ++rb_it;
        
        CHECK(std_it->first == rb_it->first);
        CHECK(std_it->second == rb_it->second);
        CHECK(validate_red_black_properties(rb_tree));
    }
    
    SUBCASE("Iterator decrement") {
        auto std_it = std_map.end();
        auto rb_it = rb_tree.end();
        
        --std_it;
        --rb_it;
        
        CHECK(std_it->first == rb_it->first);
        CHECK(std_it->second == rb_it->second);
        CHECK(validate_red_black_properties(rb_tree));
    }
}

TEST_CASE("MapRedBlackTree vs std::map - Erase Operations") {
    std::map<int, fl::string> std_map;
    fl::MapRedBlackTree<int, fl::string> rb_tree;
    
    // Insert test data
    for (int i = 1; i <= 10; ++i) {
        std::string std_value = "value" + std::to_string(i);
        fl::string rb_value = "value";
        rb_value += std::to_string(i).c_str();
        std_map[i] = fl::string(std_value.c_str());
        rb_tree[i] = rb_value;
    }
    
    SUBCASE("Erase by key") {
        size_t std_erased = std_map.erase(5);
        size_t rb_erased = rb_tree.erase(5);
        
        CHECK(std_erased == rb_erased);
        CHECK(std_erased == 1);
        CHECK(maps_equal(std_map, rb_tree));
        CHECK(validate_red_black_properties(rb_tree));
    }
    
    SUBCASE("Erase non-existent key") {
        size_t std_erased = std_map.erase(99);
        size_t rb_erased = rb_tree.erase(99);
        
        CHECK(std_erased == rb_erased);
        CHECK(std_erased == 0);
        CHECK(maps_equal(std_map, rb_tree));
        CHECK(validate_red_black_properties(rb_tree));
    }
    
    SUBCASE("Erase by iterator") {
        auto std_it = std_map.find(3);
        auto rb_it = rb_tree.find(3);
        
        std_map.erase(std_it);
        rb_tree.erase(rb_it);
        
        CHECK(maps_equal(std_map, rb_tree));
        CHECK(std_map.find(3) == std_map.end());
        CHECK(rb_tree.find(3) == rb_tree.end());
        CHECK(validate_red_black_properties(rb_tree));
    }
    
    SUBCASE("Erase multiple elements") {
        // Erase elements 2, 4, 6, 8
        for (int i = 2; i <= 8; i += 2) {
            std_map.erase(i);
            rb_tree.erase(i);
        }
        
        CHECK(maps_equal(std_map, rb_tree));
        CHECK(std_map.size() == 6); // Should have 1, 3, 5, 7, 9, 10
        CHECK(rb_tree.size() == 6);
        CHECK(validate_red_black_properties(rb_tree));
    }
    
    SUBCASE("Erase all elements") {
        for (int i = 1; i <= 10; ++i) {
            std_map.erase(i);
            rb_tree.erase(i);
        }
        
        CHECK(maps_equal(std_map, rb_tree));
        CHECK(std_map.empty());
        CHECK(rb_tree.empty());
        CHECK(validate_red_black_properties(rb_tree));
    }
}

TEST_CASE("MapRedBlackTree vs std::map - Clear and Empty") {
    std::map<int, fl::string> std_map;
    fl::MapRedBlackTree<int, fl::string> rb_tree;
    
    // Insert some data
    std_map[1] = "one";
    std_map[2] = "two";
    rb_tree[1] = "one";
    rb_tree[2] = "two";
    
    CHECK(std_map.empty() == rb_tree.empty());
    CHECK(std_map.empty() == false);
    CHECK(validate_red_black_properties(rb_tree));
    
    SUBCASE("Clear operation") {
        std_map.clear();
        rb_tree.clear();
        
        CHECK(std_map.empty() == rb_tree.empty());
        CHECK(std_map.size() == rb_tree.size());
        CHECK(std_map.size() == 0);
        CHECK(maps_equal(std_map, rb_tree));
        CHECK(validate_red_black_properties(rb_tree));
    }
}

TEST_CASE("MapRedBlackTree vs std::map - Bound Operations") {
    std::map<int, fl::string> std_map;
    fl::MapRedBlackTree<int, fl::string> rb_tree;
    
    // Insert test data: 1, 3, 5, 7, 9
    for (int i = 1; i <= 9; i += 2) {
        std::string std_value = "value" + std::to_string(i);
        fl::string rb_value = "value";
        rb_value += std::to_string(i).c_str();
        std_map[i] = fl::string(std_value.c_str());
        rb_tree[i] = rb_value;
    }
    
    SUBCASE("lower_bound existing key") {
        auto std_it = std_map.lower_bound(5);
        auto rb_it = rb_tree.lower_bound(5);
        
        CHECK(std_it->first == rb_it->first);
        CHECK(std_it->second == rb_it->second);
        CHECK(std_it->first == 5);
        CHECK(validate_red_black_properties(rb_tree));
    }
    
    SUBCASE("lower_bound non-existing key") {
        auto std_it = std_map.lower_bound(4);
        auto rb_it = rb_tree.lower_bound(4);
        
        CHECK(std_it->first == rb_it->first);
        CHECK(std_it->first == 5); // Should find next higher key
        CHECK(validate_red_black_properties(rb_tree));
    }
    
    SUBCASE("upper_bound existing key") {
        auto std_it = std_map.upper_bound(5);
        auto rb_it = rb_tree.upper_bound(5);
        
        CHECK(std_it->first == rb_it->first);
        CHECK(std_it->first == 7); // Should find next higher key
        CHECK(validate_red_black_properties(rb_tree));
    }
    
    SUBCASE("equal_range") {
        auto std_range = std_map.equal_range(5);
        auto rb_range = rb_tree.equal_range(5);
        
        CHECK(std_range.first->first == rb_range.first->first);
        CHECK(std_range.second->first == rb_range.second->first);
        CHECK(std_range.first->first == 5);
        CHECK(std_range.second->first == 7);
        CHECK(validate_red_black_properties(rb_tree));
    }
    
    SUBCASE("lower_bound with key larger than all") {
        auto std_it = std_map.lower_bound(20);
        auto rb_it = rb_tree.lower_bound(20);
        
        CHECK((std_it == std_map.end()) == (rb_it == rb_tree.end()));
        CHECK(validate_red_black_properties(rb_tree));
    }
    
    SUBCASE("upper_bound with key larger than all") {
        auto std_it = std_map.upper_bound(20);
        auto rb_it = rb_tree.upper_bound(20);
        
        CHECK((std_it == std_map.end()) == (rb_it == rb_tree.end()));
        CHECK(validate_red_black_properties(rb_tree));
    }
}

TEST_CASE("MapRedBlackTree - Comparison Operations") {
    fl::MapRedBlackTree<int, fl::string> rb_tree1;
    fl::MapRedBlackTree<int, fl::string> rb_tree2;
    
    SUBCASE("Empty trees equality") {
        CHECK(rb_tree1 == rb_tree2);
        CHECK(!(rb_tree1 != rb_tree2));
        CHECK(validate_red_black_properties(rb_tree1));
        CHECK(validate_red_black_properties(rb_tree2));
    }
    
    SUBCASE("Equal trees") {
        rb_tree1[1] = "one";
        rb_tree1[2] = "two";
        rb_tree2[1] = "one";
        rb_tree2[2] = "two";
        
        CHECK(rb_tree1 == rb_tree2);
        CHECK(!(rb_tree1 != rb_tree2));
        CHECK(validate_red_black_properties(rb_tree1));
        CHECK(validate_red_black_properties(rb_tree2));
    }
    
    SUBCASE("Different trees") {
        rb_tree1[1] = "one";
        rb_tree2[1] = "ONE"; // Different value
        
        CHECK(!(rb_tree1 == rb_tree2));
        CHECK(rb_tree1 != rb_tree2);
        CHECK(validate_red_black_properties(rb_tree1));
        CHECK(validate_red_black_properties(rb_tree2));
    }
    
    SUBCASE("Different sizes") {
        rb_tree1[1] = "one";
        rb_tree1[2] = "two";
        rb_tree2[1] = "one";
        
        CHECK(!(rb_tree1 == rb_tree2));
        CHECK(rb_tree1 != rb_tree2);
        CHECK(validate_red_black_properties(rb_tree1));
        CHECK(validate_red_black_properties(rb_tree2));
    }
}

TEST_CASE("MapRedBlackTree - Custom Comparator") {
    struct DescendingInt {
        bool operator()(int a, int b) const {
            return a > b; // Reverse order
        }
    };
    
    std::map<int, fl::string, DescendingInt> std_map;
    fl::MapRedBlackTree<int, fl::string, DescendingInt> rb_tree;
    
    std_map[1] = "one";
    std_map[2] = "two";
    std_map[3] = "three";
    
    rb_tree[1] = "one";
    rb_tree[2] = "two";
    rb_tree[3] = "three";
    
    SUBCASE("Custom ordering") {
        std::vector<int> std_order;
        std::vector<int> rb_order;
        
        for (const auto& pair : std_map) {
            std_order.push_back(pair.first);
        }
        
        for (const auto& pair : rb_tree) {
            rb_order.push_back(pair.first);
        }
        
        CHECK(std_order == rb_order);
        CHECK(std_order == std::vector<int>{3, 2, 1}); // Descending order
        CHECK(validate_red_black_properties(rb_tree));
    }
}

TEST_CASE("MapRedBlackTree - Stress Tests") {
    fl::MapRedBlackTree<int, int> rb_tree;
    std::map<int, int> std_map;
    
    SUBCASE("Random operations") {
        std::vector<int> keys;
        for (int i = 1; i <= 50; ++i) {
            keys.push_back(i);
        }
        
        // Shuffle keys for random insertion order
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(keys.begin(), keys.end(), g);
        
        // Insert in random order
        for (int key : keys) {
            std_map[key] = key * 10;
            rb_tree[key] = key * 10;
        }
        
        CHECK(maps_equal(std_map, rb_tree));
        CHECK(validate_red_black_properties(rb_tree));
        
        // Random deletions
        std::shuffle(keys.begin(), keys.end(), g);
        for (size_t i = 0; i < keys.size() / 2; ++i) {
            int key = keys[i];
            std_map.erase(key);
            rb_tree.erase(key);
        }
        
        CHECK(maps_equal(std_map, rb_tree));
        CHECK(validate_red_black_properties(rb_tree));
        
        // Random lookups
        for (int key : keys) {
            CHECK((std_map.find(key) != std_map.end()) == (rb_tree.find(key) != rb_tree.end()));
        }
        
        CHECK(validate_red_black_properties(rb_tree));
    }
    
    SUBCASE("Mixed operations sequence") {
        // Perform a complex sequence of operations
        rb_tree[5] = 50;
        std_map[5] = 50;
        CHECK(maps_equal(std_map, rb_tree));
        CHECK(validate_red_black_properties(rb_tree));
        
        rb_tree.erase(5);
        std_map.erase(5);
        CHECK(maps_equal(std_map, rb_tree));
        CHECK(validate_red_black_properties(rb_tree));
        
        for (int i = 1; i <= 20; ++i) {
            rb_tree[i] = i * 10;
            std_map[i] = i * 10;
        }
        CHECK(maps_equal(std_map, rb_tree));
        CHECK(validate_red_black_properties(rb_tree));
        
        // Erase every other element
        for (int i = 2; i <= 20; i += 2) {
            rb_tree.erase(i);
            std_map.erase(i);
        }
        CHECK(maps_equal(std_map, rb_tree));
        CHECK(validate_red_black_properties(rb_tree));
        
        rb_tree.clear();
        std_map.clear();
        CHECK(maps_equal(std_map, rb_tree));
        CHECK(rb_tree.empty());
        CHECK(std_map.empty());
        CHECK(validate_red_black_properties(rb_tree));
    }
}

TEST_CASE("MapRedBlackTree - Performance Characteristics") {
    fl::MapRedBlackTree<int, int> rb_tree;
    
    SUBCASE("Large dataset operations") {
        const int N = 1000;
        
        // Sequential insertions should still be efficient due to balancing
        for (int i = 1; i <= N; ++i) {
            rb_tree[i] = i * 2;
        }
        
        CHECK(rb_tree.size() == N);
        CHECK(validate_red_black_properties(rb_tree));
        
        // All elements should be findable
        for (int i = 1; i <= N; ++i) {
            CHECK(rb_tree.find(i) != rb_tree.end());
            CHECK(rb_tree[i] == i * 2);
        }
        
        // Reverse order deletions
        for (int i = N; i >= 1; --i) {
            CHECK(rb_tree.erase(i) == 1);
        }
        
        CHECK(rb_tree.empty());
        CHECK(validate_red_black_properties(rb_tree));
    }
}

TEST_CASE("MapRedBlackTree - Edge Cases") {
    fl::MapRedBlackTree<int, int> rb_tree;
    
    SUBCASE("Single element operations") {
        rb_tree[42] = 84;
        CHECK(rb_tree.size() == 1);
        CHECK(!rb_tree.empty());
        CHECK(rb_tree[42] == 84);
        CHECK(validate_red_black_properties(rb_tree));
        
        CHECK(rb_tree.erase(42) == 1);
        CHECK(rb_tree.empty());
        CHECK(rb_tree.size() == 0);
        CHECK(validate_red_black_properties(rb_tree));
    }
    
    SUBCASE("Boundary value operations") {
        // Test with extreme values
        rb_tree[INT_MAX] = 1;
        rb_tree[INT_MIN] = 2;
        rb_tree[0] = 3;
        
        CHECK(rb_tree.size() == 3);
        CHECK(rb_tree[INT_MAX] == 1);
        CHECK(rb_tree[INT_MIN] == 2);
        CHECK(rb_tree[0] == 3);
        CHECK(validate_red_black_properties(rb_tree));
        
        // Check ordering
        auto it = rb_tree.begin();
        CHECK(it->first == INT_MIN);
        ++it;
        CHECK(it->first == 0);
        ++it;
        CHECK(it->first == INT_MAX);
        CHECK(validate_red_black_properties(rb_tree));
    }
}

#if 0  // Flip this to true to run the stress tests
// -----------------------------------------------------------------------------
// Additional Comprehensive Stress Tests
// -----------------------------------------------------------------------------

// Define FASTLED_ENABLE_RBTREE_STRESS to enable very heavy tests locally
// Example: add -DFASTLED_ENABLE_RBTREE_STRESS to unit test compile flags

TEST_CASE("RBTree Stress Test - Large Scale Operations [rbtree][stress]") {
    fl::MapRedBlackTree<int, int> rb_tree;
    std::map<int, int> std_map;

    const int N = 5000;  // Reasonable default; increase under FASTLED_ENABLE_RBTREE_STRESS

    // Sequential inserts
    for (int i = 1; i <= N; ++i) {
        rb_tree[i] = i * 3;
        std_map[i] = i * 3;
    }
    CHECK(rb_tree.size() == std_map.size());
    CHECK(maps_equal(std_map, rb_tree));
    CHECK(validate_red_black_properties(rb_tree));

    // Reverse deletes
    for (int i = N; i >= 1; --i) {
        CHECK_EQ(rb_tree.erase(i), std_map.erase(i));
        if ((i % 257) == 0) {
            CHECK(maps_equal(std_map, rb_tree));
            CHECK(validate_red_black_properties(rb_tree));
        }
    }

    CHECK(rb_tree.empty());
    CHECK(std_map.empty());
}

TEST_CASE("RBTree Stress Test - Randomized Operations [rbtree][stress]") {
    fl::MapRedBlackTree<int, int> rb_tree;
    std::map<int, int> std_map;

    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<int> key_dist(1, 2000);
    std::uniform_int_distribution<int> val_dist(-100000, 100000);
    std::uniform_int_distribution<int> op_dist(0, 99);

    const int OPS = 8000;

    for (int i = 0; i < OPS; ++i) {
        int key = key_dist(rng);
        int op = op_dist(rng);
        if (op < 45) {
            // insert/update
            int value = val_dist(rng);
            rb_tree[key] = value;
            std_map[key] = value;
        } else if (op < 70) {
            // erase
            CHECK_EQ(rb_tree.erase(key), std_map.erase(key));
        } else if (op < 90) {
            // find/contains
            bool a = (rb_tree.find(key) != rb_tree.end());
            bool b = (std_map.find(key) != std_map.end());
            CHECK_EQ(a, b);
            CHECK_EQ(rb_tree.contains(key), b);
        } else {
            // lower/upper bound coherence checks when present
            auto it_a = rb_tree.lower_bound(key);
            auto it_b = std_map.lower_bound(key);
            bool end_a = (it_a == rb_tree.end());
            bool end_b = (it_b == std_map.end());
            CHECK_EQ(end_a, end_b);
            if (!end_a && !end_b) {
                CHECK_EQ(it_a->first, it_b->first);
                CHECK_EQ(it_a->second, it_b->second);
            }
        }

        if ((i % 401) == 0) {
            CHECK(maps_equal(std_map, rb_tree));
            CHECK(validate_red_black_properties(rb_tree));
        }
    }

    CHECK(maps_equal(std_map, rb_tree));
    CHECK(validate_red_black_properties(rb_tree));
}

TEST_CASE("RBTree Stress Test - Edge Cases and Crash Scenarios [rbtree][stress]") {
    fl::MapRedBlackTree<int, int> rb_tree;
    std::map<int, int> std_map;

    // Insert duplicates and ensure semantics match std::map (no duplicate keys)
    for (int i = 0; i < 100; ++i) {
        rb_tree[10] = i;
        std_map[10] = i;
    }
    CHECK_EQ(rb_tree.size(), std_map.size());
    CHECK_EQ(rb_tree.size(), 1);
    CHECK(maps_equal(std_map, rb_tree));
    CHECK(validate_red_black_properties(rb_tree));

    // Zig-zag insertion pattern (pathological for naive trees)
    for (int i = 1; i <= 200; ++i) {
        int k = (i % 2 == 0) ? (100 + i) : (100 - i);
        rb_tree[k] = k * 2;
        std_map[k] = k * 2;
    }
    CHECK(maps_equal(std_map, rb_tree));
    CHECK(validate_red_black_properties(rb_tree));

    // Erase root repeatedly: always erase current begin() (smallest key)
    for (int i = 0; i < 50 && !rb_tree.empty(); ++i) {
        auto it_a = rb_tree.begin();
        auto it_b = std_map.begin();
        CHECK(it_a != rb_tree.end());
        CHECK(it_b != std_map.end());
        CHECK_EQ(it_a->first, it_b->first);
        CHECK_EQ(rb_tree.erase(it_a->first), std_map.erase(it_b->first));
        CHECK(validate_red_black_properties(rb_tree));
    }

    CHECK(maps_equal(std_map, rb_tree));
}

TEST_CASE("RBTree Stress Test - Pathological Patterns [rbtree][stress]") {
    fl::MapRedBlackTree<int, int> rb_tree;
    std::map<int, int> std_map;

    // Alternate insert/erase around a moving window
    for (int round = 0; round < 2000; ++round) {
        int base = (round % 100);
        // insert a small cluster
        for (int d = -3; d <= 3; ++d) {
            int k = base + d;
            rb_tree[k] = k * 7;
            std_map[k] = k * 7;
        }
        // erase overlapping cluster
        for (int d = -2; d <= 2; ++d) {
            int k = base + d;
            CHECK_EQ(rb_tree.erase(k), std_map.erase(k));
        }
        if ((round % 127) == 0) {
            CHECK(maps_equal(std_map, rb_tree));
            CHECK(validate_red_black_properties(rb_tree));
        }
    }

    CHECK(maps_equal(std_map, rb_tree));
    CHECK(validate_red_black_properties(rb_tree));
}

TEST_CASE("RBTree Stress Test - Comparison with std::map Comprehensive [rbtree][stress]") {
    // Multiple seeds, random operation sequences, end-to-end equivalence
    for (int seed = 1; seed <= 7; ++seed) {
        fl::MapRedBlackTree<int, int> rb_tree;
        std::map<int, int> std_map;

        std::mt19937 rng(static_cast<unsigned int>(seed * 1315423911u));
        std::uniform_int_distribution<int> key_dist(1, 3000);
        std::uniform_int_distribution<int> val_dist(-1000000, 1000000);
        std::uniform_int_distribution<int> op_dist(0, 99);

        const int OPS = 4000;
        for (int i = 0; i < OPS; ++i) {
            int key = key_dist(rng);
            int op = op_dist(rng);
            if (op < 50) {
                int value = val_dist(rng);
                rb_tree[key] = value;
                std_map[key] = value;
            } else if (op < 80) {
                CHECK_EQ(rb_tree.erase(key), std_map.erase(key));
            } else if (op < 90) {
                auto it_a = rb_tree.find(key);
                auto it_b = std_map.find(key);
                bool pres_a = (it_a != rb_tree.end());
                bool pres_b = (it_b != std_map.end());
                CHECK_EQ(pres_a, pres_b);
                if (pres_a && pres_b) {
                    CHECK_EQ(it_a->second, it_b->second);
                }
            } else {
                // bounds
                auto la = rb_tree.lower_bound(key);
                auto lb = std_map.lower_bound(key);
                CHECK_EQ(la == rb_tree.end(), lb == std_map.end());
                if (la != rb_tree.end() && lb != std_map.end()) {
                    CHECK_EQ(la->first, lb->first);
                    CHECK_EQ(la->second, lb->second);
                }
                auto ua = rb_tree.upper_bound(key);
                auto ub = std_map.upper_bound(key);
                CHECK_EQ(ua == rb_tree.end(), ub == std_map.end());
                if (ua != rb_tree.end() && ub != std_map.end()) {
                    CHECK_EQ(ua->first, ub->first);
                    CHECK_EQ(ua->second, ub->second);
                }
            }

            if ((i % 503) == 0) {
                CHECK(maps_equal(std_map, rb_tree));
                CHECK(validate_red_black_properties(rb_tree));
            }
        }

        CHECK(maps_equal(std_map, rb_tree));
        CHECK(validate_red_black_properties(rb_tree));
    }
}


TEST_CASE("RBTree Stress Test - Heavy Memory and Performance [rbtree][stress][heavy]") {
    fl::MapRedBlackTree<int, int> rb_tree;
    std::map<int, int> std_map;

    const int N = 30000;  // Heavy test: enable only when explicitly requested
    for (int i = 0; i < N; ++i) {
        rb_tree[i] = i ^ (i << 1);
        std_map[i] = i ^ (i << 1);
        if ((i % 2047) == 0) {
            CHECK(maps_equal(std_map, rb_tree));
            CHECK(validate_red_black_properties(rb_tree));
        }
    }

    // Mixed deletes
    for (int i = 0; i < N; i += 2) {
        CHECK_EQ(rb_tree.erase(i), std_map.erase(i));
        if ((i % 4093) == 0) {
            CHECK(maps_equal(std_map, rb_tree));
            CHECK(validate_red_black_properties(rb_tree));
        }
    }

    CHECK(maps_equal(std_map, rb_tree));
    CHECK(validate_red_black_properties(rb_tree));
}
#endif // FASTLED_ENABLE_RBTREE_STRESS
