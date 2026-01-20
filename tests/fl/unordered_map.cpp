// test.cpp
// g++ --std=c++11 test.cpp -o test && ./test

#include "fl/stl/set.h"

#include "fl/stl/unordered_map.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/new.h"
#include "fl/stl/map.h"
#include "fl/stl/vector.h"
#include "doctest.h"
#include "fl/compiler_control.h"
#include "fl/hash.h"
#include "fl/stl/allocator.h"
#include "fl/stl/move.h"
#include "fl/stl/pair.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"
#include "initializer_list"
#include "fl/int.h"

using namespace fl;

TEST_CASE("Empty map properties") {
    fl::unordered_map<int, int> m;
    REQUIRE_EQ(m.size(), 0u);
    REQUIRE(!m.find_value(42));
    // begin()==end() on empty
    REQUIRE(m.begin() == m.end());
}

TEST_CASE("Single insert, lookup & operator[]") {
    fl::unordered_map<int, int> m;
    m.insert(10, 20);
    REQUIRE_EQ(m.size(), 1u);
    auto *v = m.find_value(10);
    REQUIRE(v);
    REQUIRE_EQ(*v, 20);

    // operator[] default-construct & assignment
    fl::unordered_map<int, fl::Str> ms;
    auto &ref = ms[5];
    REQUIRE(ref.empty()); // default-constructed
    REQUIRE_EQ(ms.size(), 1u);
    ref = "hello";
    REQUIRE_EQ(*ms.find_value(5), "hello");

    // operator[] overwrite existing
    ms[5] = "world";
    REQUIRE_EQ(ms.size(), 1u);
    REQUIRE_EQ(*ms.find_value(5), "world");
}

TEST_CASE("Insert duplicate key overwrites without growing") {
    fl::unordered_map<int, fl::Str> m;
    m.insert(1, "foo");
    REQUIRE_EQ(m.size(), 1u);
    REQUIRE_EQ(*m.find_value(1), "foo");

    m.insert(1, "bar");
    REQUIRE_EQ(m.size(), 1u);
    REQUIRE_EQ(*m.find_value(1), "bar");
}

TEST_CASE("Multiple distinct inserts & lookups") {
    fl::unordered_map<char, int> m;
    int count = 0;
    for (char c = 'a'; c < 'a' + 10; ++c) {
        MESSAGE("insert " << count++);
        m.insert(c, c - 'a');
    }
    REQUIRE_EQ(m.size(), 10u);
    for (char c = 'a'; c < 'a' + 10; ++c) {
        auto *v = m.find_value(c);
        REQUIRE(v);
        REQUIRE_EQ(*v, static_cast<int>(c - 'a'));
    }
    REQUIRE(!m.find_value('z'));
}

TEST_CASE("Erase and remove behavior") {
    fl::unordered_map<int, int> m;
    m.insert(5, 55);
    m.insert(6, 66);
    REQUIRE_EQ(m.size(), 2u);

    // erase existing
    REQUIRE(m.erase(5));
    REQUIRE_EQ(m.size(), 1u);
    REQUIRE(!m.find_value(5));

    // erase non-existent
    REQUIRE(!m.erase(5));
    REQUIRE_EQ(m.size(), 1u);

    REQUIRE(m.erase(6));
    REQUIRE_EQ(m.size(), 0u);
}

TEST_CASE("Re-insert after erase reuses slot") {
    fl::unordered_map<int, int> m(4);
    m.insert(1, 10);
    REQUIRE(m.erase(1));
    REQUIRE(!m.find_value(1));
    REQUIRE_EQ(m.size(), 0u);

    m.insert(1, 20);
    REQUIRE(m.find_value(1));
    REQUIRE_EQ(*m.find_value(1), 20);
    REQUIRE_EQ(m.size(), 1u);
}

TEST_CASE("Clear resets map and allows fresh inserts") {
    fl::unordered_map<int, int> m(4);
    for (int i = 0; i < 3; ++i)
        m.insert(i, i);
    m.remove(1);
    REQUIRE_EQ(m.size(), 2u);

    m.clear();
    REQUIRE_EQ(m.size(), 0u);
    REQUIRE(!m.find_value(0));
    REQUIRE(!m.find_value(1));
    REQUIRE(!m.find_value(2));

    m.insert(5, 50);
    REQUIRE_EQ(m.size(), 1u);
    REQUIRE_EQ(*m.find_value(5), 50);
}

TEST_CASE("Stress collisions & rehash with small initial capacity") {
    fl::unordered_map<int, int> m(1 /*capacity*/);
    const int N = 100;
    for (int i = 0; i < N; ++i) {
        m.insert(i, i * 3);
        // test that size is increasing
        REQUIRE_EQ(m.size(), static_cast<fl::size>(i + 1));
    }
    REQUIRE_EQ(m.size(), static_cast<fl::size>(N));
    for (int i = 0; i < N; ++i) {
        auto *v = m.find_value(i);
        REQUIRE(v);
        REQUIRE_EQ(*v, i * 3);
    }
}

TEST_CASE("Iterator round-trip and const-iteration") {
    fl::unordered_map<int, int> m;
    for (int i = 0; i < 20; ++i) {
        m.insert(i, i + 100);
    }

    // non-const iteration
    fl::size count = 0;
    for (auto kv : m) {
        REQUIRE_EQ(kv.second, kv.first + 100);
        ++count;
    }
    REQUIRE_EQ(count, m.size());

    // const iteration
    const auto &cm = m;
    count = 0;
    for (auto it = cm.begin(); it != cm.end(); ++it) {
        auto kv = *it;
        REQUIRE_EQ(kv.second, kv.first + 100);
        ++count;
    }
    REQUIRE_EQ(count, cm.size());
}

TEST_CASE("Remove non-existent returns false, find on const map") {
    fl::unordered_map<int, int> m;
    REQUIRE(!m.remove(999));

    const fl::unordered_map<int, int> cm;
    REQUIRE(!cm.find_value(0));
}

TEST_CASE("Inserting multiple elements while deleting them will trigger inline "
          "rehash") {
    const static int MAX_CAPACITY = 2;
    fl::unordered_map<int, int> m(8 /*capacity*/);
    REQUIRE_EQ(8, m.capacity());
    for (int i = 0; i < 8; ++i) {
        m.insert(i, i);
        if (m.size() > MAX_CAPACITY) {
            m.remove(i);
        }
    }
    size_t new_capacity = m.capacity();
    // should still be 8
    REQUIRE_EQ(new_capacity, 8u);
    fl::set<int> found_values;

    for (auto it = m.begin(); it != m.end(); ++it) {
        auto kv = *it;
        auto key = kv.first;
        auto value = kv.second;
        REQUIRE_EQ(key, value);
        found_values.insert(kv.second);
    }

    fl::vector<int> found_values_vec;
    for (auto it = found_values.begin(); it != found_values.end(); ++it) {
        found_values_vec.push_back(*it);
    }
    REQUIRE_EQ(MAX_CAPACITY, found_values_vec.size());
    for (int i = 0; i < MAX_CAPACITY; ++i) {
        auto value = found_values_vec[i];
        REQUIRE_EQ(value, i);
    }
}

