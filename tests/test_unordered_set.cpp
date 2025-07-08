#include "fl/hash_set.h"
#include "fl/str.h"
#include "test.h"

#include <set>
#include <unordered_set>

using namespace fl;

TEST_CASE("Empty set properties") {
    HashSet<int> s;
    REQUIRE_EQ(s.size(), 0u);
    REQUIRE(s.empty());
    REQUIRE(s.find(42) == s.end());
    // begin() == end() on empty set
    REQUIRE(s.begin() == s.end());
}

TEST_CASE("Single insert and lookup") {
    HashSet<int> s;
    s.insert(10);
    REQUIRE_EQ(s.size(), 1u);
    REQUIRE(!s.empty());
    
    auto it = s.find(10);
    REQUIRE(it != s.end());
    REQUIRE_EQ((*it).first, 10);  // HashSet stores key as first in pair
    
    // Test non-existent element
    REQUIRE(s.find(20) == s.end());
}

TEST_CASE("Insert duplicate key does not increase size") {
    HashSet<int> s;
    s.insert(5);
    REQUIRE_EQ(s.size(), 1u);
    
    // Insert same key again
    s.insert(5);
    REQUIRE_EQ(s.size(), 1u);  // Size should remain 1
    REQUIRE(s.find(5) != s.end());
}

TEST_CASE("Multiple distinct inserts and lookups") {
    HashSet<char> s;
    
    // Insert multiple elements
    for (char c = 'a'; c <= 'j'; ++c) {
        s.insert(c);
    }
    
    REQUIRE_EQ(s.size(), 10u);
    
    // Verify all elements are present
    for (char c = 'a'; c <= 'j'; ++c) {
        REQUIRE(s.find(c) != s.end());
    }
    
    // Verify non-existent element
    REQUIRE(s.find('z') == s.end());
}

TEST_CASE("Erase behavior") {
    HashSet<int> s;
    s.insert(5);
    s.insert(10);
    s.insert(15);
    REQUIRE_EQ(s.size(), 3u);
    
    // Erase existing element
    s.erase(10);
    REQUIRE_EQ(s.size(), 2u);
    REQUIRE(s.find(10) == s.end());
    REQUIRE(s.find(5) != s.end());
    REQUIRE(s.find(15) != s.end());
    
    // Erase non-existent element (should not crash)
    s.erase(99);
    REQUIRE_EQ(s.size(), 2u);
    
    // Erase remaining elements
    s.erase(5);
    s.erase(15);
    REQUIRE_EQ(s.size(), 0u);
    REQUIRE(s.empty());
}

TEST_CASE("Re-insert after erase") {
    HashSet<int> s(4);  // Small initial capacity
    s.insert(1);
    s.erase(1);
    REQUIRE(s.find(1) == s.end());
    REQUIRE_EQ(s.size(), 0u);
    
    // Re-insert same element
    s.insert(1);
    REQUIRE(s.find(1) != s.end());
    REQUIRE_EQ(s.size(), 1u);
}

TEST_CASE("Clear resets set") {
    HashSet<int> s;
    for (int i = 0; i < 5; ++i) {
        s.insert(i);
    }
    REQUIRE_EQ(s.size(), 5u);
    
    s.clear();
    REQUIRE_EQ(s.size(), 0u);
    REQUIRE(s.empty());
    
    // Verify all elements are gone
    for (int i = 0; i < 5; ++i) {
        REQUIRE(s.find(i) == s.end());
    }
    
    // Insert after clear should work
    s.insert(100);
    REQUIRE_EQ(s.size(), 1u);
    REQUIRE(s.find(100) != s.end());
}

TEST_CASE("Stress test with many elements and rehashing") {
    HashSet<int> s(1);  // Start with minimal capacity to force rehashing
    const int N = 100;
    
    // Insert many elements
    for (int i = 0; i < N; ++i) {
        s.insert(i);
        REQUIRE_EQ(s.size(), static_cast<fl::size>(i + 1));
    }
    
    REQUIRE_EQ(s.size(), static_cast<fl::size>(N));
    
    // Verify all elements are present
    for (int i = 0; i < N; ++i) {
        REQUIRE(s.find(i) != s.end());
    }
}

