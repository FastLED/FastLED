#include "fl/stl/multi_set.h"
#include "fl/stl/vector.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

FL_TEST_CASE("fl::multi_set basic operations") {
    fl::multi_set<int> ms;

    FL_SUBCASE("Empty multiset") {
        FL_CHECK(ms.empty());
        FL_CHECK(ms.size() == 0);
    }

    FL_SUBCASE("Insert single element") {
        auto it = ms.insert(42);
        FL_CHECK(ms.size() == 1);
        FL_CHECK(ms.contains(42));
        FL_CHECK(*it == 42);
    }

    FL_SUBCASE("Insert duplicate keys") {
        ms.insert(5);
        ms.insert(5);
        ms.insert(5);

        FL_CHECK(ms.size() == 3);
        FL_CHECK(ms.count(5) == 3);

        // All values should be findable
        auto range = ms.equal_range(5);
        int count = 0;
        for (auto it = range.first; it != range.second; ++it) {
            count++;
        }
        FL_CHECK(count == 3);
    }

    FL_SUBCASE("Insert different keys") {
        ms.insert(1);
        ms.insert(2);
        ms.insert(3);

        FL_CHECK(ms.size() == 3);
        FL_CHECK(ms.count(1) == 1);
        FL_CHECK(ms.count(2) == 1);
        FL_CHECK(ms.count(3) == 1);
    }

    FL_SUBCASE("Find operations") {
        ms.insert(10);
        ms.insert(20);

        auto it = ms.find(10);
        FL_CHECK(it != ms.end());
        FL_CHECK(*it == 10);

        auto missing = ms.find(999);
        FL_CHECK(missing == ms.end());
    }

    FL_SUBCASE("Erase by iterator") {
        ms.insert(1);
        ms.insert(2);
        ms.insert(3);

        auto it = ms.find(2);
        auto next = ms.erase(it);
        FL_CHECK(ms.size() == 2);
        FL_CHECK_FALSE(ms.contains(2));
        FL_CHECK(*next == 3);
    }

    FL_SUBCASE("Erase by key") {
        ms.insert(1);
        ms.insert(2);
        ms.insert(2);
        ms.insert(3);

        fl::size erased = ms.erase(2);
        FL_CHECK(erased == 2);
        FL_CHECK(ms.size() == 2);
        FL_CHECK_FALSE(ms.contains(2));
        FL_CHECK(ms.contains(1));
        FL_CHECK(ms.contains(3));
    }

    FL_SUBCASE("Clear") {
        ms.insert(1);
        ms.insert(2);

        ms.clear();
        FL_CHECK(ms.empty());
        FL_CHECK(ms.size() == 0);
    }
}

FL_TEST_CASE("fl::multi_set duplicate handling") {
    fl::multi_set<int> ms;

    FL_SUBCASE("Multiple duplicate values") {
        ms.insert(5);
        ms.insert(5);
        ms.insert(5);
        ms.insert(5);

        FL_CHECK(ms.count(5) == 4);

        // Check equal_range returns all duplicates in order
        auto range = ms.equal_range(5);
        int count = 0;
        for (auto it = range.first; it != range.second; ++it) {
            count++;
        }
        FL_CHECK(count == 4);
    }

    FL_SUBCASE("Duplicate preservation on erase") {
        ms.insert(1);
        ms.insert(1);
        ms.insert(1);

        auto it = ms.find(1);
        ms.erase(it);  // Remove one instance

        FL_CHECK(ms.count(1) == 2);
    }

    FL_SUBCASE("Erase all duplicates") {
        ms.insert(1);
        ms.insert(1);
        ms.insert(2);
        ms.insert(2);
        ms.insert(2);

        fl::size erased = ms.erase(2);
        FL_CHECK(erased == 3);
        FL_CHECK(ms.count(1) == 2);
        FL_CHECK(ms.count(2) == 0);
    }
}