TEST_CASE("HashMap with standard iterator access") {
    fl::unordered_map<int, int> m;
    m.insert(1, 1);

    REQUIRE_EQ(m.size(), 1u);

    // standard iterator access
    auto it = m.begin();
    auto entry = *it;
    REQUIRE_EQ(entry.first, 1);
    REQUIRE_EQ(entry.second, 1);
    ++it;
    REQUIRE(it == m.end());

    auto bad_it = m.find(0);
    REQUIRE(bad_it == m.end());
}

TEST_CASE("at() method - bounds-checked access") {
    fl::unordered_map<int, fl::Str> m;
    m.insert(5, "hello");
    m.insert(10, "world");

    // Valid access
    REQUIRE_EQ(m.at(5), "hello");
    REQUIRE_EQ(m.at(10), "world");

    // const version
    const fl::unordered_map<int, fl::Str>& cm = m;
    REQUIRE_EQ(cm.at(5), "hello");

    // Invalid access should trigger assertion (would fail in debug builds)
    // Note: In release builds without assertions, this may cause undefined behavior
    // So we don't test the failure case here
}

TEST_CASE("count() method - returns 0 or 1") {
    fl::unordered_map<int, int> m;
    m.insert(1, 10);
    m.insert(2, 20);

    REQUIRE_EQ(m.count(1), 1u);
    REQUIRE_EQ(m.count(2), 1u);
    REQUIRE_EQ(m.count(99), 0u);

    m.erase(1);
    REQUIRE_EQ(m.count(1), 0u);
    REQUIRE_EQ(m.count(2), 1u);
}

TEST_CASE("equal_range() method") {
    fl::unordered_map<int, int> m;
    m.insert(1, 10);
    m.insert(2, 20);
    m.insert(3, 30);

    // Found element
    auto range = m.equal_range(2);
    REQUIRE(range.first != m.end());
    REQUIRE_EQ((*range.first).first, 2);
    REQUIRE_EQ((*range.first).second, 20);
    // range.second should be one past range.first
    REQUIRE(range.first != range.second);

    // Not found element
    auto range_none = m.equal_range(99);
    REQUIRE(range_none.first == m.end());
    REQUIRE(range_none.second == m.end());

    // const version
    const fl::unordered_map<int, int>& cm = m;
    auto crange = cm.equal_range(1);
    REQUIRE(crange.first != cm.end());
    REQUIRE_EQ((*crange.first).first, 1);
}

TEST_CASE("max_size() method") {
    fl::unordered_map<int, int> m;
    fl::size max = m.max_size();
    // max_size should be a large number
    REQUIRE(max > 0u);
    REQUIRE(max > 1000u); // should be able to hold at least 1000 elements
}

TEST_CASE("hash_function() and key_eq() observers") {
    fl::unordered_map<int, int> m;
    auto hash_fn = m.hash_function();
    auto eq_fn = m.key_eq();

    // Test that the hash function returns consistent values
    int key1 = 42;
    int key2 = 42;
    int key3 = 43;
    REQUIRE_EQ(hash_fn(key1), hash_fn(key2));
    // Different keys likely have different hashes (not guaranteed, but probable)

    // Test that the equality function works
    REQUIRE(eq_fn(key1, key2));
    REQUIRE(!eq_fn(key1, key3));
}

TEST_CASE("insert() returns pair<iterator, bool> - new elements") {
    fl::unordered_map<int, fl::Str> m;

    // First insert of a new key should return {iterator, true}
    auto result1 = m.insert(5, "hello");
    REQUIRE(result1.second == true);  // was inserted
    REQUIRE(result1.first != m.end()); // valid iterator
    REQUIRE_EQ((*result1.first).first, 5);
    REQUIRE_EQ((*result1.first).second, "hello");
    REQUIRE_EQ(m.size(), 1u);

    // Second insert with different key
    auto result2 = m.insert(10, "world");
    REQUIRE(result2.second == true);  // was inserted
    REQUIRE(result2.first != m.end());
    REQUIRE_EQ((*result2.first).first, 10);
    REQUIRE_EQ((*result2.first).second, "world");
    REQUIRE_EQ(m.size(), 2u);
}

TEST_CASE("insert() returns pair<iterator, bool> - duplicate keys") {
    fl::unordered_map<int, fl::Str> m;

    // First insert
    auto result1 = m.insert(5, "hello");
    REQUIRE(result1.second == true);
    REQUIRE_EQ(m.size(), 1u);

    // Insert with same key should return {iterator, false} and update value
    auto result2 = m.insert(5, "goodbye");
    REQUIRE(result2.second == false);  // not inserted (updated)
    REQUIRE(result2.first != m.end());
    REQUIRE_EQ((*result2.first).first, 5);
    REQUIRE_EQ((*result2.first).second, "goodbye"); // value updated
    REQUIRE_EQ(m.size(), 1u); // size unchanged
}

TEST_CASE("insert() move version returns pair<iterator, bool>") {
    fl::unordered_map<int, fl::Str> m;

    // Move insert of new key
    fl::Str s1 = "movable";
    auto result1 = m.insert(7, fl::move(s1));
    REQUIRE(result1.second == true);
    REQUIRE(result1.first != m.end());
    REQUIRE_EQ((*result1.first).first, 7);
    REQUIRE_EQ((*result1.first).second, "movable");

    // Move insert of duplicate key
    fl::Str s2 = "replaced";
    auto result2 = m.insert(7, fl::move(s2));
    REQUIRE(result2.second == false);  // not inserted (updated)
    REQUIRE_EQ((*result2.first).second, "replaced");
    REQUIRE_EQ(m.size(), 1u);
}

TEST_CASE("insert() return iterator is usable") {
    fl::unordered_map<int, int> m;

    auto result = m.insert(42, 100);
    REQUIRE(result.second == true);

    // Use the returned iterator
    auto it = result.first;
    REQUIRE(it != m.end());
    REQUIRE_EQ((*it).first, 42);
    REQUIRE_EQ((*it).second, 100);

    // Iterator should be incrementable
    ++it;
    REQUIRE(it == m.end()); // only one element
}

TEST_CASE("insert(pair) - const pair insert") {
    fl::unordered_map<int, fl::Str> m;

    // Insert using pair
    fl::pair<int, Str> p1(5, "hello");
    auto result1 = m.insert(p1);
    REQUIRE(result1.second == true);  // was inserted
    REQUIRE(result1.first != m.end());
    REQUIRE_EQ((*result1.first).first, 5);
    REQUIRE_EQ((*result1.first).second, "hello");
    REQUIRE_EQ(m.size(), 1u);

    // Insert duplicate key with pair
    fl::pair<int, Str> p2(5, "world");
    auto result2 = m.insert(p2);
    REQUIRE(result2.second == false);  // not inserted (updated)
    REQUIRE_EQ((*result2.first).second, "world");
    REQUIRE_EQ(m.size(), 1u);

    // Insert new key with pair
    fl::pair<int, Str> p3(10, "foo");
    auto result3 = m.insert(p3);
    REQUIRE(result3.second == true);  // was inserted
    REQUIRE_EQ((*result3.first).first, 10);
    REQUIRE_EQ((*result3.first).second, "foo");
    REQUIRE_EQ(m.size(), 2u);
}

