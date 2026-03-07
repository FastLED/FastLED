#include "test.h"
#include "fl/stl/flat_set.h"

FL_TEST_CASE("flat_set: basic construction") {
    fl::flat_set<int> s;
    FL_CHECK_TRUE(s.empty());
    FL_CHECK_EQ(s.size(), 0);
}

FL_TEST_CASE("flat_set: insert and find") {
    fl::flat_set<int> s;

    auto result = s.insert(1);
    FL_CHECK_TRUE(result.second);
    FL_CHECK_EQ(*result.first, 1);

    FL_CHECK_EQ(s.size(), 1);
    FL_CHECK_FALSE(s.empty());
}

FL_TEST_CASE("flat_set: insert duplicate") {
    fl::flat_set<int> s;

    auto result1 = s.insert(5);
    FL_CHECK_TRUE(result1.second);

    auto result2 = s.insert(5);
    FL_CHECK_FALSE(result2.second);
    FL_CHECK_EQ(*result2.first, 5);
    FL_CHECK_EQ(s.size(), 1);
}

FL_TEST_CASE("flat_set: find existing key") {
    fl::flat_set<int> s;
    s.insert(1);
    s.insert(2);

    auto it = s.find(1);
    FL_CHECK_NE(it, s.end());
    FL_CHECK_EQ(*it, 1);
}

FL_TEST_CASE("flat_set: find non-existent key") {
    fl::flat_set<int> s;
    s.insert(1);

    auto it = s.find(999);
    FL_CHECK_EQ(it, s.end());
}

FL_TEST_CASE("flat_set: multiple insertions maintain order") {
    fl::flat_set<int> s;
    s.insert(3);
    s.insert(1);
    s.insert(2);
    s.insert(5);
    s.insert(4);

    FL_CHECK_EQ(s.size(), 5);

    // Check sorted order via iteration
    int expected_keys[] = {1, 2, 3, 4, 5};
    int i = 0;
    for (auto it = s.begin(); it != s.end(); ++it) {
        FL_CHECK_EQ(*it, expected_keys[i]);
        i++;
    }
}

FL_TEST_CASE("flat_set: erase by iterator") {
    fl::flat_set<int> s;
    s.insert(1);
    s.insert(2);
    s.insert(3);

    auto it = s.find(2);
    s.erase(it);

    FL_CHECK_EQ(s.size(), 2);
    FL_CHECK_EQ(s.find(2), s.end());
    FL_CHECK_NE(s.find(1), s.end());
    FL_CHECK_NE(s.find(3), s.end());
}

FL_TEST_CASE("flat_set: erase by key") {
    fl::flat_set<int> s;
    s.insert(1);
    s.insert(2);
    s.insert(3);

    int erase_count = s.erase(2);

    FL_CHECK_EQ(erase_count, 1);
    FL_CHECK_EQ(s.size(), 2);
    FL_CHECK_EQ(s.find(2), s.end());
}

FL_TEST_CASE("flat_set: erase non-existent key") {
    fl::flat_set<int> s;
    s.insert(1);

    int erase_count = s.erase(999);
    FL_CHECK_EQ(erase_count, 0);
    FL_CHECK_EQ(s.size(), 1);
}

FL_TEST_CASE("flat_set: clear") {
    fl::flat_set<int> s;
    s.insert(1);
    s.insert(2);

    s.clear();

    FL_CHECK_TRUE(s.empty());
    FL_CHECK_EQ(s.size(), 0);
}

FL_TEST_CASE("flat_set: count") {
    fl::flat_set<int> s;
    s.insert(1);

    FL_CHECK_EQ(s.count(1), 1);
    FL_CHECK_EQ(s.count(999), 0);
}

FL_TEST_CASE("flat_set: contains") {
    fl::flat_set<int> s;
    s.insert(1);

    FL_CHECK_TRUE(s.contains(1));
    FL_CHECK_FALSE(s.contains(999));
}

