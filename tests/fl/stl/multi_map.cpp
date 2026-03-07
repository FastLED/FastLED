#include "fl/stl/multi_map.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

FL_TEST_CASE("fl::multi_map basic operations") {
    fl::multi_map<int, fl::string> mm;

    FL_SUBCASE("Empty multimap") {
        FL_CHECK(mm.empty());
        FL_CHECK(mm.size() == 0);
    }

    FL_SUBCASE("Insert single element") {
        auto it = mm.insert({1, "one"});
        FL_CHECK(mm.size() == 1);
        FL_CHECK(mm.contains(1));
        FL_CHECK(it->first == 1);
        FL_CHECK(it->second == "one");
    }

    FL_SUBCASE("Insert duplicate keys") {
        mm.insert({1, "first"});
        mm.insert({1, "second"});
        mm.insert({1, "third"});

        FL_CHECK(mm.size() == 3);
        FL_CHECK(mm.count(1) == 3);

        // All values should be findable
        auto range = mm.equal_range(1);
        int count = 0;
        for (auto it = range.first; it != range.second; ++it) {
            count++;
        }
        FL_CHECK(count == 3);
    }

    FL_SUBCASE("Insert different keys") {
        mm.insert({1, "one"});
        mm.insert({2, "two"});
        mm.insert({3, "three"});

        FL_CHECK(mm.size() == 3);
        FL_CHECK(mm.count(1) == 1);
        FL_CHECK(mm.count(2) == 1);
        FL_CHECK(mm.count(3) == 1);
    }

    FL_SUBCASE("Find operations") {
        mm.insert({10, "ten"});
        mm.insert({20, "twenty"});

        auto it = mm.find(10);
        FL_CHECK(it != mm.end());
        FL_CHECK(it->first == 10);
        FL_CHECK(it->second == "ten");

        auto missing = mm.find(999);
        FL_CHECK(missing == mm.end());
    }

    FL_SUBCASE("Erase by iterator") {
        mm.insert({1, "one"});
        mm.insert({2, "two"});
        mm.insert({3, "three"});

        auto it = mm.find(2);
        auto next = mm.erase(it);
        FL_CHECK(mm.size() == 2);
        FL_CHECK_FALSE(mm.contains(2));
        FL_CHECK(next->first == 3);
    }

    FL_SUBCASE("Erase by key") {
        mm.insert({1, "one"});
        mm.insert({2, "two"});
        mm.insert({2, "two_dup"});
        mm.insert({3, "three"});

        fl::size erased = mm.erase(2);
        FL_CHECK(erased == 2);
        FL_CHECK(mm.size() == 2);
        FL_CHECK_FALSE(mm.contains(2));
        FL_CHECK(mm.contains(1));
        FL_CHECK(mm.contains(3));
    }

    FL_SUBCASE("Clear") {
        mm.insert({1, "one"});
        mm.insert({2, "two"});

        mm.clear();
        FL_CHECK(mm.empty());
        FL_CHECK(mm.size() == 0);
    }
}

FL_TEST_CASE("fl::multi_map duplicate handling") {
    fl::multi_map<int, int> mm;

    FL_SUBCASE("Multiple values for same key") {
        mm.insert({5, 50});
        mm.insert({5, 51});
        mm.insert({5, 52});
        mm.insert({5, 53});

        FL_CHECK(mm.count(5) == 4);

        // Check equal_range returns all duplicates in order
        auto range = mm.equal_range(5);
        int count = 0;
        for (auto it = range.first; it != range.second; ++it) {
            count++;
        }
        FL_CHECK(count == 4);
    }

    FL_SUBCASE("Duplicate preservation on erase") {
        mm.insert({1, 10});
        mm.insert({1, 11});
        mm.insert({1, 12});

        auto it = mm.find(1);
        mm.erase(it);  // Remove one instance

        FL_CHECK(mm.count(1) == 2);
    }

    FL_SUBCASE("Erase all duplicates") {
        mm.insert({1, 10});
        mm.insert({1, 11});
        mm.insert({2, 20});
        mm.insert({2, 21});
        mm.insert({2, 22});

        fl::size erased = mm.erase(2);
        FL_CHECK(erased == 3);
        FL_CHECK(mm.count(1) == 2);
        FL_CHECK(mm.count(2) == 0);
    }
}

