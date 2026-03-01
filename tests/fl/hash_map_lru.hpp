// test.cpp
// g++ --std=c++11 test.cpp -o test && ./test


#include "fl/hash_map_lru.h"
#include "fl/stl/new.h"
#include "test.h"
#include "fl/stl/move.h"
#include "fl/stl/string.h"


FL_TEST_CASE("Test HashMapLRU") {
    FL_SUBCASE("Basic operations") {
        fl::HashMapLru<int, int> lru(3);
        
        // Test empty state
        FL_CHECK(lru.empty());
        FL_CHECK(lru.size() == 0);
        FL_CHECK(lru.capacity() == 3);
        FL_CHECK(lru.find_value(1) == nullptr);
        
        // Test insertion
        lru.insert(1, 100);
        FL_CHECK(!lru.empty());
        FL_CHECK(lru.size() == 1);
        FL_CHECK(*lru.find_value(1) == 100);
        
        // Test operator[]
        lru[2] = 200;
        FL_CHECK(lru.size() == 2);
        FL_CHECK(*lru.find_value(2) == 200);
        
        // Test update
        lru[1] = 150;
        FL_CHECK(lru.size() == 2);
        FL_CHECK(*lru.find_value(1) == 150);
        
        // Test removal
        FL_CHECK(lru.remove(1));
        FL_CHECK(lru.size() == 1);
        FL_CHECK(lru.find_value(1) == nullptr);
        FL_CHECK(!lru.remove(1)); // Already removed
        
        // Test clear
        lru.clear();
        FL_CHECK(lru.empty());
        FL_CHECK(lru.size() == 0);
    }
    
    FL_SUBCASE("LRU eviction") {
        fl::HashMapLru<int, int> lru(3);
        
        // Fill the cache
        lru.insert(1, 100);
        lru.insert(2, 200);
        lru.insert(3, 300);
        FL_CHECK(lru.size() == 3);
        
        // Access key 1 to make it most recently used
        FL_CHECK(*lru.find_value(1) == 100);
        
        // Insert a new key, should evict key 2 (least recently used)
        lru.insert(4, 400);
        FL_CHECK(lru.size() == 3);
        FL_CHECK(lru.find_value(2) == nullptr); // Key 2 should be evicted
        FL_CHECK(*lru.find_value(1) == 100);
        FL_CHECK(*lru.find_value(3) == 300);
        FL_CHECK(*lru.find_value(4) == 400);
        
        // Access key 3, then insert new key
        FL_CHECK(*lru.find_value(3) == 300);
        lru.insert(5, 500);
        FL_CHECK(lru.size() == 3);
        FL_CHECK(lru.find_value(1) == nullptr); // Key 1 should be evicted
        FL_CHECK(*lru.find_value(3) == 300);
        FL_CHECK(*lru.find_value(4) == 400);
        FL_CHECK(*lru.find_value(5) == 500);
    }
    
    FL_SUBCASE("Operator[] LRU behavior") {
        fl::HashMapLru<int, int> lru(3);
        
        // Fill the cache using operator[]
        lru[1] = 100;
        lru[2] = 200;
        lru[3] = 300;
        
        // Access key 1 to make it most recently used
        int val = lru[1];
        FL_CHECK(val == 100);
        
        // Insert a new key, should evict key 2
        lru[4] = 400;
        FL_CHECK(lru.size() == 3);
        FL_CHECK(lru.find_value(2) == nullptr);
        FL_CHECK(*lru.find_value(1) == 100);
        FL_CHECK(*lru.find_value(3) == 300);
        FL_CHECK(*lru.find_value(4) == 400);
    }
    
    FL_SUBCASE("Edge cases") {
        // Test with capacity 1
        fl::HashMapLru<int, int> tiny_lru(1);
        tiny_lru.insert(1, 100);
        FL_CHECK(*tiny_lru.find_value(1) == 100);
        
        tiny_lru.insert(2, 200);
        FL_CHECK(tiny_lru.size() == 1);
        FL_CHECK(tiny_lru.find_value(1) == nullptr);
        FL_CHECK(*tiny_lru.find_value(2) == 200);
        
        // Test with string keys
        fl::HashMapLru<fl::string, int> str_lru(2);
        str_lru.insert("one", 1);
        str_lru.insert("two", 2);
        FL_CHECK(*str_lru.find_value("one") == 1);
        FL_CHECK(*str_lru.find_value("two") == 2);
        
        // This should evict "one" since it's least recently used
        str_lru.insert("three", 3);
        FL_CHECK(str_lru.find_value("one") == nullptr);
        FL_CHECK(*str_lru.find_value("two") == 2);
        FL_CHECK(*str_lru.find_value("three") == 3);
    }
    
    FL_SUBCASE("Update existing key") {
        fl::HashMapLru<int, int> lru(3);
        
        // Fill the cache
        lru.insert(1, 100);
        lru.insert(2, 200);
        lru.insert(3, 300);
        
        // Update an existing key
        lru.insert(2, 250);
        FL_CHECK(lru.size() == 3);
        FL_CHECK(*lru.find_value(1) == 100);
        FL_CHECK(*lru.find_value(2) == 250);
        FL_CHECK(*lru.find_value(3) == 300);
        
        // Insert a new key, should evict key 1 (least recently used)
        lru.insert(4, 400);
        FL_CHECK(lru.size() == 3);
        FL_CHECK(lru.find_value(1) == nullptr);
        FL_CHECK(*lru.find_value(2) == 250);
        FL_CHECK(*lru.find_value(3) == 300);
        FL_CHECK(*lru.find_value(4) == 400);
    }
}