TEST_CASE("insert(pair) - move pair insert") {
    fl::unordered_map<int, fl::Str> m;

    // Insert using moved pair
    fl::pair<int, Str> p1(7, "movable");
    auto result1 = m.insert(fl::move(p1));
    REQUIRE(result1.second == true);
    REQUIRE(result1.first != m.end());
    REQUIRE_EQ((*result1.first).first, 7);
    REQUIRE_EQ((*result1.first).second, "movable");
    REQUIRE_EQ(m.size(), 1u);

    // Insert duplicate key with moved pair
    fl::pair<int, Str> p2(7, "replaced");
    auto result2 = m.insert(fl::move(p2));
    REQUIRE(result2.second == false);  // not inserted (updated)
    REQUIRE_EQ((*result2.first).second, "replaced");
    REQUIRE_EQ(m.size(), 1u);
}

TEST_CASE("insert(pair) - inline pair creation") {
    fl::unordered_map<int, int> m;

    // Use inline pair creation (like std::make_pair)
    auto result1 = m.insert(fl::pair<int, int>(42, 100));
    REQUIRE(result1.second == true);
    REQUIRE_EQ((*result1.first).first, 42);
    REQUIRE_EQ((*result1.first).second, 100);

    // Update with inline pair
    auto result2 = m.insert(fl::pair<int, int>(42, 200));
    REQUIRE(result2.second == false);
    REQUIRE_EQ((*result2.first).second, 200);
    REQUIRE_EQ(m.size(), 1u);
}

TEST_CASE("insert(InputIt first, InputIt last) - range insert") {
    fl::unordered_map<int, fl::Str> m;

    // Create a vector of pairs to insert
    fl::vector<fl::pair<int, Str>> pairs;
    pairs.push_back(fl::pair<int, Str>(1, "one"));
    pairs.push_back(fl::pair<int, Str>(2, "two"));
    pairs.push_back(fl::pair<int, Str>(3, "three"));
    pairs.push_back(fl::pair<int, Str>(4, "four"));

    // Insert range of pairs
    m.insert(pairs.begin(), pairs.end());

    // Verify all elements were inserted
    REQUIRE_EQ(m.size(), 4u);
    REQUIRE_EQ(m[1], "one");
    REQUIRE_EQ(m[2], "two");
    REQUIRE_EQ(m[3], "three");
    REQUIRE_EQ(m[4], "four");

    // Insert range with duplicate keys (should update values)
    fl::vector<fl::pair<int, Str>> more_pairs;
    more_pairs.push_back(fl::pair<int, Str>(2, "TWO"));  // duplicate
    more_pairs.push_back(fl::pair<int, Str>(5, "five")); // new

    m.insert(more_pairs.begin(), more_pairs.end());

    // Size should be 5 (4 original + 1 new)
    REQUIRE_EQ(m.size(), 5u);
    REQUIRE_EQ(m[2], "TWO");  // value updated
    REQUIRE_EQ(m[5], "five"); // new value
}

TEST_CASE("insert(InputIt first, InputIt last) - empty range") {
    fl::unordered_map<int, int> m;
    m.insert(1, 100);

    // Insert empty range should not change the map
    fl::vector<fl::pair<int, int>> empty;
    m.insert(empty.begin(), empty.end());

    REQUIRE_EQ(m.size(), 1u);
    REQUIRE_EQ(m[1], 100);
}

TEST_CASE("insert(initializer_list) - basic usage") {
    fl::unordered_map<int, fl::Str> m;

    // Insert using initializer list syntax
    m.insert({{1, "one"}, {2, "two"}, {3, "three"}});

    // Verify all elements were inserted
    REQUIRE_EQ(m.size(), 3u);
    REQUIRE_EQ(m[1], "one");
    REQUIRE_EQ(m[2], "two");
    REQUIRE_EQ(m[3], "three");
}

TEST_CASE("insert(initializer_list) - with duplicates") {
    fl::unordered_map<int, int> m;
    m.insert(1, 100);
    m.insert(2, 200);

    // Insert initializer list with some duplicates
    m.insert({{2, 222}, {3, 333}, {4, 444}});

    // Size should be 4 (original 1,2 + new 3,4)
    REQUIRE_EQ(m.size(), 4u);
    REQUIRE_EQ(m[1], 100);   // unchanged
    REQUIRE_EQ(m[2], 222);   // updated
    REQUIRE_EQ(m[3], 333);   // new
    REQUIRE_EQ(m[4], 444);   // new
}

TEST_CASE("insert(initializer_list) - empty list") {
    fl::unordered_map<int, int> m;
    m.insert(1, 100);

    // Insert empty initializer list
    m.insert({});

    REQUIRE_EQ(m.size(), 1u);
    REQUIRE_EQ(m[1], 100);
}

TEST_CASE("insert(initializer_list) - complex types") {
    fl::unordered_map<int, fl::Str> m;

    // Insert complex string types
    m.insert({{10, "hello"}, {20, "world"}, {30, "fastled"}});

    REQUIRE_EQ(m.size(), 3u);
    REQUIRE_EQ(m[10], "hello");
    REQUIRE_EQ(m[20], "world");
    REQUIRE_EQ(m[30], "fastled");
}

TEST_CASE("insert_or_assign() - insert new elements") {
    fl::unordered_map<int, fl::Str> m;

    // Insert new element
    fl::Str val1 = "hello";
    auto result1 = m.insert_or_assign(5, fl::move(val1));
    REQUIRE(result1.second == true);  // was inserted
    REQUIRE(result1.first != m.end());
    REQUIRE_EQ((*result1.first).first, 5);
    REQUIRE_EQ((*result1.first).second, "hello");
    REQUIRE_EQ(m.size(), 1u);

    // Insert another new element
    fl::Str val2 = "world";
    auto result2 = m.insert_or_assign(10, fl::move(val2));
    REQUIRE(result2.second == true);
    REQUIRE_EQ((*result2.first).first, 10);
    REQUIRE_EQ((*result2.first).second, "world");
    REQUIRE_EQ(m.size(), 2u);
}

TEST_CASE("insert_or_assign() - update existing elements") {
    fl::unordered_map<int, fl::Str> m;

    // Insert initial value
    fl::Str val1 = "hello";
    auto result1 = m.insert_or_assign(5, fl::move(val1));
    REQUIRE(result1.second == true);
    REQUIRE_EQ(m[5], "hello");

    // Update with insert_or_assign
    fl::Str val2 = "goodbye";
    auto result2 = m.insert_or_assign(5, fl::move(val2));
    REQUIRE(result2.second == false);  // not inserted (updated)
    REQUIRE(result2.first != m.end());
    REQUIRE_EQ((*result2.first).first, 5);
    REQUIRE_EQ((*result2.first).second, "goodbye");
    REQUIRE_EQ(m.size(), 1u);  // size unchanged
}

TEST_CASE("insert_or_assign() - move key version") {
    fl::unordered_map<int, fl::Str> m;

    // Insert with moved key and value
    int key1 = 42;
    fl::Str val1 = "answer";
    auto result1 = m.insert_or_assign(fl::move(key1), fl::move(val1));
    REQUIRE(result1.second == true);
    REQUIRE_EQ((*result1.first).first, 42);
    REQUIRE_EQ((*result1.first).second, "answer");

    // Update with moved key and value
    int key2 = 42;
    fl::Str val2 = "new answer";
    auto result2 = m.insert_or_assign(fl::move(key2), fl::move(val2));
    REQUIRE(result2.second == false);
    REQUIRE_EQ((*result2.first).second, "new answer");
    REQUIRE_EQ(m.size(), 1u);
}