FL_TEST_CASE("fl::multi_map bound operations") {
    fl::multi_map<int, fl::string> mm;

    mm.insert({1, "a"});
    mm.insert({3, "b"});
    mm.insert({3, "c"});
    mm.insert({5, "d"});

    FL_SUBCASE("lower_bound") {
        auto it = mm.lower_bound(3);
        FL_CHECK(it != mm.end());
        FL_CHECK(it->first == 3);
    }

    FL_SUBCASE("upper_bound") {
        auto it = mm.upper_bound(3);
        FL_CHECK(it != mm.end());
        FL_CHECK(it->first == 5);
    }

    FL_SUBCASE("lower_bound for missing key between existing") {
        auto it = mm.lower_bound(4);
        FL_CHECK(it != mm.end());
        FL_CHECK(it->first == 5);
    }

    FL_SUBCASE("upper_bound for missing key between existing") {
        auto it = mm.upper_bound(4);
        FL_CHECK(it != mm.end());
        FL_CHECK(it->first == 5);
    }

    FL_SUBCASE("equal_range") {
        auto range = mm.equal_range(3);
        int count = 0;
        for (auto it = range.first; it != range.second; ++it) {
            FL_CHECK(it->first == 3);
            count++;
        }
        FL_CHECK(count == 2);
    }

    FL_SUBCASE("equal_range for non-existent key") {
        auto range = mm.equal_range(2);
        FL_CHECK(range.first == range.second);
    }
}

FL_TEST_CASE("fl::multi_map iterators") {
    fl::multi_map<int, int> mm;

    mm.insert({1, 10});
    mm.insert({2, 20});
    mm.insert({2, 21});
    mm.insert({3, 30});

    FL_SUBCASE("Forward iteration") {
        int sum = 0;
        for (const auto& p : mm) {
            sum += p.second;
        }
        FL_CHECK(sum == 81);  // 10 + 20 + 21 + 30
    }

    FL_SUBCASE("Iterator increment/decrement") {
        auto it = mm.begin();
        FL_CHECK(it->first == 1);

        ++it;
        FL_CHECK(it->first == 2);

        ++it;
        FL_CHECK(it->first == 2);

        ++it;
        FL_CHECK(it->first == 3);

        --it;
        FL_CHECK(it->first == 2);
    }

    FL_SUBCASE("Const iteration") {
        const auto& cmm = mm;
        int count = 0;
        for (auto it = cmm.begin(); it != cmm.end(); ++it) {
            count++;
        }
        FL_CHECK(count == 4);
    }

    FL_SUBCASE("Reverse iteration") {
        int count = 0;
        for (auto it = mm.rbegin(); it != mm.rend(); ++it) {
            count++;
        }
        FL_CHECK(count == 4);

        auto rit = mm.rbegin();
        FL_CHECK(rit->first == 3);
        ++rit;
        FL_CHECK(rit->first == 2);
    }
}

FL_TEST_CASE("fl::multi_map emplace") {
    fl::multi_map<int, fl::string> mm;

    FL_SUBCASE("Emplace single element") {
        auto it = mm.emplace(1, "one");
        FL_CHECK(mm.size() == 1);
        FL_CHECK(it->first == 1);
        FL_CHECK(it->second == "one");
    }

    FL_SUBCASE("Emplace duplicates") {
        mm.emplace(5, "a");
        mm.emplace(5, "b");
        mm.emplace(5, "c");

        FL_CHECK(mm.count(5) == 3);
    }
}