FL_TEST_CASE("fl::multi_set bound operations") {
    fl::multi_set<int> ms;

    FL_SUBCASE("lower_bound and upper_bound with duplicates") {
        ms.insert(1);
        ms.insert(3);
        ms.insert(3);
        ms.insert(3);
        ms.insert(5);

        auto lower = ms.lower_bound(3);
        auto upper = ms.upper_bound(3);

        FL_CHECK(*lower == 3);
        FL_CHECK(*upper == 5);

        // Count elements in range
        int count = 0;
        for (auto it = lower; it != upper; ++it) {
            count++;
        }
        FL_CHECK(count == 3);
    }

    FL_SUBCASE("lower_bound on non-existent key") {
        ms.insert(1);
        ms.insert(5);

        auto it = ms.lower_bound(3);
        FL_CHECK(*it == 5);
    }

    FL_SUBCASE("upper_bound on non-existent key") {
        ms.insert(1);
        ms.insert(5);

        auto it = ms.upper_bound(3);
        FL_CHECK(*it == 5);
    }

    FL_SUBCASE("upper_bound on key at end") {
        ms.insert(1);
        ms.insert(5);

        auto it = ms.upper_bound(5);
        FL_CHECK(it == ms.end());
    }
}

FL_TEST_CASE("fl::multi_set range operations") {
    fl::multi_set<int> ms;

    FL_SUBCASE("Range insert from initializer list") {
        ms.insert({10, 20, 30, 20, 10});
        FL_CHECK(ms.size() == 5);
        FL_CHECK(ms.count(10) == 2);
        FL_CHECK(ms.count(20) == 2);
        FL_CHECK(ms.count(30) == 1);
    }

    FL_SUBCASE("Range erase") {
        ms.insert(1);
        ms.insert(2);
        ms.insert(3);
        ms.insert(4);
        ms.insert(5);

        auto first = ms.find(2);
        auto last = ms.find(5);

        ms.erase(first, last);

        FL_CHECK(ms.size() == 2);
        FL_CHECK(ms.contains(1));
        FL_CHECK(ms.contains(5));
        FL_CHECK_FALSE(ms.contains(2));
        FL_CHECK_FALSE(ms.contains(3));
        FL_CHECK_FALSE(ms.contains(4));
    }

    FL_SUBCASE("Range erase with duplicates") {
        ms.insert(1);
        ms.insert(2);
        ms.insert(2);
        ms.insert(2);
        ms.insert(3);

        auto range = ms.equal_range(2);
        ms.erase(range.first, range.second);

        FL_CHECK(ms.size() == 2);
        FL_CHECK(ms.count(2) == 0);
    }
}

FL_TEST_CASE("fl::multi_set iterators") {
    fl::multi_set<int> ms;
    ms.insert(5);
    ms.insert(3);
    ms.insert(3);
    ms.insert(7);
    ms.insert(1);

    FL_SUBCASE("Forward iteration order") {
        fl::vector<int> values;
        for (auto it = ms.begin(); it != ms.end(); ++it) {
            values.push_back(*it);
        }

        // Should be in sorted order
        FL_CHECK(values.size() == 5);
        FL_CHECK(values[0] == 1);
        FL_CHECK(values[1] == 3);
        FL_CHECK(values[2] == 3);
        FL_CHECK(values[3] == 5);
        FL_CHECK(values[4] == 7);
    }

    FL_SUBCASE("Reverse iteration order") {
        fl::vector<int> values;
        for (auto it = ms.rbegin(); it != ms.rend(); ++it) {
            values.push_back(*it);
        }

        // Should be in reverse sorted order
        FL_CHECK(values.size() == 5);
        FL_CHECK(values[0] == 7);
        FL_CHECK(values[1] == 5);
        FL_CHECK(values[2] == 3);
        FL_CHECK(values[3] == 3);
        FL_CHECK(values[4] == 1);
    }

    FL_SUBCASE("Iterator post-increment") {
        auto it = ms.begin();
        auto next = it++;
        FL_CHECK(*next == 1);
        FL_CHECK(*it == 3);
    }

    FL_SUBCASE("Iterator pre-increment") {
        auto it = ms.begin();
        auto next = ++it;
        FL_CHECK(*next == 3);
        FL_CHECK(*it == 3);
    }

    FL_SUBCASE("Iterator equality") {
        auto it1 = ms.begin();
        auto it2 = ms.begin();
        FL_CHECK(it1 == it2);

        ++it2;
        FL_CHECK(it1 != it2);
    }
}

