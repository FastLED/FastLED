#include "test.h"
#include "fl/stl/flat_map.h"

FL_TEST_CASE("flat_map: basic construction") {
    fl::flat_map<int, int> m;
    FL_CHECK_TRUE(m.empty());
    FL_CHECK_EQ(m.size(), 0);
}

FL_TEST_CASE("flat_map: insert and find") {
    fl::flat_map<int, fl::string> m;

    auto result = m.insert({1, "one"});
    FL_CHECK_TRUE(result.second);
    FL_CHECK_EQ(result.first->first, 1);
    FL_CHECK_EQ(result.first->second, fl::string("one"));

    FL_CHECK_EQ(m.size(), 1);
    FL_CHECK_FALSE(m.empty());
}

FL_TEST_CASE("flat_map: insert duplicate") {
    fl::flat_map<int, int> m;

    auto result1 = m.insert({5, 10});
    FL_CHECK_TRUE(result1.second);

    auto result2 = m.insert({5, 20});
    FL_CHECK_FALSE(result2.second);
    FL_CHECK_EQ(result2.first->second, 10);  // Value unchanged
    FL_CHECK_EQ(m.size(), 1);
}

FL_TEST_CASE("flat_map: find existing key") {
    fl::flat_map<int, int> m;
    m.insert({1, 100});
    m.insert({2, 200});

    auto it = m.find(1);
    FL_CHECK_NE(it, m.end());
    FL_CHECK_EQ(it->second, 100);
}

FL_TEST_CASE("flat_map: find non-existent key") {
    fl::flat_map<int, int> m;
    m.insert({1, 100});

    auto it = m.find(999);
    FL_CHECK_EQ(it, m.end());
}

FL_TEST_CASE("flat_map: operator[]") {
    fl::flat_map<int, int> m;
    m[5] = 50;

    FL_CHECK_EQ(m[5], 50);
    FL_CHECK_EQ(m.size(), 1);
}

FL_TEST_CASE("flat_map: operator[] with new key") {
    fl::flat_map<int, int> m;
    int& ref = m[10];
    ref = 100;

    FL_CHECK_EQ(m[10], 100);
    FL_CHECK_EQ(m.size(), 1);
}

FL_TEST_CASE("flat_map: at() method") {
    fl::flat_map<int, int> m;
    m.insert({5, 50});

    FL_CHECK_EQ(m.at(5), 50);
}

FL_TEST_CASE("flat_map: multiple insertions maintain order") {
    fl::flat_map<int, int> m;
    m.insert({3, 30});
    m.insert({1, 10});
    m.insert({2, 20});
    m.insert({5, 50});
    m.insert({4, 40});

    FL_CHECK_EQ(m.size(), 5);

    // Check sorted order via iteration
    int expected_keys[] = {1, 2, 3, 4, 5};
    int i = 0;
    for (auto it = m.begin(); it != m.end(); ++it) {
        FL_CHECK_EQ(it->first, expected_keys[i]);
        FL_CHECK_EQ(it->second, it->first * 10);
        i++;
    }
}

FL_TEST_CASE("flat_map: erase by iterator") {
    fl::flat_map<int, int> m;
    m.insert({1, 10});
    m.insert({2, 20});
    m.insert({3, 30});

    auto it = m.find(2);
    m.erase(it);

    FL_CHECK_EQ(m.size(), 2);
    FL_CHECK_EQ(m.find(2), m.end());
    FL_CHECK_NE(m.find(1), m.end());
    FL_CHECK_NE(m.find(3), m.end());
}

FL_TEST_CASE("flat_map: erase by key") {
    fl::flat_map<int, int> m;
    m.insert({1, 10});
    m.insert({2, 20});
    m.insert({3, 30});

    int erase_count = m.erase(2);

    FL_CHECK_EQ(erase_count, 1);
    FL_CHECK_EQ(m.size(), 2);
    FL_CHECK_EQ(m.find(2), m.end());
}

FL_TEST_CASE("flat_map: erase non-existent key") {
    fl::flat_map<int, int> m;
    m.insert({1, 10});

    int erase_count = m.erase(999);
    FL_CHECK_EQ(erase_count, 0);
    FL_CHECK_EQ(m.size(), 1);
}

