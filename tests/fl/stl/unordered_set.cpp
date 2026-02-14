#include "fl/stl/unordered_set.h"

#include "fl/stl/set.h"
#include "fl/stl/unordered_map.h"
#include "test.h"
#include "fl/stl/string.h"


FL_TEST_CASE("Empty set properties") {
    fl::unordered_set<int> s;
    FL_REQUIRE_EQ(s.size(), 0u);
    FL_REQUIRE(s.empty());
    FL_REQUIRE(s.find(42) == s.end());
    // begin() == end() on empty set
    FL_REQUIRE(s.begin() == s.end());
}

FL_TEST_CASE("Single insert and lookup") {
    fl::unordered_set<int> s;
    s.insert(10);
    FL_REQUIRE_EQ(s.size(), 1u);
    FL_REQUIRE(!s.empty());
    
    auto it = s.find(10);
    FL_REQUIRE(it != s.end());
    FL_REQUIRE_EQ((*it), 10);  // unordered_set stores key as first in pair
    
    // Test non-existent element
    FL_REQUIRE(s.find(20) == s.end());
}

FL_TEST_CASE("Insert duplicate key does not increase size") {
    fl::unordered_set<int> s;
    s.insert(5);
    FL_REQUIRE_EQ(s.size(), 1u);
    
    // Insert same key again
    s.insert(5);
    FL_REQUIRE_EQ(s.size(), 1u);  // Size should remain 1
    FL_REQUIRE(s.find(5) != s.end());
}

FL_TEST_CASE("Multiple distinct inserts and lookups") {
    fl::unordered_set<char> s;
    
    // Insert multiple elements
    for (char c = 'a'; c <= 'j'; ++c) {
        s.insert(c);
    }
    
    FL_REQUIRE_EQ(s.size(), 10u);
    
    // Verify all elements are present
    for (char c = 'a'; c <= 'j'; ++c) {
        FL_REQUIRE(s.find(c) != s.end());
    }
    
    // Verify non-existent element
    FL_REQUIRE(s.find('z') == s.end());
}

FL_TEST_CASE("Erase behavior") {
    fl::unordered_set<int> s;
    s.insert(5);
    s.insert(10);
    s.insert(15);
    FL_REQUIRE_EQ(s.size(), 3u);
    
    // Erase existing element
    s.erase(10);
    FL_REQUIRE_EQ(s.size(), 2u);
    FL_REQUIRE(s.find(10) == s.end());
    FL_REQUIRE(s.find(5) != s.end());
    FL_REQUIRE(s.find(15) != s.end());
    
    // Erase non-existent element (should not crash)
    s.erase(99);
    FL_REQUIRE_EQ(s.size(), 2u);
    
    // Erase remaining elements
    s.erase(5);
    s.erase(15);
    FL_REQUIRE_EQ(s.size(), 0u);
    FL_REQUIRE(s.empty());
}

FL_TEST_CASE("Re-insert after erase") {
    fl::unordered_set<int> s;  // Small initial capacity
    s.insert(1);
    s.erase(1);
    FL_REQUIRE(s.find(1) == s.end());
    FL_REQUIRE_EQ(s.size(), 0u);
    
    // Re-insert same element
    s.insert(1);
    FL_REQUIRE(s.find(1) != s.end());
    FL_REQUIRE_EQ(s.size(), 1u);
}

FL_TEST_CASE("Clear resets set") {
    fl::unordered_set<int> s;
    for (int i = 0; i < 5; ++i) {
        s.insert(i);
    }
    FL_REQUIRE_EQ(s.size(), 5u);
    
    s.clear();
    FL_REQUIRE_EQ(s.size(), 0u);
    FL_REQUIRE(s.empty());
    
    // Verify all elements are gone
    for (int i = 0; i < 5; ++i) {
        FL_REQUIRE(s.find(i) == s.end());
    }
    
    // Insert after clear should work
    s.insert(100);
    FL_REQUIRE_EQ(s.size(), 1u);
    FL_REQUIRE(s.find(100) != s.end());
}

FL_TEST_CASE("Stress test with many elements and rehashing") {
    fl::unordered_set<int> s;  // Start with minimal capacity to force rehashing
    const int N = 100;
    
    // Insert many elements
    for (int i = 0; i < N; ++i) {
        s.insert(i);
        FL_REQUIRE_EQ(s.size(), static_cast<fl::size>(i + 1));
    }
    
    FL_REQUIRE_EQ(s.size(), static_cast<fl::size>(N));
    
    // Verify all elements are present
    for (int i = 0; i < N; ++i) {
        FL_REQUIRE(s.find(i) != s.end());
    }
}