FL_TEST_CASE("flat_set: lower_bound") {
    fl::flat_set<int> s;
    s.insert(1);
    s.insert(3);
    s.insert(5);

    auto it = s.lower_bound(3);
    FL_CHECK_NE(it, s.end());
    FL_CHECK_EQ(*it, 3);

    auto it2 = s.lower_bound(2);
    FL_CHECK_NE(it2, s.end());
    FL_CHECK_EQ(*it2, 3);

    auto it3 = s.lower_bound(6);
    FL_CHECK_EQ(it3, s.end());
}

FL_TEST_CASE("flat_set: upper_bound") {
    fl::flat_set<int> s;
    s.insert(1);
    s.insert(3);
    s.insert(5);

    auto it = s.upper_bound(3);
    FL_CHECK_NE(it, s.end());
    FL_CHECK_EQ(*it, 5);

    auto it2 = s.upper_bound(5);
    FL_CHECK_EQ(it2, s.end());
}

FL_TEST_CASE("flat_set: equal_range") {
    fl::flat_set<int> s;
    s.insert(1);
    s.insert(3);
    s.insert(5);

    auto range = s.equal_range(3);
    FL_CHECK_EQ(*range.first, 3);
    FL_CHECK_EQ(*range.second, 5);
}

FL_TEST_CASE("flat_set: reverse iteration") {
    fl::flat_set<int> s;
    s.insert(1);
    s.insert(2);
    s.insert(3);

    int expected[] = {3, 2, 1};
    int i = 0;
    for (auto it = s.rbegin(); it != s.rend(); ++it) {
        FL_CHECK_EQ(*it, expected[i]);
        i++;
    }
}

FL_TEST_CASE("flat_set: move semantics") {
    fl::flat_set<int> s1;
    s1.insert(1);
    s1.insert(2);

    fl::flat_set<int> s2 = fl::move(s1);

    FL_CHECK_EQ(s2.size(), 2);
    FL_CHECK_NE(s2.find(1), s2.end());
}

FL_TEST_CASE("flat_set: copy semantics") {
    fl::flat_set<int> s1;
    s1.insert(1);
    s1.insert(2);

    fl::flat_set<int> s2 = s1;

    FL_CHECK_EQ(s1.size(), 2);
    FL_CHECK_EQ(s2.size(), 2);
    FL_CHECK_NE(s2.find(1), s2.end());
}

FL_TEST_CASE("flat_set: swap") {
    fl::flat_set<int> s1;
    s1.insert(1);
    s1.insert(2);

    fl::flat_set<int> s2;
    s2.insert(5);

    s1.swap(s2);

    FL_CHECK_EQ(s1.size(), 1);
    FL_CHECK_EQ(*s1.find(5), 5);
    FL_CHECK_EQ(s2.size(), 2);
    FL_CHECK_NE(s2.find(1), s2.end());
}

FL_TEST_CASE("flat_set: equality comparison") {
    fl::flat_set<int> s1;
    s1.insert(1);
    s1.insert(2);

    fl::flat_set<int> s2;
    s2.insert(1);
    s2.insert(2);

    fl::flat_set<int> s3;
    s3.insert(1);

    FL_CHECK_EQ(s1, s2);
    FL_CHECK_NE(s1, s3);
}

FL_TEST_CASE("flat_set: less-than comparison") {
    fl::flat_set<int> s1;
    s1.insert(1);

    fl::flat_set<int> s2;
    s2.insert(2);

    FL_CHECK_TRUE(s1 < s2);
    FL_CHECK_FALSE(s2 < s1);
}

FL_TEST_CASE("flat_set: emplace") {
    fl::flat_set<int> s;

    auto result = s.emplace(1);
    FL_CHECK_TRUE(result.second);
    FL_CHECK_EQ(*result.first, 1);
    FL_CHECK_EQ(s.size(), 1);
}