FL_TEST_CASE("flat_map: clear") {
    fl::flat_map<int, int> m;
    m.insert({1, 10});
    m.insert({2, 20});

    m.clear();

    FL_CHECK_TRUE(m.empty());
    FL_CHECK_EQ(m.size(), 0);
}

FL_TEST_CASE("flat_map: count") {
    fl::flat_map<int, int> m;
    m.insert({1, 10});

    FL_CHECK_EQ(m.count(1), 1);
    FL_CHECK_EQ(m.count(999), 0);
}

FL_TEST_CASE("flat_map: contains") {
    fl::flat_map<int, int> m;
    m.insert({1, 10});

    FL_CHECK_TRUE(m.contains(1));
    FL_CHECK_FALSE(m.contains(999));
}

FL_TEST_CASE("flat_map: lower_bound") {
    fl::flat_map<int, int> m;
    m.insert({1, 10});
    m.insert({3, 30});
    m.insert({5, 50});

    auto it = m.lower_bound(3);
    FL_CHECK_NE(it, m.end());
    FL_CHECK_EQ(it->first, 3);

    auto it2 = m.lower_bound(2);
    FL_CHECK_NE(it2, m.end());
    FL_CHECK_EQ(it2->first, 3);

    auto it3 = m.lower_bound(6);
    FL_CHECK_EQ(it3, m.end());
}

FL_TEST_CASE("flat_map: upper_bound") {
    fl::flat_map<int, int> m;
    m.insert({1, 10});
    m.insert({3, 30});
    m.insert({5, 50});

    auto it = m.upper_bound(3);
    FL_CHECK_NE(it, m.end());
    FL_CHECK_EQ(it->first, 5);

    auto it2 = m.upper_bound(5);
    FL_CHECK_EQ(it2, m.end());
}

FL_TEST_CASE("flat_map: equal_range") {
    fl::flat_map<int, int> m;
    m.insert({1, 10});
    m.insert({3, 30});
    m.insert({5, 50});

    auto range = m.equal_range(3);
    FL_CHECK_EQ(range.first->first, 3);
    FL_CHECK_EQ(range.second->first, 5);
}

FL_TEST_CASE("flat_map: reverse iteration") {
    fl::flat_map<int, int> m;
    m.insert({1, 10});
    m.insert({2, 20});
    m.insert({3, 30});

    int expected[] = {3, 2, 1};
    int i = 0;
    for (auto it = m.rbegin(); it != m.rend(); ++it) {
        FL_CHECK_EQ(it->first, expected[i]);
        i++;
    }
}

FL_TEST_CASE("flat_map: get with default") {
    fl::flat_map<int, int> m;
    m.insert({1, 100});

    FL_CHECK_EQ(m.get(1, -1), 100);
    FL_CHECK_EQ(m.get(999, -1), -1);
}

FL_TEST_CASE("flat_map: get with out parameter") {
    fl::flat_map<int, int> m;
    m.insert({1, 100});

    int value = -1;
    bool found = m.get(1, &value);
    FL_CHECK_TRUE(found);
    FL_CHECK_EQ(value, 100);

    found = m.get(999, &value);
    FL_CHECK_FALSE(found);
}

FL_TEST_CASE("flat_map: insert_or_update - insert new") {
    fl::flat_map<int, int> m;

    auto result = m.insert_or_update(1, 100);
    FL_CHECK_TRUE(result.second);
    FL_CHECK_EQ(result.first->second, 100);
    FL_CHECK_EQ(m.size(), 1);
}

FL_TEST_CASE("flat_map: insert_or_update - update existing") {
    fl::flat_map<int, int> m;
    m.insert({1, 100});

    auto result = m.insert_or_update(1, 200);
    FL_CHECK_FALSE(result.second);
    FL_CHECK_EQ(result.first->second, 200);
    FL_CHECK_EQ(m.size(), 1);
}

FL_TEST_CASE("flat_map: move semantics") {
    fl::flat_map<int, fl::string> m1;
    m1.insert({1, "one"});
    m1.insert({2, "two"});

    fl::flat_map<int, fl::string> m2 = fl::move(m1);

    FL_CHECK_EQ(m2.size(), 2);
    FL_CHECK_NE(m2.find(1), m2.end());
}

FL_TEST_CASE("flat_map: copy semantics") {
    fl::flat_map<int, int> m1;
    m1.insert({1, 10});
    m1.insert({2, 20});

    fl::flat_map<int, int> m2 = m1;

    FL_CHECK_EQ(m1.size(), 2);
    FL_CHECK_EQ(m2.size(), 2);
    FL_CHECK_EQ(m2[1], 10);
}