TEST_CASE("emplace() - basic usage") {
    fl::unordered_map<int, fl::Str> m;

    // Emplace with key and value arguments
    auto result1 = m.emplace(5, "hello");
    REQUIRE(result1.second == true);  // was inserted
    REQUIRE(result1.first != m.end());
    REQUIRE_EQ((*result1.first).first, 5);
    REQUIRE_EQ((*result1.first).second, "hello");
    REQUIRE_EQ(m.size(), 1u);

    // Emplace another element
    auto result2 = m.emplace(10, "world");
    REQUIRE(result2.second == true);
    REQUIRE_EQ((*result2.first).first, 10);
    REQUIRE_EQ((*result2.first).second, "world");
    REQUIRE_EQ(m.size(), 2u);
}

TEST_CASE("emplace() - duplicate key") {
    fl::unordered_map<int, fl::Str> m;

    // First emplace
    auto result1 = m.emplace(5, "hello");
    REQUIRE(result1.second == true);
    REQUIRE_EQ(m[5], "hello");

    // Emplace with duplicate key (should not insert, returns existing)
    auto result2 = m.emplace(5, "goodbye");
    REQUIRE(result2.second == false);  // not inserted
    REQUIRE_EQ((*result2.first).first, 5);
    // Note: Our emplace implementation uses insert, which updates the value
    REQUIRE_EQ((*result2.first).second, "goodbye");
    REQUIRE_EQ(m.size(), 1u);
}

TEST_CASE("emplace() - with POD types") {
    fl::unordered_map<int, int> m;

    // Emplace simple POD types
    auto result1 = m.emplace(1, 100);
    REQUIRE(result1.second == true);
    REQUIRE_EQ((*result1.first).first, 1);
    REQUIRE_EQ((*result1.first).second, 100);

    auto result2 = m.emplace(2, 200);
    REQUIRE(result2.second == true);
    REQUIRE_EQ(m.size(), 2u);
    REQUIRE_EQ(m[1], 100);
    REQUIRE_EQ(m[2], 200);
}

TEST_CASE("emplace_hint() - basic usage") {
    fl::unordered_map<int, fl::Str> m;

    // emplace_hint with hint (hint is ignored in hash maps)
    auto it1 = m.emplace_hint(m.end(), 5, "hello");
    REQUIRE(it1 != m.end());
    REQUIRE_EQ((*it1).first, 5);
    REQUIRE_EQ((*it1).second, "hello");
    REQUIRE_EQ(m.size(), 1u);

    // emplace_hint another element
    auto it2 = m.emplace_hint(m.begin(), 10, "world");
    REQUIRE(it2 != m.end());
    REQUIRE_EQ((*it2).first, 10);
    REQUIRE_EQ((*it2).second, "world");
    REQUIRE_EQ(m.size(), 2u);
}

TEST_CASE("emplace_hint() - hint is ignored") {
    fl::unordered_map<int, int> m;
    m.insert(1, 100);
    m.insert(2, 200);

    // emplace_hint with arbitrary hint - should work same as emplace
    auto hint = m.find(1);
    auto it = m.emplace_hint(hint, 3, 300);
    REQUIRE(it != m.end());
    REQUIRE_EQ((*it).first, 3);
    REQUIRE_EQ((*it).second, 300);
    REQUIRE_EQ(m.size(), 3u);
}

TEST_CASE("try_emplace() - insert new elements") {
    fl::unordered_map<int, fl::Str> m;

    // try_emplace with new key
    auto result1 = m.try_emplace(5, "hello");
    REQUIRE(result1.second == true);  // was inserted
    REQUIRE(result1.first != m.end());
    REQUIRE_EQ((*result1.first).first, 5);
    REQUIRE_EQ((*result1.first).second, "hello");
    REQUIRE_EQ(m.size(), 1u);

    // try_emplace another new element
    auto result2 = m.try_emplace(10, "world");
    REQUIRE(result2.second == true);
    REQUIRE_EQ((*result2.first).first, 10);
    REQUIRE_EQ((*result2.first).second, "world");
    REQUIRE_EQ(m.size(), 2u);
}

TEST_CASE("try_emplace() - does not modify existing key") {
    fl::unordered_map<int, fl::Str> m;

    // First try_emplace
    auto result1 = m.try_emplace(5, "hello");
    REQUIRE(result1.second == true);
    REQUIRE_EQ(m[5], "hello");

    // try_emplace with duplicate key - should NOT modify the value
    auto result2 = m.try_emplace(5, "goodbye");
    REQUIRE(result2.second == false);  // not inserted
    REQUIRE(result2.first != m.end());
    REQUIRE_EQ((*result2.first).first, 5);
    REQUIRE_EQ((*result2.first).second, "hello");  // value unchanged!
    REQUIRE_EQ(m.size(), 1u);
}

TEST_CASE("try_emplace() - move key version") {
    fl::unordered_map<int, fl::Str> m;

    // try_emplace with moved key for new element
    int key1 = 42;
    auto result1 = m.try_emplace(fl::move(key1), "answer");
    REQUIRE(result1.second == true);
    REQUIRE_EQ((*result1.first).first, 42);
    REQUIRE_EQ((*result1.first).second, "answer");

    // try_emplace with moved key for duplicate - key should NOT be moved
    int key2 = 42;
    auto result2 = m.try_emplace(fl::move(key2), "new answer");
    REQUIRE(result2.second == false);  // not inserted
    REQUIRE_EQ((*result2.first).second, "answer");  // value unchanged
    REQUIRE_EQ(m.size(), 1u);
}

TEST_CASE("try_emplace() - with POD types") {
    fl::unordered_map<int, int> m;

    // try_emplace simple POD types
    auto result1 = m.try_emplace(1, 100);
    REQUIRE(result1.second == true);
    REQUIRE_EQ((*result1.first).first, 1);
    REQUIRE_EQ((*result1.first).second, 100);

    // Duplicate key
    auto result2 = m.try_emplace(1, 999);
    REQUIRE(result2.second == false);
    REQUIRE_EQ((*result2.first).second, 100);  // value unchanged
    REQUIRE_EQ(m.size(), 1u);
}

TEST_CASE("try_emplace() - constructs value in place") {
    fl::unordered_map<int, fl::Str> m;

    // try_emplace constructs value from args
    auto result = m.try_emplace(1, "constructed");
    REQUIRE(result.second == true);
    REQUIRE_EQ(m[1], "constructed");

    // Verify no construction happens on duplicate
    auto result2 = m.try_emplace(1, "not constructed");
    REQUIRE(result2.second == false);
    REQUIRE_EQ(m[1], "constructed");  // still the original
}

TEST_CASE("try_emplace() vs emplace() - behavior difference") {
    fl::unordered_map<int, fl::Str> m;

    // Both insert new keys similarly
    m.try_emplace(1, "one");
    m.emplace(2, "two");
    REQUIRE_EQ(m[1], "one");
    REQUIRE_EQ(m[2], "two");

    // Difference: try_emplace does NOT update existing key
    auto result1 = m.try_emplace(1, "ONE");
    REQUIRE(result1.second == false);
    REQUIRE_EQ(m[1], "one");  // unchanged

    // emplace DOES update existing key (in our implementation)
    auto result2 = m.emplace(2, "TWO");
    REQUIRE(result2.second == false);
    REQUIRE_EQ(m[2], "TWO");  // updated
}