TEST_CASE("Iterator functionality") {
    HashSet<int> s;
    fl::size expected_size = 0;
    
    // Insert some elements
    for (int i = 0; i < 10; ++i) {
        s.insert(i * 2);  // Insert even numbers 0, 2, 4, ..., 18
        ++expected_size;
    }
    
    REQUIRE_EQ(s.size(), expected_size);
    
    // Iterate and collect all keys
    std::set<int> found_keys;
    fl::size count = 0;
    
    for (auto it = s.begin(); it != s.end(); ++it) {
        found_keys.insert((*it).first);
        ++count;
    }
    
    REQUIRE_EQ(count, s.size());
    REQUIRE_EQ(found_keys.size(), s.size());
    
    // Verify we found all expected keys
    for (int i = 0; i < 10; ++i) {
        REQUIRE(found_keys.find(i * 2) != found_keys.end());
    }
}

TEST_CASE("Const iterator functionality") {
    HashSet<int> s;
    for (int i = 1; i <= 5; ++i) {
        s.insert(i);
    }
    
    fl::size count = 0;
    std::set<int> found_keys;
    
    // Use const_iterator directly from non-const object
    for (auto it = s.cbegin(); it != s.cend(); ++it) {
        found_keys.insert((*it).first);
        ++count;
    }
    
    REQUIRE_EQ(count, s.size());
    REQUIRE_EQ(found_keys.size(), 5u);
}

TEST_CASE("Range-based for loop") {
    HashSet<int> s;
    for (int i = 10; i < 15; ++i) {
        s.insert(i);
    }
    
    std::set<int> found_keys;
    fl::size count = 0;
    
    // Range-based for loop
    for (const auto& kv : s) {
        found_keys.insert(kv.first);
        ++count;
    }
    
    REQUIRE_EQ(count, s.size());
    REQUIRE_EQ(found_keys.size(), 5u);
    
    for (int i = 10; i < 15; ++i) {
        REQUIRE(found_keys.find(i) != found_keys.end());
    }
}

TEST_CASE("String elements") {
    HashSet<fl::string> s;
    
    s.insert("hello");
    s.insert("world");
    s.insert("test");
    
    REQUIRE_EQ(s.size(), 3u);
    REQUIRE(s.find("hello") != s.end());
    REQUIRE(s.find("world") != s.end());
    REQUIRE(s.find("test") != s.end());
    REQUIRE(s.find("missing") == s.end());
    
    // Erase string element
    s.erase("world");
    REQUIRE_EQ(s.size(), 2u);
    REQUIRE(s.find("world") == s.end());
    REQUIRE(s.find("hello") != s.end());
    REQUIRE(s.find("test") != s.end());
}

TEST_CASE("Capacity management") {
    HashSet<int> s(16, 0.75f);  // Initial capacity 16, load factor 0.75
    
    // Initial state
    REQUIRE_EQ(s.size(), 0u);
    REQUIRE_GE(s.capacity(), 16u);
    
    // Fill beyond initial capacity to test growth
    for (int i = 0; i < 20; ++i) {
        s.insert(i);
    }
    
    REQUIRE_EQ(s.size(), 20u);
    // Capacity should have grown
    REQUIRE_GE(s.capacity(), 20u);
}

TEST_CASE("Custom hash and equality") {
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
    
    HashSet<fl::string, CaseInsensitiveHash, CaseInsensitiveEqual> s;
    
    s.insert("Hello");
    s.insert("WORLD");
    s.insert("test");
    
    REQUIRE_EQ(s.size(), 3u);
    
    // These should be found due to case-insensitive comparison
    REQUIRE(s.find("hello") != s.end());
    REQUIRE(s.find("HELLO") != s.end());
    REQUIRE(s.find("world") != s.end());
    REQUIRE(s.find("World") != s.end());
    REQUIRE(s.find("TEST") != s.end());
    
    // Insert duplicate in different case should not increase size
    s.insert("hello");
    s.insert("HELLO");
    REQUIRE_EQ(s.size(), 3u);
}