FL_TEST_CASE("Iterator functionality") {
    fl::unordered_set<int> s;
    fl::size expected_size = 0;
    
    // Insert some elements
    for (int i = 0; i < 10; ++i) {
        s.insert(i * 2);  // Insert even numbers 0, 2, 4, ..., 18
        ++expected_size;
    }
    
    FL_REQUIRE_EQ(s.size(), expected_size);
    
    // Iterate and collect all keys
    fl::set<int> found_keys;
    fl::size count = 0;
    
    for (auto it = s.begin(); it != s.end(); ++it) {
        found_keys.insert((*it));
        ++count;
    }
    
    FL_REQUIRE_EQ(count, s.size());
    FL_REQUIRE_EQ(found_keys.size(), s.size());
    
    // Verify we found all expected keys
    for (int i = 0; i < 10; ++i) {
        FL_REQUIRE(found_keys.find(i * 2) != found_keys.end());
    }
}

FL_TEST_CASE("Const iterator functionality") {
    fl::unordered_set<int> s;
    for (int i = 1; i <= 5; ++i) {
        s.insert(i);
    }
    
    fl::size count = 0;
    fl::set<int> found_keys;
    
    // Use const_iterator directly from non-const object
    for (auto it = s.cbegin(); it != s.cend(); ++it) {
        found_keys.insert((*it));
        ++count;
    }
    
    FL_REQUIRE_EQ(count, s.size());
    FL_REQUIRE_EQ(found_keys.size(), 5u);
}

FL_TEST_CASE("Range-based for loop") {
    fl::unordered_set<int> s;
    for (int i = 10; i < 15; ++i) {
        s.insert(i);
    }
    
    fl::set<int> found_keys;
    fl::size count = 0;
    
    // Range-based for loop
    for (const auto& kv : s) {
        found_keys.insert(kv);
        ++count;
    }
    
    FL_REQUIRE_EQ(count, s.size());
    FL_REQUIRE_EQ(found_keys.size(), 5u);
    
    for (int i = 10; i < 15; ++i) {
        FL_REQUIRE(found_keys.find(i) != found_keys.end());
    }
}

FL_TEST_CASE("String elements") {
    fl::unordered_set<fl::string> s;
    
    s.insert("hello");
    s.insert("world");
    s.insert("test");
    
    FL_REQUIRE_EQ(s.size(), 3u);
    FL_REQUIRE(s.find("hello") != s.end());
    FL_REQUIRE(s.find("world") != s.end());
    FL_REQUIRE(s.find("test") != s.end());
    FL_REQUIRE(s.find("missing") == s.end());
    
    // Erase string element
    s.erase("world");
    FL_REQUIRE_EQ(s.size(), 2u);
    FL_REQUIRE(s.find("world") == s.end());
    FL_REQUIRE(s.find("hello") != s.end());
    FL_REQUIRE(s.find("test") != s.end());
}

FL_TEST_CASE("Capacity management") {
    fl::unordered_set<int> s;

    // Initial state
    FL_REQUIRE_EQ(s.size(), 0u);
    fl::size initial_capacity = s.capacity();
    FL_REQUIRE_GT(initial_capacity, 0u);  // Should have some initial capacity

    // Fill beyond initial capacity to test growth
    for (int i = 0; i < 20; ++i) {
        s.insert(i);
    }

    FL_REQUIRE_EQ(s.size(), 20u);
    // Capacity should have grown
    FL_REQUIRE_GE(s.capacity(), 20u);
}

#if 0  // Disabled: new unordered_set does not support custom hash/equal
FL_TEST_CASE("Custom hash and equality") {
    // Test with custom hash and equality functions for case-insensitive strings
    struct CaseInsensitiveHash {
        fl::size operator()(const fl::string& str) const {
            fl::size hash = 0;
            for (fl::size i = 0; i < str.size(); ++i) {
                char c = str[i];
                char lower_c = (c >= 'A' && c <= 'Z') ? c + 32 : c;
                hash = hash * 31 + static_cast<fl::size>(lower_c);
            }
            return hash;
        }
    };

    struct CaseInsensitiveEqual {
        bool operator()(const fl::string& a, const fl::string& b) const {
            if (a.size() != b.size()) return false;
            for (fl::size i = 0; i < a.size(); ++i) {
                char ca = (a[i] >= 'A' && a[i] <= 'Z') ? a[i] + 32 : a[i];
                char cb = (b[i] >= 'A' && b[i] <= 'Z') ? b[i] + 32 : b[i];
                if (ca != cb) return false;
            }
            return true;
        }
    };

    fl::unordered_set<fl::string, CaseInsensitiveHash, CaseInsensitiveEqual> s;

    s.insert("Hello");
    s.insert("WORLD");
    s.insert("test");

    FL_REQUIRE_EQ(s.size(), 3u);

    // These should be found due to case-insensitive comparison
    FL_REQUIRE(s.find("hello") != s.end());
    FL_REQUIRE(s.find("HELLO") != s.end());
    FL_REQUIRE(s.find("world") != s.end());
    FL_REQUIRE(s.find("World") != s.end());
    FL_REQUIRE(s.find("TEST") != s.end());

    // Insert duplicate in different case should not increase size
    s.insert("hello");
    s.insert("HELLO");
    FL_REQUIRE_EQ(s.size(), 3u);
}
#endif