// Phase 3: Constructors & Assignment Operators Tests

TEST_CASE("Copy constructor - basic usage") {
    fl::unordered_map<int, fl::Str> m1;
    m1.insert(1, "one");
    m1.insert(2, "two");
    m1.insert(3, "three");
    REQUIRE_EQ(m1.size(), 3u);

    // Copy construct
    fl::unordered_map<int, fl::Str> m2(m1);

    // Verify m2 has same contents
    REQUIRE_EQ(m2.size(), 3u);
    REQUIRE_EQ(m2[1], "one");
    REQUIRE_EQ(m2[2], "two");
    REQUIRE_EQ(m2[3], "three");

    // Verify m1 is unchanged
    REQUIRE_EQ(m1.size(), 3u);
    REQUIRE_EQ(m1[1], "one");

    // Verify they are independent (modifying m2 doesn't affect m1)
    m2[1] = "ONE";
    REQUIRE_EQ(m2[1], "ONE");
    REQUIRE_EQ(m1[1], "one");  // unchanged
}

TEST_CASE("Copy constructor - empty map") {
    fl::unordered_map<int, int> m1;
    REQUIRE_EQ(m1.size(), 0u);

    fl::unordered_map<int, int> m2(m1);
    REQUIRE_EQ(m2.size(), 0u);
    REQUIRE(m2.empty());

    // Can still insert into copied empty map
    m2.insert(1, 100);
    REQUIRE_EQ(m2.size(), 1u);
    REQUIRE_EQ(m1.size(), 0u);  // original unchanged
}

TEST_CASE("Copy constructor - with tombstones") {
    fl::unordered_map<int, int> m1;
    m1.insert(1, 10);
    m1.insert(2, 20);
    m1.insert(3, 30);
    m1.erase(2);  // Create a tombstone
    REQUIRE_EQ(m1.size(), 2u);

    // Copy should only copy live entries, not tombstones
    fl::unordered_map<int, int> m2(m1);
    REQUIRE_EQ(m2.size(), 2u);
    REQUIRE_EQ(m2[1], 10);
    REQUIRE_EQ(m2[3], 30);
    REQUIRE_EQ(m2.count(2), 0u);  // tombstone not copied
}

TEST_CASE("Move constructor - basic usage") {
    fl::unordered_map<int, fl::Str> m1;
    m1.insert(1, "one");
    m1.insert(2, "two");
    m1.insert(3, "three");
    REQUIRE_EQ(m1.size(), 3u);

    // Move construct
    fl::unordered_map<int, fl::Str> m2(fl::move(m1));

    // Verify m2 has the contents
    REQUIRE_EQ(m2.size(), 3u);
    REQUIRE_EQ(m2[1], "one");
    REQUIRE_EQ(m2[2], "two");
    REQUIRE_EQ(m2[3], "three");

    // Verify m1 is in valid empty state
    REQUIRE_EQ(m1.size(), 0u);
    REQUIRE(m1.empty());
}

TEST_CASE("Move constructor - empty map") {
    fl::unordered_map<int, int> m1;
    REQUIRE_EQ(m1.size(), 0u);

    fl::unordered_map<int, int> m2(fl::move(m1));
    REQUIRE_EQ(m2.size(), 0u);
    REQUIRE(m2.empty());

    // Both should be in valid state
    m2.insert(1, 100);
    REQUIRE_EQ(m2.size(), 1u);
}

TEST_CASE("Range constructor - from vector") {
    fl::vector<fl::pair<int, Str>> pairs;
    pairs.push_back(fl::pair<int, Str>(1, "one"));
    pairs.push_back(fl::pair<int, Str>(2, "two"));
    pairs.push_back(fl::pair<int, Str>(3, "three"));

    // Construct from range
    fl::unordered_map<int, fl::Str> m(pairs.begin(), pairs.end());

    REQUIRE_EQ(m.size(), 3u);
    REQUIRE_EQ(m[1], "one");
    REQUIRE_EQ(m[2], "two");
    REQUIRE_EQ(m[3], "three");
}

TEST_CASE("Range constructor - empty range") {
    fl::vector<fl::pair<int, int>> empty;

    fl::unordered_map<int, int> m(empty.begin(), empty.end());
    REQUIRE_EQ(m.size(), 0u);
    REQUIRE(m.empty());
}

TEST_CASE("Range constructor - with duplicates") {
    fl::vector<fl::pair<int, int>> pairs;
    pairs.push_back(fl::pair<int, int>(1, 100));
    pairs.push_back(fl::pair<int, int>(2, 200));
    pairs.push_back(fl::pair<int, int>(1, 111));  // duplicate key

    fl::unordered_map<int, int> m(pairs.begin(), pairs.end());

    // Size should be 2 (keys 1 and 2)
    REQUIRE_EQ(m.size(), 2u);
    REQUIRE_EQ(m[1], 111);  // last value for key 1
    REQUIRE_EQ(m[2], 200);
}

TEST_CASE("Initializer list constructor - basic usage") {
    fl::unordered_map<int, fl::Str> m({{1, "one"}, {2, "two"}, {3, "three"}});

    REQUIRE_EQ(m.size(), 3u);
    REQUIRE_EQ(m[1], "one");
    REQUIRE_EQ(m[2], "two");
    REQUIRE_EQ(m[3], "three");
}

TEST_CASE("Initializer list constructor - empty list") {
    fl::unordered_map<int, int> m({});
    REQUIRE_EQ(m.size(), 0u);
    REQUIRE(m.empty());
}

TEST_CASE("Initializer list constructor - with duplicates") {
    fl::unordered_map<int, int> m({{1, 100}, {2, 200}, {1, 111}});

    // Size should be 2 (keys 1 and 2)
    REQUIRE_EQ(m.size(), 2u);
    REQUIRE_EQ(m[1], 111);  // last value for key 1
    REQUIRE_EQ(m[2], 200);
}

TEST_CASE("Constructor with hash and equal parameters") {
    // Custom hash and equality functors
    fl::Hash<int> custom_hash;
    fl::EqualTo<int> custom_equal;

    fl::unordered_map<int, fl::Str> m(16, custom_hash, custom_equal);

    // Verify it's usable
    m.insert(1, "one");
    m.insert(2, "two");
    REQUIRE_EQ(m.size(), 2u);
    REQUIRE_EQ(m[1], "one");
    REQUIRE_EQ(m[2], "two");

    // Verify hash and equal functions work
    auto hash_fn = m.hash_function();
    auto eq_fn = m.key_eq();
    REQUIRE_EQ(hash_fn(5), hash_fn(5));
    REQUIRE(eq_fn(5, 5));
    REQUIRE(!eq_fn(5, 6));
}

TEST_CASE("Copy assignment operator - basic usage") {
    fl::unordered_map<int, fl::Str> m1;
    m1.insert(1, "one");
    m1.insert(2, "two");
    m1.insert(3, "three");

    fl::unordered_map<int, fl::Str> m2;
    m2.insert(99, "old");

    // Copy assign
    m2 = m1;

    // Verify m2 has same contents as m1
    REQUIRE_EQ(m2.size(), 3u);
    REQUIRE_EQ(m2[1], "one");
    REQUIRE_EQ(m2[2], "two");
    REQUIRE_EQ(m2[3], "three");
    REQUIRE_EQ(m2.count(99), 0u);  // old content gone

    // Verify m1 is unchanged
    REQUIRE_EQ(m1.size(), 3u);
    REQUIRE_EQ(m1[1], "one");

    // Verify independence
    m2[1] = "ONE";
    REQUIRE_EQ(m2[1], "ONE");
    REQUIRE_EQ(m1[1], "one");
}

