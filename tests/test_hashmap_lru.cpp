// test.cpp
// g++ --std=c++11 test.cpp -o test && ./test

#include <vector>
#include <set>
#include <unordered_map>

#include "fl/hash_map_lru.h"
#include "fl/str.h"
#include "test.h"

using namespace fl;

TEST_CASE("Test HashMapLRU") {
    SUBCASE("Basic operations") {
        HashMapLru<int, int> lru(3);
        
        // Test empty state
        CHECK(lru.empty());
        CHECK(lru.size() == 0);
        CHECK(lru.capacity() == 3);
        CHECK(lru.find_value(1) == nullptr);
        
        // Test insertion
        lru.insert(1, 100);
        CHECK(!lru.empty());
        CHECK(lru.size() == 1);
        CHECK(*lru.find_value(1) == 100);
        
        // Test operator[]
        lru[2] = 200;
        CHECK(lru.size() == 2);
        CHECK(*lru.find_value(2) == 200);
        
        // Test update
        lru[1] = 150;
        CHECK(lru.size() == 2);
        CHECK(*lru.find_value(1) == 150);
        
        // Test removal
        CHECK(lru.remove(1));
        CHECK(lru.size() == 1);
        CHECK(lru.find_value(1) == nullptr);
        CHECK(!lru.remove(1)); // Already removed
        
        // Test clear
        lru.clear();
        CHECK(lru.empty());
        CHECK(lru.size() == 0);
    }
    
    SUBCASE("LRU eviction") {
        HashMapLru<int, int> lru(3);
        
        // Fill the cache
        lru.insert(1, 100);
        lru.insert(2, 200);
        lru.insert(3, 300);
        CHECK(lru.size() == 3);
        
        // Access key 1 to make it most recently used
        CHECK(*lru.find_value(1) == 100);
        
        // Insert a new key, should evict key 2 (least recently used)
        lru.insert(4, 400);
        CHECK(lru.size() == 3);
        CHECK(lru.find_value(2) == nullptr); // Key 2 should be evicted
        CHECK(*lru.find_value(1) == 100);
        CHECK(*lru.find_value(3) == 300);
        CHECK(*lru.find_value(4) == 400);
        
        // Access key 3, then insert new key
        CHECK(*lru.find_value(3) == 300);
        lru.insert(5, 500);
        CHECK(lru.size() == 3);
        CHECK(lru.find_value(1) == nullptr); // Key 1 should be evicted
        CHECK(*lru.find_value(3) == 300);
        CHECK(*lru.find_value(4) == 400);
        CHECK(*lru.find_value(5) == 500);
    }
    
    SUBCASE("Operator[] LRU behavior") {
        HashMapLru<int, int> lru(3);
        
        // Fill the cache using operator[]
        lru[1] = 100;
        lru[2] = 200;
        lru[3] = 300;
        
        // Access key 1 to make it most recently used
        int val = lru[1];
        CHECK(val == 100);
        
        // Insert a new key, should evict key 2
        lru[4] = 400;
        CHECK(lru.size() == 3);
        CHECK(lru.find_value(2) == nullptr);
        CHECK(*lru.find_value(1) == 100);
        CHECK(*lru.find_value(3) == 300);
        CHECK(*lru.find_value(4) == 400);
    }
    
    SUBCASE("Edge cases") {
        // Test with capacity 1
        HashMapLru<int, int> tiny_lru(1);
        tiny_lru.insert(1, 100);
        CHECK(*tiny_lru.find_value(1) == 100);
        
        tiny_lru.insert(2, 200);
        CHECK(tiny_lru.size() == 1);
        CHECK(tiny_lru.find_value(1) == nullptr);
        CHECK(*tiny_lru.find_value(2) == 200);
        
        // Test with string keys
        HashMapLru<fl::string, int> str_lru(2);
        str_lru.insert("one", 1);
        str_lru.insert("two", 2);
        CHECK(*str_lru.find_value("one") == 1);
        CHECK(*str_lru.find_value("two") == 2);
        
        // This should evict "one" since it's least recently used
        str_lru.insert("three", 3);
        CHECK(str_lru.find_value("one") == nullptr);
        CHECK(*str_lru.find_value("two") == 2);
        CHECK(*str_lru.find_value("three") == 3);
    }
    
    SUBCASE("Update existing key") {
        HashMapLru<int, int> lru(3);
        
        // Fill the cache
        lru.insert(1, 100);
        lru.insert(2, 200);
        lru.insert(3, 300);
        
        // Update an existing key
        lru.insert(2, 250);
        CHECK(lru.size() == 3);
        CHECK(*lru.find_value(1) == 100);
        CHECK(*lru.find_value(2) == 250);
        CHECK(*lru.find_value(3) == 300);
        
        // Insert a new key, should evict key 1 (least recently used)
        lru.insert(4, 400);
        CHECK(lru.size() == 3);
        CHECK(lru.find_value(1) == nullptr);
        CHECK(*lru.find_value(2) == 250);
        CHECK(*lru.find_value(3) == 300);
        CHECK(*lru.find_value(4) == 400);
    }
}