FL_TEST_CASE("Edge cases") {
    fl::unordered_set<int> s;
    
    // Test with negative numbers
    s.insert(-1);
    s.insert(-100);
    s.insert(0);
    s.insert(100);
    
    FL_REQUIRE_EQ(s.size(), 4u);
    FL_REQUIRE(s.find(-1) != s.end());
    FL_REQUIRE(s.find(-100) != s.end());
    FL_REQUIRE(s.find(0) != s.end());
    FL_REQUIRE(s.find(100) != s.end());
    
    // Test erasing from single-element set
    fl::unordered_set<int> single;
    single.insert(42);
    FL_REQUIRE_EQ(single.size(), 1u);
    single.erase(42);
    FL_REQUIRE_EQ(single.size(), 0u);
    FL_REQUIRE(single.empty());
    
    // Test multiple operations on same element
    fl::unordered_set<int> multi;
    multi.insert(1);
    multi.insert(1);  // duplicate
    multi.erase(1);
    FL_REQUIRE_EQ(multi.size(), 0u);
    multi.insert(1);   // re-insert
    FL_REQUIRE_EQ(multi.size(), 1u);
    FL_REQUIRE(multi.find(1) != multi.end());
}

FL_TEST_CASE("Large scale operations with deletion patterns") {
    fl::unordered_set<int> s;  // Start small to test rehashing behavior
    
    // Insert and selectively delete to trigger rehashing behaviors
    for (int i = 0; i < 20; ++i) {
        s.insert(i);
        // Delete every other element to create deletion patterns
        if (i % 2 == 1) {
            s.erase(i - 1);
        }
    }
    
    // Verify final state - should contain only the odd numbers up to 19
    // (0,2,4,6,8,10,12,14,16,18 were deleted)
    // Remaining: 1,3,5,7,9,11,13,15,17,19
    FL_REQUIRE_EQ(s.size(), 10u);
    
    // Check that the correct elements are still present
    fl::set<int> found_keys;
    for (auto kv : s) {
        found_keys.insert(kv);
    }
    
    FL_REQUIRE_EQ(found_keys.size(), 10u);
    
    // Verify the odd numbers from 1 to 19 are present
    for (int i = 1; i < 20; i += 2) {
        FL_REQUIRE(found_keys.find(i) != found_keys.end());
    }
    
    // Verify the even numbers from 0 to 18 are absent
    for (int i = 0; i < 20; i += 2) {
        FL_REQUIRE(s.find(i) == s.end());
    }
}

FL_TEST_CASE("Type aliases and compatibility") {
    // Test that hash_set alias works
    fl::unordered_set<int> hs;
    hs.insert(123);
    FL_REQUIRE_EQ(hs.size(), 1u);
    FL_REQUIRE(hs.find(123) != hs.end());

    // Test that it behaves the same as unordered_set
    fl::unordered_set<int> HS;
    HS.insert(123);
    FL_REQUIRE_EQ(HS.size(), hs.size());
}

FL_TEST_CASE("Iterator operator*() returns reference to set data, not temporary") {
    // Regression test: Iterator must return reference to data in the set,
    // not a temporary copy. This ensures pointers to dereferenced iterators
    // remain valid as long as the set is alive.

    fl::unordered_set<fl::string> set;
    set.insert("one");
    set.insert("two");
    set.insert("three");

    // Store pointer to the first key
    const fl::string* ptr = nullptr;

    for (const auto& key : set) {
        // key should be a reference to data in the set, not a temporary
        // Taking its address should be safe as long as set is alive
        ptr = &key;
        break;  // Just grab the first one
    }

    // Access the pointer after the loop (set is still alive)
    // This should work correctly without ASAN errors
    FL_REQUIRE(ptr != nullptr);
    FL_REQUIRE(!ptr->empty());  // key is valid
}