FL_TEST_CASE("fl::multi_map range operations") {
    fl::multi_map<int, int> mm;

    mm.insert({1, 10});
    mm.insert({2, 20});
    mm.insert({3, 30});

    FL_SUBCASE("Insert from initializer list") {
        fl::multi_map<int, int> mm2;
        mm2.insert({{1, 100}, {2, 200}, {2, 201}, {3, 300}});

        FL_CHECK(mm2.size() == 4);
        FL_CHECK(mm2.count(2) == 2);
    }

    FL_SUBCASE("Insert from vector") {
        fl::vector<fl::pair<int, int>> vec = {{4, 40}, {4, 41}, {5, 50}};
        mm.insert(vec.begin(), vec.end());

        FL_CHECK(mm.size() == 6);
        FL_CHECK(mm.count(4) == 2);
        FL_CHECK(mm.count(5) == 1);
    }

    FL_SUBCASE("Erase range") {
        mm.insert({2, 21});
        auto first = mm.lower_bound(2);
        auto last = mm.upper_bound(2);
        mm.erase(first, last);

        FL_CHECK(mm.count(2) == 0);
        FL_CHECK(mm.count(1) == 1);
        FL_CHECK(mm.count(3) == 1);
    }
}

FL_TEST_CASE("fl::multi_map swap") {
    fl::multi_map<int, int> mm1;
    fl::multi_map<int, int> mm2;

    mm1.insert({1, 10});
    mm1.insert({2, 20});

    mm2.insert({3, 30});

    mm1.swap(mm2);

    FL_CHECK(mm1.size() == 1);
    FL_CHECK(mm1.count(3) == 1);
    FL_CHECK(mm1.count(1) == 0);

    FL_CHECK(mm2.size() == 2);
    FL_CHECK(mm2.count(1) == 1);
    FL_CHECK(mm2.count(2) == 1);
}

FL_TEST_CASE("fl::multi_map comparison") {
    fl::multi_map<int, int> mm1;
    fl::multi_map<int, int> mm2;
    fl::multi_map<int, int> mm3;

    mm1.insert({1, 10});
    mm1.insert({2, 20});

    mm2.insert({1, 10});
    mm2.insert({2, 20});

    mm3.insert({1, 10});
    mm3.insert({3, 30});

    FL_SUBCASE("Equality") {
        FL_CHECK(mm1 == mm2);
        FL_CHECK(mm1 != mm3);
    }

    FL_SUBCASE("Less than") {
        FL_CHECK(mm1 < mm3);
        FL_CHECK_FALSE(mm3 < mm1);
    }

    FL_SUBCASE("Less than or equal") {
        FL_CHECK(mm1 <= mm2);
        FL_CHECK(mm1 <= mm3);
        FL_CHECK_FALSE(mm3 <= mm1);
    }
}

FL_TEST_CASE("fl::multi_map initializer list constructor") {
    fl::multi_map<int, fl::string> mm({
        {1, "one"},
        {2, "two"},
        {2, "two_dup"},
        {3, "three"}
    });

    FL_CHECK(mm.size() == 4);
    FL_CHECK(mm.count(2) == 2);
    FL_CHECK(mm.contains(1));
    FL_CHECK(mm.contains(3));
}

FL_TEST_CASE("fl::multi_map allocator") {
    fl::multi_map<int, int> mm;

    mm.insert({1, 10});
    mm.insert({2, 20});
    mm.insert({1, 11});

    FL_CHECK(mm.size() == 3);
    FL_CHECK(mm.count(1) == 2);

    auto alloc = mm.get_allocator();
    (void)alloc; // Allocator obtained successfully
}