FL_TEST_CASE("flat_set: erase range") {
    fl::flat_set<int> s;
    s.insert(1);
    s.insert(2);
    s.insert(3);
    s.insert(4);

    auto it_start = s.find(2);
    auto it_end = s.find(4);
    s.erase(it_start, it_end);

    FL_CHECK_EQ(s.size(), 2);
    FL_CHECK_NE(s.find(1), s.end());
    FL_CHECK_EQ(s.find(2), s.end());
    FL_CHECK_EQ(s.find(3), s.end());
    FL_CHECK_NE(s.find(4), s.end());
}

FL_TEST_CASE("flat_set: reserve") {
    fl::flat_set<int> s;
    s.reserve(100);

    FL_CHECK_GE(s.capacity(), 100);
}

FL_TEST_CASE("flat_set: with string elements") {
    fl::flat_set<fl::string> s;
    s.insert("apple");
    s.insert("banana");
    s.insert("cherry");

    FL_CHECK_EQ(s.size(), 3);
    FL_CHECK_NE(s.find("banana"), s.end());

    // Check ordering is alphabetical
    auto it = s.begin();
    FL_CHECK_EQ(*it, fl::string("apple"));
    ++it;
    FL_CHECK_EQ(*it, fl::string("banana"));
    ++it;
    FL_CHECK_EQ(*it, fl::string("cherry"));
}

FL_TEST_CASE("flat_set: large number of insertions") {
    fl::flat_set<int> s;

    // Insert 100 items
    for (int i = 0; i < 100; i++) {
        s.insert(i);
    }

    FL_CHECK_EQ(s.size(), 100);

    // Verify all items are there
    for (int i = 0; i < 100; i++) {
        auto it = s.find(i);
        FL_CHECK_NE(it, s.end());
        FL_CHECK_EQ(*it, i);
    }
}

FL_TEST_CASE("flat_set: const_iterator") {
    fl::flat_set<int> s;
    s.insert(1);
    s.insert(2);

    const fl::flat_set<int>& cs = s;
    auto it = cs.find(1);
    FL_CHECK_NE(it, cs.end());
    FL_CHECK_EQ(*it, 1);
}

FL_TEST_CASE("flat_set: cbegin/cend") {
    fl::flat_set<int> s;
    s.insert(1);

    auto it = s.cbegin();
    FL_CHECK_NE(it, s.cend());
    FL_CHECK_EQ(*it, 1);
}

FL_TEST_CASE("flat_set: key_comp") {
    fl::flat_set<int> s;
    s.insert(1);
    s.insert(5);

    auto comp = s.key_comp();
    FL_CHECK_TRUE(comp(1, 5));
    FL_CHECK_FALSE(comp(5, 1));
}

FL_TEST_CASE("flat_set: value_comp") {
    fl::flat_set<int> s;
    s.insert(1);
    s.insert(5);

    auto comp = s.value_comp();
    FL_CHECK_TRUE(comp(1, 5));
    FL_CHECK_FALSE(comp(5, 1));
}

FL_TEST_CASE("flat_set: custom comparator") {
    fl::flat_set<int, fl::greater<int>> s;
    s.insert(3);
    s.insert(1);
    s.insert(5);

    // Elements should be sorted in descending order
    int expected[] = {5, 3, 1};
    int i = 0;
    for (auto it = s.begin(); it != s.end(); ++it) {
        FL_CHECK_EQ(*it, expected[i]);
        i++;
    }
}

FL_TEST_CASE("flat_set: allocator") {
    fl::allocator<int> alloc;
    fl::flat_set<int, fl::less<int>, fl::allocator<int>> s(alloc);

    // Allocator is obtainable (we can't compare allocators, just verify it's callable)
    auto result_alloc = s.get_allocator();
    s.reserve(10);  // Use allocator via reserve
    FL_CHECK_TRUE(true);
}