FL_TEST_CASE("fl::multi_set comparison operators") {
    fl::multi_set<int> ms1;
    fl::multi_set<int> ms2;

    FL_SUBCASE("Empty multisets are equal") {
        FL_CHECK(ms1 == ms2);
        FL_CHECK_FALSE(ms1 != ms2);
    }

    FL_SUBCASE("Same elements are equal") {
        ms1.insert(1);
        ms1.insert(2);
        ms1.insert(2);

        ms2.insert(1);
        ms2.insert(2);
        ms2.insert(2);

        FL_CHECK(ms1 == ms2);
        FL_CHECK_FALSE(ms1 != ms2);
    }

    FL_SUBCASE("Different sizes are not equal") {
        ms1.insert(1);
        ms1.insert(2);

        ms2.insert(1);

        FL_CHECK(ms1 != ms2);
        FL_CHECK_FALSE(ms1 == ms2);
    }

    FL_SUBCASE("Different elements are not equal") {
        ms1.insert(1);
        ms1.insert(2);

        ms2.insert(1);
        ms2.insert(3);

        FL_CHECK(ms1 != ms2);
    }

    FL_SUBCASE("Lexicographic less comparison") {
        ms1.insert(1);
        ms1.insert(2);

        ms2.insert(1);
        ms2.insert(3);

        FL_CHECK(ms1 < ms2);
        FL_CHECK(ms1 <= ms2);
        FL_CHECK_FALSE(ms1 > ms2);
        FL_CHECK_FALSE(ms1 >= ms2);
    }
}

FL_TEST_CASE("fl::multi_set emplace") {
    fl::multi_set<int> ms;

    FL_SUBCASE("Emplace single element") {
        auto it = ms.emplace(42);
        FL_CHECK(ms.size() == 1);
        FL_CHECK(*it == 42);
    }

    FL_SUBCASE("Emplace multiple elements") {
        ms.emplace(1);
        ms.emplace(2);
        ms.emplace(2);
        ms.emplace(3);

        FL_CHECK(ms.size() == 4);
        FL_CHECK(ms.count(2) == 2);
    }
}

FL_TEST_CASE("fl::multi_set swap") {
    fl::multi_set<int> ms1;
    fl::multi_set<int> ms2;

    ms1.insert(1);
    ms1.insert(2);

    ms2.insert(10);
    ms2.insert(20);
    ms2.insert(30);

    ms1.swap(ms2);

    FL_CHECK(ms1.size() == 3);
    FL_CHECK(ms2.size() == 2);
    FL_CHECK(ms1.contains(10));
    FL_CHECK_FALSE(ms1.contains(1));
    FL_CHECK(ms2.contains(1));
    FL_CHECK_FALSE(ms2.contains(10));
}

FL_TEST_CASE("fl::multi_set initializer list constructor") {
    fl::multi_set<int> ms({5, 3, 3, 7, 1});

    FL_CHECK(ms.size() == 5);
    FL_CHECK(ms.count(3) == 2);
    FL_CHECK(ms.count(5) == 1);
    FL_CHECK(ms.count(7) == 1);
    FL_CHECK(ms.count(1) == 1);
}

FL_TEST_CASE("fl::multi_set const operations") {
    fl::multi_set<int> ms;
    ms.insert(1);
    ms.insert(2);
    ms.insert(2);
    ms.insert(3);

    const fl::multi_set<int>& cms = ms;

    FL_SUBCASE("const find") {
        auto it = cms.find(2);
        FL_CHECK(it != cms.end());
        FL_CHECK(*it == 2);
    }

    FL_SUBCASE("const equal_range") {
        auto range = cms.equal_range(2);
        int count = 0;
        for (auto it = range.first; it != range.second; ++it) {
            count++;
        }
        FL_CHECK(count == 2);
    }

    FL_SUBCASE("const iteration") {
        int count = 0;
        for (auto it = cms.cbegin(); it != cms.cend(); ++it) {
            count++;
        }
        FL_CHECK(count == 4);
    }
}

}