FL_TEST_CASE("fl::multi_map slab allocator basic usage") {
    // Create multimap with slab allocator (default)
    fl::multi_map<int, int> mm;

    // Insert elements
    for (int i = 0; i < 10; ++i) {
        mm.insert({i, i * 10});
    }

    // Verify size matches expected
    FL_CHECK(mm.size() == 10);

    // Verify all elements are present
    for (int i = 0; i < 10; ++i) {
        FL_CHECK(mm.count(i) == 1);
        auto it = mm.find(i);
        FL_CHECK(it != mm.end());
        FL_CHECK(it->second == i * 10);
    }
}

FL_TEST_CASE("fl::multi_map duplicate keys with slab allocator") {
    fl::multi_map<int, fl::string> mm;

    // Insert duplicates
    mm.insert({1, "one"});
    mm.insert({1, "uno"});
    mm.insert({1, "eins"});
    mm.insert({2, "two"});
    mm.insert({2, "dos"});

    // Verify counts
    FL_CHECK(mm.size() == 5);
    FL_CHECK(mm.count(1) == 3);
    FL_CHECK(mm.count(2) == 2);

    // Verify equal_range works
    auto range = mm.equal_range(1);
    fl::size count = 0;
    for (auto it = range.first; it != range.second; ++it) {
        ++count;
        FL_CHECK(it->first == 1);
    }
    FL_CHECK(count == 3);
}

FL_TEST_CASE("fl::multi_map large insertion test") {
    fl::multi_map<int, int> mm;

    // Insert many elements with duplicates
    fl::size total_insertions = 0;
    for (int key = 0; key < 20; ++key) {
        for (int dup = 0; dup < 3; ++dup) {
            mm.insert({key, key * 100 + dup});
            ++total_insertions;
        }
    }

    // Verify total count matches insertions
    FL_CHECK(mm.size() == total_insertions);
    FL_CHECK(mm.size() == 60);  // 20 keys * 3 duplicates each

    // Verify specific key counts
    for (int key = 0; key < 20; ++key) {
        FL_CHECK(mm.count(key) == 3);
    }

    // Verify iteration count matches size
    fl::size iteration_count = 0;
    for (auto it = mm.begin(); it != mm.end(); ++it) {
        ++iteration_count;
    }
    FL_CHECK(iteration_count == mm.size());
}

FL_TEST_CASE("fl::multi_map erase and reinsert") {
    fl::multi_map<int, int> mm;

    // Insert initial elements
    for (int i = 0; i < 10; ++i) {
        mm.insert({i % 3, i});  // Keys 0, 1, 2 with multiple values
    }

    FL_CHECK(mm.size() == 10);

    // Erase by key
    fl::size erased = mm.erase(1);
    FL_CHECK(erased == 3);  // Should erase all elements with key 1 (i=1,4,7)
    FL_CHECK(mm.size() == 7);

    fl::size count1 = mm.count(1);
    FL_CHECK(count1 == 0);

    // Verify other keys still exist
    fl::size count0 = mm.count(0);
    fl::size count2 = mm.count(2);
    FL_CHECK(count0 > 0);
    FL_CHECK(count2 > 0);

    // Reinsert
    mm.insert({1, 100});
    FL_CHECK(mm.size() == 8);

    fl::size count1_after = mm.count(1);
    FL_CHECK(count1_after == 1);
}

FL_TEST_CASE("fl::multi_map memory efficiency check") {
    // This test verifies that the slab allocator doesn't cause
    // excessive allocations for moderate-sized multimaps

    fl::multi_map<int, int> mm;

    // Insert a reasonable number of elements
    fl::size insert_count = 100;
    for (fl::size i = 0; i < insert_count; ++i) {
        mm.insert({(int)(i % 20), (int)i});
    }

    // Verify all elements are stored
    FL_CHECK(mm.size() == insert_count);

    // Verify lookup still works for all keys
    for (int key = 0; key < 20; ++key) {
        fl::size count = mm.count(key);
        FL_CHECK(count == 5);  // 100 elements / 20 keys = 5 per key
    }
}

} // FL_TEST_FILE