FL_TEST_CASE("flat_map: swap") {
    fl::flat_map<int, int> m1;
    m1.insert({1, 10});
    m1.insert({2, 20});

    fl::flat_map<int, int> m2;
    m2.insert({5, 50});

    m1.swap(m2);

    FL_CHECK_EQ(m1.size(), 1);
    FL_CHECK_EQ(m1[5], 50);
    FL_CHECK_EQ(m2.size(), 2);
    FL_CHECK_EQ(m2[1], 10);
}

FL_TEST_CASE("flat_map: equality comparison") {
    fl::flat_map<int, int> m1;
    m1.insert({1, 10});
    m1.insert({2, 20});

    fl::flat_map<int, int> m2;
    m2.insert({1, 10});
    m2.insert({2, 20});

    fl::flat_map<int, int> m3;
    m3.insert({1, 10});

    FL_CHECK_EQ(m1, m2);
    FL_CHECK_NE(m1, m3);
}

FL_TEST_CASE("flat_map: less-than comparison") {
    fl::flat_map<int, int> m1;
    m1.insert({1, 10});

    fl::flat_map<int, int> m2;
    m2.insert({2, 20});

    FL_CHECK_TRUE(m1 < m2);
    FL_CHECK_FALSE(m2 < m1);
}

FL_TEST_CASE("flat_map: emplace") {
    fl::flat_map<int, fl::string> m;

    auto result = m.emplace(1, "one");
    FL_CHECK_TRUE(result.second);
    FL_CHECK_EQ(result.first->second, fl::string("one"));
    FL_CHECK_EQ(m.size(), 1);
}

FL_TEST_CASE("flat_map: erase range") {
    fl::flat_map<int, int> m;
    m.insert({1, 10});
    m.insert({2, 20});
    m.insert({3, 30});
    m.insert({4, 40});

    auto it_start = m.find(2);
    auto it_end = m.find(4);
    m.erase(it_start, it_end);

    FL_CHECK_EQ(m.size(), 2);
    FL_CHECK_NE(m.find(1), m.end());
    FL_CHECK_EQ(m.find(2), m.end());
    FL_CHECK_EQ(m.find(3), m.end());
    FL_CHECK_NE(m.find(4), m.end());
}

FL_TEST_CASE("flat_map: reserve") {
    fl::flat_map<int, int> m;
    m.reserve(100);

    FL_CHECK_GE(m.capacity(), 100);
}

FL_TEST_CASE("flat_map: with string keys") {
    fl::flat_map<fl::string, int> m;
    m.insert({"apple", 1});
    m.insert({"banana", 2});
    m.insert({"cherry", 3});

    FL_CHECK_EQ(m.size(), 3);
    FL_CHECK_EQ(m.find("banana")->second, 2);

    // Check ordering is alphabetical
    auto it = m.begin();
    FL_CHECK_EQ(it->first, fl::string("apple"));
    ++it;
    FL_CHECK_EQ(it->first, fl::string("banana"));
    ++it;
    FL_CHECK_EQ(it->first, fl::string("cherry"));
}

FL_TEST_CASE("flat_map: large number of insertions") {
    fl::flat_map<int, int> m;

    // Insert 100 items
    for (int i = 0; i < 100; i++) {
        m.insert({i, i * 10});
    }

    FL_CHECK_EQ(m.size(), 100);

    // Verify all items are there
    for (int i = 0; i < 100; i++) {
        auto it = m.find(i);
        FL_CHECK_NE(it, m.end());
        FL_CHECK_EQ(it->second, i * 10);
    }
}

FL_TEST_CASE("flat_map: const_iterator") {
    fl::flat_map<int, int> m;
    m.insert({1, 10});
    m.insert({2, 20});

    const fl::flat_map<int, int>& cm = m;
    auto it = cm.find(1);
    FL_CHECK_NE(it, cm.end());
    FL_CHECK_EQ(it->second, 10);
}

FL_TEST_CASE("flat_map: cbegin/cend") {
    fl::flat_map<int, int> m;
    m.insert({1, 10});

    auto it = m.cbegin();
    FL_CHECK_NE(it, m.cend());
    FL_CHECK_EQ(it->first, 1);
}