TEST_CASE("Copy assignment operator - self assignment") {
    fl::unordered_map<int, fl::Str> m;
    m.insert(1, "one");
    m.insert(2, "two");

    // Self assignment
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING_SELF_ASSIGN_OVERLOADED
    m = m;
    FL_DISABLE_WARNING_POP

    // Should be unchanged
    REQUIRE_EQ(m.size(), 2u);
    REQUIRE_EQ(m[1], "one");
    REQUIRE_EQ(m[2], "two");
}

TEST_CASE("Copy assignment operator - to empty map") {
    fl::unordered_map<int, int> m1;
    m1.insert(1, 10);
    m1.insert(2, 20);

    fl::unordered_map<int, int> m2;  // empty
    m2 = m1;

    REQUIRE_EQ(m2.size(), 2u);
    REQUIRE_EQ(m2[1], 10);
    REQUIRE_EQ(m2[2], 20);
}

TEST_CASE("Copy assignment operator - from empty map") {
    fl::unordered_map<int, int> m1;  // empty

    fl::unordered_map<int, int> m2;
    m2.insert(1, 10);
    m2.insert(2, 20);

    m2 = m1;

    REQUIRE_EQ(m2.size(), 0u);
    REQUIRE(m2.empty());
}

TEST_CASE("Move assignment operator - basic usage") {
    fl::unordered_map<int, fl::Str> m1;
    m1.insert(1, "one");
    m1.insert(2, "two");
    m1.insert(3, "three");

    fl::unordered_map<int, fl::Str> m2;
    m2.insert(99, "old");

    // Move assign
    m2 = fl::move(m1);

    // Verify m2 has the contents
    REQUIRE_EQ(m2.size(), 3u);
    REQUIRE_EQ(m2[1], "one");
    REQUIRE_EQ(m2[2], "two");
    REQUIRE_EQ(m2[3], "three");
    REQUIRE_EQ(m2.count(99), 0u);  // old content gone

    // Verify m1 is in valid empty state
    REQUIRE_EQ(m1.size(), 0u);
    REQUIRE(m1.empty());
}

TEST_CASE("Move assignment operator - self assignment") {
    fl::unordered_map<int, fl::Str> m;
    m.insert(1, "one");
    m.insert(2, "two");

    // Self assignment (unusual but should be safe)
    m = fl::move(m);

    // Should still be valid (implementation dependent behavior)
    // At minimum, it should not crash
}

TEST_CASE("Move assignment operator - from empty map") {
    fl::unordered_map<int, int> m1;  // empty

    fl::unordered_map<int, int> m2;
    m2.insert(1, 10);
    m2.insert(2, 20);

    m2 = fl::move(m1);

    REQUIRE_EQ(m2.size(), 0u);
    REQUIRE(m2.empty());
}

TEST_CASE("Initializer list assignment operator - basic usage") {
    fl::unordered_map<int, fl::Str> m;
    m.insert(99, "old");
    REQUIRE_EQ(m.size(), 1u);

    // Assign from initializer list
    m = {{1, "one"}, {2, "two"}, {3, "three"}};

    REQUIRE_EQ(m.size(), 3u);
    REQUIRE_EQ(m[1], "one");
    REQUIRE_EQ(m[2], "two");
    REQUIRE_EQ(m[3], "three");
    REQUIRE_EQ(m.count(99), 0u);  // old content gone
}

TEST_CASE("Initializer list assignment operator - empty list") {
    fl::unordered_map<int, int> m;
    m.insert(1, 10);
    m.insert(2, 20);

    // Assign empty list
    m = {};

    REQUIRE_EQ(m.size(), 0u);
    REQUIRE(m.empty());
}

TEST_CASE("Initializer list assignment operator - with duplicates") {
    fl::unordered_map<int, int> m;
    m.insert(99, 999);

    m = {{1, 100}, {2, 200}, {1, 111}};

    // Size should be 2 (keys 1 and 2)
    REQUIRE_EQ(m.size(), 2u);
    REQUIRE_EQ(m[1], 111);  // last value for key 1
    REQUIRE_EQ(m[2], 200);
    REQUIRE_EQ(m.count(99), 0u);  // old content gone
}

TEST_CASE("Chained assignments") {
    fl::unordered_map<int, fl::Str> m1;
    m1.insert(1, "one");
    m1.insert(2, "two");

    fl::unordered_map<int, fl::Str> m2;
    fl::unordered_map<int, fl::Str> m3;

    // Chained copy assignment
    m3 = m2 = m1;

    REQUIRE_EQ(m3.size(), 2u);
    REQUIRE_EQ(m3[1], "one");
    REQUIRE_EQ(m3[2], "two");

    REQUIRE_EQ(m2.size(), 2u);
    REQUIRE_EQ(m2[1], "one");
}

// Phase 4: Erase & Swap Tests

TEST_CASE("erase(const_iterator first, const_iterator last) - basic range") {
    fl::unordered_map<int, fl::Str> m;
    m.insert(1, "one");
    m.insert(2, "two");
    m.insert(3, "three");
    m.insert(4, "four");
    m.insert(5, "five");
    REQUIRE_EQ(m.size(), 5u);

    // Erase range from second to fourth element (not including fourth)
    auto it_begin = m.begin();
    ++it_begin; // skip first element
    auto it_end = it_begin;
    ++it_end;
    ++it_end; // advance two more

    auto result = m.erase(it_begin, it_end);
    REQUIRE(result != m.end());
    REQUIRE_EQ(m.size(), 3u);
}

TEST_CASE("erase(const_iterator first, const_iterator last) - erase all") {
    fl::unordered_map<int, int> m;
    m.insert(1, 10);
    m.insert(2, 20);
    m.insert(3, 30);
    REQUIRE_EQ(m.size(), 3u);

    // Erase entire range
    auto result = m.erase(m.begin(), m.end());
    REQUIRE(result == m.end());
    REQUIRE_EQ(m.size(), 0u);
    REQUIRE(m.empty());
}

TEST_CASE("erase(const_iterator first, const_iterator last) - empty range") {
    fl::unordered_map<int, int> m;
    m.insert(1, 10);
    m.insert(2, 20);
    m.insert(3, 30);
    REQUIRE_EQ(m.size(), 3u);

    // Erase empty range (first == last)
    auto it = m.begin();
    m.erase(it, it);
    REQUIRE_EQ(m.size(), 3u); // size unchanged
}

TEST_CASE("erase(const_iterator first, const_iterator last) - single element") {
    fl::unordered_map<int, fl::Str> m;
    m.insert(1, "one");
    m.insert(2, "two");
    m.insert(3, "three");
    REQUIRE_EQ(m.size(), 3u);

    // Erase single element using range
    auto it_begin = m.begin();
    ++it_begin; // point to second element
    auto it_end = it_begin;
    ++it_end; // one past

    auto result = m.erase(it_begin, it_end);
    REQUIRE(result != m.end());
    REQUIRE_EQ(m.size(), 2u);
}

