// test.cpp
// g++ --std=c++11 test.cpp -o test && ./test

#include <vector>
#include <set>
#include <unordered_map>

#include "fl/hash_map.h"
#include "fl/str.h"
#include "test.h"

using namespace fl;

TEST_CASE("Empty map properties") {
    HashMap<int, int> m;
    REQUIRE_EQ(m.size(), 0u);
    REQUIRE(!m.find_value(42));
    // begin()==end() on empty
    REQUIRE(m.begin() == m.end());
}

TEST_CASE("Single insert, lookup & operator[]") {
    HashMap<int, int> m;
    m.insert(10, 20);
    REQUIRE_EQ(m.size(), 1u);
    auto *v = m.find_value(10);
    REQUIRE(v);
    REQUIRE_EQ(*v, 20);

    // operator[] default-construct & assignment
    HashMap<int, Str> ms;
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
    HashMap<int, Str> m;
    m.insert(1, "foo");
    REQUIRE_EQ(m.size(), 1u);
    REQUIRE_EQ(*m.find_value(1), "foo");

    m.insert(1, "bar");
    REQUIRE_EQ(m.size(), 1u);
    REQUIRE_EQ(*m.find_value(1), "bar");
}

TEST_CASE("Multiple distinct inserts & lookups") {
    HashMap<char, int> m;
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
    HashMap<int, int> m;
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
    HashMap<int, int> m(4);
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
    HashMap<int, int> m(4);
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
    HashMap<int, int> m(1 /*capacity*/);
    const int N = 100;
    for (int i = 0; i < N; ++i) {
        m.insert(i, i * 3);
        // test that size is increasing
        REQUIRE_EQ(m.size(), static_cast<std::size_t>(i + 1));
    }
    REQUIRE_EQ(m.size(), static_cast<std::size_t>(N));
    for (int i = 0; i < N; ++i) {
        auto *v = m.find_value(i);
        REQUIRE(v);
        REQUIRE_EQ(*v, i * 3);
    }
}

TEST_CASE("Iterator round-trip and const-iteration") {
    HashMap<int, int> m;
    for (int i = 0; i < 20; ++i) {
        m.insert(i, i + 100);
    }

    // non-const iteration
    std::size_t count = 0;
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
    HashMap<int, int> m;
    REQUIRE(!m.remove(999));

    const HashMap<int, int> cm;
    REQUIRE(!cm.find_value(0));
}

TEST_CASE("Inserting multiple elements while deleting them will trigger inline "
          "rehash") {
    const static int MAX_CAPACITY = 2;
    HashMap<int, int> m(8 /*capacity*/);
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
    std::set<int> found_values;

    for (auto it = m.begin(); it != m.end(); ++it) {
        auto kv = *it;
        auto key = kv.first;
        auto value = kv.second;
        REQUIRE_EQ(key, value);
        found_values.insert(kv.second);
    }

    std::vector<int> found_values_vec(found_values.begin(), found_values.end());
    REQUIRE_EQ(MAX_CAPACITY, found_values_vec.size());
    for (int i = 0; i < MAX_CAPACITY; ++i) {
        auto value = found_values_vec[i];
        REQUIRE_EQ(value, i);
    }
}

TEST_CASE("HashMap with standard iterator access") {
    HashMap<int, int> m;
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