TEST_CASE("Equivalence with std::unordered_set for basic operations") {
    HashSet<int> custom_set;
    std::unordered_set<int> std_set;
    
    // Test insertion
    for (int i = 1; i <= 10; ++i) {
        custom_set.insert(i);
        std_set.insert(i);
    }
    
    REQUIRE_EQ(custom_set.size(), std_set.size());
    
    // Test lookup
    for (int i = 1; i <= 10; ++i) {
        bool custom_found = custom_set.find(i) != custom_set.end();
        bool std_found = std_set.find(i) != std_set.end();
        REQUIRE_EQ(custom_found, std_found);
    }
    
    // Test non-existent element
    REQUIRE_EQ(custom_set.find(99) == custom_set.end(), 
               std_set.find(99) == std_set.end());
    
    // Test erase
    custom_set.erase(5);
    std_set.erase(5);
    REQUIRE_EQ(custom_set.size(), std_set.size());
    REQUIRE_EQ(custom_set.find(5) == custom_set.end(),
               std_set.find(5) == std_set.end());
    
    // Test clear
    custom_set.clear();
    std_set.clear();
    REQUIRE_EQ(custom_set.size(), std_set.size());
    REQUIRE_EQ(custom_set.size(), 0u);
}

TEST_CASE("Edge cases") {
    HashSet<int> s;
    
    // Test with negative numbers
    s.insert(-1);
    s.insert(-100);
    s.insert(0);
    s.insert(100);
    
    REQUIRE_EQ(s.size(), 4u);
    REQUIRE(s.find(-1) != s.end());
    REQUIRE(s.find(-100) != s.end());
    REQUIRE(s.find(0) != s.end());
    REQUIRE(s.find(100) != s.end());
    
    // Test erasing from single-element set
    HashSet<int> single;
    single.insert(42);
    REQUIRE_EQ(single.size(), 1u);
    single.erase(42);
    REQUIRE_EQ(single.size(), 0u);
    REQUIRE(single.empty());
    
    // Test multiple operations on same element
    HashSet<int> multi;
    multi.insert(1);
    multi.insert(1);  // duplicate
    multi.erase(1);
    REQUIRE_EQ(multi.size(), 0u);
    multi.insert(1);   // re-insert
    REQUIRE_EQ(multi.size(), 1u);
    REQUIRE(multi.find(1) != multi.end());
}

TEST_CASE("Large scale operations with deletion patterns") {
    HashSet<int> s(8);  // Start small to test rehashing behavior
    
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
    REQUIRE_EQ(s.size(), 10u);
    
    // Check that the correct elements are still present
    std::set<int> found_keys;
    for (auto kv : s) {
        found_keys.insert(kv.first);
    }
    
    REQUIRE_EQ(found_keys.size(), 10u);
    
    // Verify the odd numbers from 1 to 19 are present
    for (int i = 1; i < 20; i += 2) {
        REQUIRE(found_keys.find(i) != found_keys.end());
    }
    
    // Verify the even numbers from 0 to 18 are absent
    for (int i = 0; i < 20; i += 2) {
        REQUIRE(s.find(i) == s.end());
    }
}

TEST_CASE("Type aliases and compatibility") {
    // Test that hash_set alias works
    fl::hash_set<int> hs;
    hs.insert(123);
    REQUIRE_EQ(hs.size(), 1u);
    REQUIRE(hs.find(123) != hs.end());
    
    // Test that it behaves the same as HashSet
    fl::HashSet<int> HS;
    HS.insert(123);
    REQUIRE_EQ(HS.size(), hs.size());
}