TEST_CASE("erase(const_iterator first, const_iterator last) - after erase") {
    fl::unordered_map<int, int> m;
    for (int i = 1; i <= 10; ++i) {
        m.insert(i, i * 10);
    }
    REQUIRE_EQ(m.size(), 10u);

    // Erase first half
    auto mid = m.begin();
    for (int i = 0; i < 5; ++i) {
        ++mid;
    }
    auto result = m.erase(m.begin(), mid);

    // Verify size
    REQUIRE_EQ(m.size(), 5u);
    REQUIRE(result != m.end());
}

TEST_CASE("swap() - basic usage") {
    fl::unordered_map<int, fl::Str> m1;
    m1.insert(1, "one");
    m1.insert(2, "two");
    m1.insert(3, "three");

    fl::unordered_map<int, fl::Str> m2;
    m2.insert(10, "ten");
    m2.insert(20, "twenty");

    // Swap contents
    m1.swap(m2);

    // Verify m1 now has m2's old contents
    REQUIRE_EQ(m1.size(), 2u);
    REQUIRE_EQ(m1[10], "ten");
    REQUIRE_EQ(m1[20], "twenty");
    REQUIRE_EQ(m1.count(1), 0u);

    // Verify m2 now has m1's old contents
    REQUIRE_EQ(m2.size(), 3u);
    REQUIRE_EQ(m2[1], "one");
    REQUIRE_EQ(m2[2], "two");
    REQUIRE_EQ(m2[3], "three");
    REQUIRE_EQ(m2.count(10), 0u);
}

TEST_CASE("swap() - with empty map") {
    fl::unordered_map<int, int> m1;
    m1.insert(1, 10);
    m1.insert(2, 20);
    m1.insert(3, 30);

    fl::unordered_map<int, int> m2; // empty

    m1.swap(m2);

    // m1 should now be empty
    REQUIRE_EQ(m1.size(), 0u);
    REQUIRE(m1.empty());

    // m2 should have old m1 contents
    REQUIRE_EQ(m2.size(), 3u);
    REQUIRE_EQ(m2[1], 10);
    REQUIRE_EQ(m2[2], 20);
    REQUIRE_EQ(m2[3], 30);
}

TEST_CASE("swap() - two empty maps") {
    fl::unordered_map<int, int> m1;
    fl::unordered_map<int, int> m2;

    m1.swap(m2);

    REQUIRE(m1.empty());
    REQUIRE(m2.empty());
}

TEST_CASE("swap() - preserves independent state") {
    fl::unordered_map<int, int> m1;
    m1.insert(1, 100);
    fl::unordered_map<int, int> m2;
    m2.insert(2, 200);

    m1.swap(m2);

    // Modify m1 and verify m2 is unaffected
    m1[2] = 999;
    REQUIRE_EQ(m1[2], 999);
    REQUIRE_EQ(m2[1], 100);  // m2 unchanged

    // Modify m2 and verify m1 is unaffected
    m2[1] = 777;
    REQUIRE_EQ(m2[1], 777);
    REQUIRE_EQ(m1[2], 999);  // m1 unchanged
}

TEST_CASE("swap() - different capacities") {
    fl::unordered_map<int, int> m1(4);  // small capacity
    m1.insert(1, 10);

    fl::unordered_map<int, int> m2(64); // large capacity
    for (int i = 10; i < 20; ++i) {
        m2.insert(i, i * 10);
    }

    fl::size cap1_before = m1.capacity();
    fl::size cap2_before = m2.capacity();

    m1.swap(m2);

    // Capacities should be swapped
    REQUIRE_EQ(m1.capacity(), cap2_before);
    REQUIRE_EQ(m2.capacity(), cap1_before);

    // Contents should be swapped
    REQUIRE_EQ(m1.size(), 10u);
    REQUIRE_EQ(m2.size(), 1u);
    REQUIRE_EQ(m2[1], 10);
    REQUIRE_EQ(m1[10], 100);
}

TEST_CASE("swap() - with tombstones") {
    fl::unordered_map<int, int> m1;
    m1.insert(1, 10);
    m1.insert(2, 20);
    m1.insert(3, 30);
    m1.erase(2); // create tombstone
    REQUIRE_EQ(m1.size(), 2u);

    fl::unordered_map<int, int> m2;
    m2.insert(100, 1000);

    m1.swap(m2);

    // m1 should have m2's contents
    REQUIRE_EQ(m1.size(), 1u);
    REQUIRE_EQ(m1[100], 1000);

    // m2 should have m1's contents (including tombstone state)
    REQUIRE_EQ(m2.size(), 2u);
    REQUIRE_EQ(m2[1], 10);
    REQUIRE_EQ(m2[3], 30);
    REQUIRE_EQ(m2.count(2), 0u); // still deleted
}

// Phase 6: Hash Policy Interface Tests

TEST_CASE("load_factor() - basic usage") {
    fl::unordered_map<int, int> m(8);  // 8 buckets
    REQUIRE_EQ(m.size(), 0u);
    REQUIRE_EQ(m.bucket_count(), 8u);

    // Empty map has 0 load factor
    REQUIRE_EQ(m.load_factor(), 0.0f);

    // Add 2 elements: load_factor = 2/8 = 0.25
    m.insert(1, 10);
    m.insert(2, 20);
    REQUIRE_EQ(m.size(), 2u);
    float lf = m.load_factor();
    REQUIRE(lf >= 0.24f);
    REQUIRE(lf <= 0.26f);  // approximately 0.25

    // Add 2 more: load_factor = 4/8 = 0.5
    m.insert(3, 30);
    m.insert(4, 40);
    lf = m.load_factor();
    REQUIRE(lf >= 0.49f);
    REQUIRE(lf <= 0.51f);  // approximately 0.5
}

TEST_CASE("load_factor() - after rehash") {
    fl::unordered_map<int, int> m(8);

    // Fill to trigger rehash
    for (int i = 0; i < 10; ++i) {
        m.insert(i, i * 10);
    }

    // After rehash, load factor should be lower
    fl::size buckets = m.bucket_count();
    fl::size size = m.size();
    float expected_lf = static_cast<float>(size) / static_cast<float>(buckets);
    float actual_lf = m.load_factor();

    REQUIRE(actual_lf >= expected_lf - 0.01f);
    REQUIRE(actual_lf <= expected_lf + 0.01f);
}

TEST_CASE("max_load_factor() - default value") {
    fl::unordered_map<int, int> m;

    // Default max load factor should be 0.7
    float max_lf = m.max_load_factor();
    REQUIRE(max_lf >= 0.69f);
    REQUIRE(max_lf <= 0.71f);
}

TEST_CASE("max_load_factor() - custom value") {
    fl::unordered_map<int, int> m(8, 0.5f);  // Set max load factor to 0.5

    float max_lf = m.max_load_factor();
    REQUIRE(max_lf >= 0.49f);
    REQUIRE(max_lf <= 0.51f);
}

TEST_CASE("max_load_factor(float) - set new value") {
    fl::unordered_map<int, int> m;

    // Set max load factor to 0.6
    m.max_load_factor(0.6f);
    float max_lf = m.max_load_factor();
    REQUIRE(max_lf >= 0.59f);
    REQUIRE(max_lf <= 0.61f);

    // Set max load factor to 0.9
    m.max_load_factor(0.9f);
    max_lf = m.max_load_factor();
    REQUIRE(max_lf >= 0.89f);
    REQUIRE(max_lf <= 0.91f);
}

TEST_CASE("max_load_factor(float) - clamping") {
    fl::unordered_map<int, int> m;

    // Values should be clamped to [0, 1]
    m.max_load_factor(1.5f);  // Should clamp to 1.0
    float max_lf = m.max_load_factor();
    REQUIRE(max_lf >= 0.99f);
    REQUIRE(max_lf <= 1.01f);

    m.max_load_factor(-0.5f);  // Should clamp to 0.0
    max_lf = m.max_load_factor();
    REQUIRE(max_lf >= 0.0f);
    REQUIRE(max_lf <= 0.01f);
}

TEST_CASE("bucket_count() - basic usage") {
    fl::unordered_map<int, int> m1(4);
    REQUIRE_EQ(m1.bucket_count(), 4u);

    fl::unordered_map<int, int> m2(16);
    REQUIRE_EQ(m2.bucket_count(), 16u);

    fl::unordered_map<int, int> m3(100);
    // bucket_count should be next power of 2
    REQUIRE_EQ(m3.bucket_count(), 128u);
}

TEST_CASE("bucket_count() - after rehash") {
    fl::unordered_map<int, int> m(8);
    REQUIRE_EQ(m.bucket_count(), 8u);

    // Fill to trigger automatic rehash
    for (int i = 0; i < 20; ++i) {
        m.insert(i, i * 10);
    }

    // Bucket count should have increased
    REQUIRE(m.bucket_count() > 8u);
}

TEST_CASE("rehash() - increase buckets") {
    fl::unordered_map<int, int> m(8);
    m.insert(1, 10);
    m.insert(2, 20);
    m.insert(3, 30);
    REQUIRE_EQ(m.size(), 3u);
    REQUIRE_EQ(m.bucket_count(), 8u);

    // Rehash to 32 buckets
    m.rehash(32);

    // Verify bucket count increased
    REQUIRE_EQ(m.bucket_count(), 32u);

    // Verify all elements still present
    REQUIRE_EQ(m.size(), 3u);
    REQUIRE_EQ(m[1], 10);
    REQUIRE_EQ(m[2], 20);
    REQUIRE_EQ(m[3], 30);
}

TEST_CASE("rehash() - with smaller value cleans tombstones") {
    fl::unordered_map<int, int> m(16);

    // Insert and delete to create tombstones
    for (int i = 0; i < 10; ++i) {
        m.insert(i, i * 10);
    }
    for (int i = 0; i < 5; ++i) {
        m.erase(i);
    }
    REQUIRE_EQ(m.size(), 5u);

    // Rehash with same or smaller value (should clean tombstones)
    m.rehash(8);

    // Elements should still be present
    REQUIRE_EQ(m.size(), 5u);
    for (int i = 5; i < 10; ++i) {
        REQUIRE_EQ(m[i], i * 10);
    }
}

TEST_CASE("rehash() - empty map") {
    fl::unordered_map<int, int> m(8);
    REQUIRE_EQ(m.size(), 0u);
    REQUIRE_EQ(m.bucket_count(), 8u);

    // Rehash empty map
    m.rehash(16);

    REQUIRE_EQ(m.size(), 0u);
    REQUIRE_EQ(m.bucket_count(), 16u);

    // Should still be usable
    m.insert(1, 10);
    REQUIRE_EQ(m.size(), 1u);
    REQUIRE_EQ(m[1], 10);
}

TEST_CASE("reserve() - basic usage") {
    fl::unordered_map<int, int> m(4);
    REQUIRE_EQ(m.bucket_count(), 4u);

    // Reserve space for 20 elements
    // With max_load_factor 0.7, we need at least ceil(20/0.7) = 29 buckets
    // Next power of 2 is 32
    m.reserve(20);

    fl::size buckets = m.bucket_count();
    REQUIRE(buckets >= 29u);  // At least enough for 20 elements

    // Verify we can insert 20 elements without rehash
    fl::size buckets_before = m.bucket_count();
    for (int i = 0; i < 20; ++i) {
        m.insert(i, i * 10);
    }
    REQUIRE_EQ(m.bucket_count(), buckets_before);  // No rehash occurred
    REQUIRE_EQ(m.size(), 20u);
}

TEST_CASE("reserve() - no-op if already large enough") {
    fl::unordered_map<int, int> m(64);
    fl::size buckets_before = m.bucket_count();

    // Reserve for fewer elements than current capacity supports
    m.reserve(10);

    // Bucket count should be unchanged
    REQUIRE_EQ(m.bucket_count(), buckets_before);
}

TEST_CASE("reserve() - with existing elements") {
    fl::unordered_map<int, int> m(8);

    // Insert some elements
    for (int i = 0; i < 5; ++i) {
        m.insert(i, i * 10);
    }
    REQUIRE_EQ(m.size(), 5u);

    // Reserve space for 50 more elements
    m.reserve(50);

    // All existing elements should still be present
    REQUIRE_EQ(m.size(), 5u);
    for (int i = 0; i < 5; ++i) {
        REQUIRE_EQ(m[i], i * 10);
    }

    // Should be able to add more without rehashing
    fl::size buckets_after_reserve = m.bucket_count();
    for (int i = 5; i < 50; ++i) {
        m.insert(i, i * 10);
    }
    REQUIRE_EQ(m.bucket_count(), buckets_after_reserve);
}

TEST_CASE("reserve() - empty map") {
    fl::unordered_map<int, int> m;

    // Reserve on empty map
    m.reserve(100);

    fl::size buckets = m.bucket_count();
    REQUIRE(buckets >= 100);  // Should have enough for 100 elements

    // Still empty
    REQUIRE_EQ(m.size(), 0u);
    REQUIRE(m.empty());
}

TEST_CASE("Hash policy - comprehensive workflow") {
    fl::unordered_map<int, int> m(8, 0.8f);  // 8 buckets, 0.8 max load factor

    // Check initial state
    REQUIRE_EQ(m.bucket_count(), 8u);
    float max_lf = m.max_load_factor();
    REQUIRE(max_lf >= 0.79f);
    REQUIRE(max_lf <= 0.81f);
    REQUIRE_EQ(m.load_factor(), 0.0f);

    // Insert 6 elements (load factor = 6/8 = 0.75, under 0.8)
    for (int i = 0; i < 6; ++i) {
        m.insert(i, i * 10);
    }
    REQUIRE_EQ(m.size(), 6u);
    REQUIRE_EQ(m.bucket_count(), 8u);  // No rehash yet
    float lf = m.load_factor();
    REQUIRE(lf >= 0.74f);
    REQUIRE(lf <= 0.76f);

    // Change max load factor to 0.5 (should stay at current size)
    m.max_load_factor(0.5f);
    max_lf = m.max_load_factor();
    REQUIRE(max_lf >= 0.49f);
    REQUIRE(max_lf <= 0.51f);

    // Reserve for 20 elements
    // With max_load_factor 0.5, need ceil(20/0.5) = 40 buckets
    m.reserve(20);
    REQUIRE(m.bucket_count() >= 40u);
    REQUIRE_EQ(m.size(), 6u);  // Elements preserved

    // Verify all elements still accessible
    for (int i = 0; i < 6; ++i) {
        REQUIRE_EQ(m[i], i * 10);
    }

    // Load factor should be much lower now
    lf = m.load_factor();
    REQUIRE(lf < 0.2f);  // 6 elements in 40+ buckets
}

