#include "fl/stl/algorithm.h"
#include "fl/stl/vector.h"
#include "fl/stl/new.h"
#include "doctest.h"
#include "fl/stl/allocator.h"
#include "fl/stl/pair.h"
#include "fl/stl/random.h"

using namespace fl;

TEST_CASE("fl::reverse") {
    SUBCASE("reverse empty vector") {
        fl::vector<int> v;
        fl::reverse(v.begin(), v.end());
        CHECK(v.empty());
    }

    SUBCASE("reverse single element") {
        fl::vector<int> v = {42};
        fl::reverse(v.begin(), v.end());
        CHECK_EQ(v[0], 42);
    }

    SUBCASE("reverse multiple elements") {
        fl::vector<int> v = {1, 2, 3, 4, 5};
        fl::reverse(v.begin(), v.end());
        CHECK_EQ(v[0], 5);
        CHECK_EQ(v[1], 4);
        CHECK_EQ(v[2], 3);
        CHECK_EQ(v[3], 2);
        CHECK_EQ(v[4], 1);
    }

    SUBCASE("reverse even number of elements") {
        fl::vector<int> v = {1, 2, 3, 4};
        fl::reverse(v.begin(), v.end());
        CHECK_EQ(v[0], 4);
        CHECK_EQ(v[1], 3);
        CHECK_EQ(v[2], 2);
        CHECK_EQ(v[3], 1);
    }
}

TEST_CASE("fl::max_element") {
    SUBCASE("max_element empty range") {
        fl::vector<int> v;
        auto it = fl::max_element(v.begin(), v.end());
        CHECK(it == v.end());
    }

    SUBCASE("max_element single element") {
        fl::vector<int> v = {42};
        auto it = fl::max_element(v.begin(), v.end());
        CHECK_EQ(*it, 42);
    }

    SUBCASE("max_element finds maximum") {
        fl::vector<int> v = {3, 7, 2, 9, 1, 5};
        auto it = fl::max_element(v.begin(), v.end());
        CHECK_EQ(*it, 9);
    }

    SUBCASE("max_element with duplicates") {
        fl::vector<int> v = {1, 9, 3, 9, 2};
        auto it = fl::max_element(v.begin(), v.end());
        CHECK_EQ(*it, 9);
        // Should find first occurrence
        CHECK_EQ(it - v.begin(), 1);
    }

    SUBCASE("max_element with custom comparator") {
        fl::vector<int> v = {3, 7, 2, 9, 1, 5};
        auto it = fl::max_element(v.begin(), v.end(), [](int a, int b) { return a > b; });
        CHECK_EQ(*it, 1); // Min with reversed comparator
    }
}

TEST_CASE("fl::min_element") {
    SUBCASE("min_element empty range") {
        fl::vector<int> v;
        auto it = fl::min_element(v.begin(), v.end());
        CHECK(it == v.end());
    }

    SUBCASE("min_element single element") {
        fl::vector<int> v = {42};
        auto it = fl::min_element(v.begin(), v.end());
        CHECK_EQ(*it, 42);
    }

    SUBCASE("min_element finds minimum") {
        fl::vector<int> v = {3, 7, 2, 9, 1, 5};
        auto it = fl::min_element(v.begin(), v.end());
        CHECK_EQ(*it, 1);
    }

    SUBCASE("min_element with duplicates") {
        fl::vector<int> v = {3, 1, 7, 1, 2};
        auto it = fl::min_element(v.begin(), v.end());
        CHECK_EQ(*it, 1);
        // Should find first occurrence
        CHECK_EQ(it - v.begin(), 1);
    }

    SUBCASE("min_element with custom comparator") {
        fl::vector<int> v = {3, 7, 2, 9, 1, 5};
        auto it = fl::min_element(v.begin(), v.end(), [](int a, int b) { return a > b; });
        CHECK_EQ(*it, 9); // Max with reversed comparator
    }
}

TEST_CASE("fl::equal") {
    SUBCASE("equal empty ranges") {
        fl::vector<int> v1, v2;
        CHECK(fl::equal(v1.begin(), v1.end(), v2.begin()));
    }

    SUBCASE("equal identical ranges") {
        fl::vector<int> v1 = {1, 2, 3, 4, 5};
        fl::vector<int> v2 = {1, 2, 3, 4, 5};
        CHECK(fl::equal(v1.begin(), v1.end(), v2.begin()));
    }

    SUBCASE("equal different ranges") {
        fl::vector<int> v1 = {1, 2, 3, 4, 5};
        fl::vector<int> v2 = {1, 2, 3, 4, 6};
        CHECK_FALSE(fl::equal(v1.begin(), v1.end(), v2.begin()));
    }

    SUBCASE("equal with custom predicate") {
        fl::vector<int> v1 = {1, 2, 3};
        fl::vector<int> v2 = {2, 4, 6};
        CHECK(fl::equal(v1.begin(), v1.end(), v2.begin(),
            [](int a, int b) { return a * 2 == b; }));
    }

    SUBCASE("equal with both ranges checked") {
        fl::vector<int> v1 = {1, 2, 3};
        fl::vector<int> v2 = {1, 2, 3};
        CHECK(fl::equal(v1.begin(), v1.end(), v2.begin(), v2.end()));
    }

    SUBCASE("equal different sizes") {
        fl::vector<int> v1 = {1, 2, 3};
        fl::vector<int> v2 = {1, 2, 3, 4};
        CHECK_FALSE(fl::equal(v1.begin(), v1.end(), v2.begin(), v2.end()));
    }
}

TEST_CASE("fl::lexicographical_compare") {
    SUBCASE("lexicographical empty ranges") {
        fl::vector<int> v1, v2;
        CHECK_FALSE(fl::lexicographical_compare(v1.begin(), v1.end(), v2.begin(), v2.end()));
    }

    SUBCASE("lexicographical first is less") {
        fl::vector<int> v1 = {1, 2, 3};
        fl::vector<int> v2 = {1, 2, 4};
        CHECK(fl::lexicographical_compare(v1.begin(), v1.end(), v2.begin(), v2.end()));
    }

    SUBCASE("lexicographical first is greater") {
        fl::vector<int> v1 = {1, 2, 5};
        fl::vector<int> v2 = {1, 2, 4};
        CHECK_FALSE(fl::lexicographical_compare(v1.begin(), v1.end(), v2.begin(), v2.end()));
    }

    SUBCASE("lexicographical first is prefix") {
        fl::vector<int> v1 = {1, 2};
        fl::vector<int> v2 = {1, 2, 3};
        CHECK(fl::lexicographical_compare(v1.begin(), v1.end(), v2.begin(), v2.end()));
    }

    SUBCASE("lexicographical second is prefix") {
        fl::vector<int> v1 = {1, 2, 3};
        fl::vector<int> v2 = {1, 2};
        CHECK_FALSE(fl::lexicographical_compare(v1.begin(), v1.end(), v2.begin(), v2.end()));
    }

    SUBCASE("lexicographical equal ranges") {
        fl::vector<int> v1 = {1, 2, 3};
        fl::vector<int> v2 = {1, 2, 3};
        CHECK_FALSE(fl::lexicographical_compare(v1.begin(), v1.end(), v2.begin(), v2.end()));
    }

    SUBCASE("lexicographical with custom comparator") {
        fl::vector<int> v1 = {3, 2, 1};
        fl::vector<int> v2 = {3, 2, 0};
        CHECK(fl::lexicographical_compare(v1.begin(), v1.end(), v2.begin(), v2.end(),
            [](int a, int b) { return a > b; }));
    }
}

TEST_CASE("fl::equal_container") {
    SUBCASE("equal_container identical") {
        fl::vector<int> v1 = {1, 2, 3, 4, 5};
        fl::vector<int> v2 = {1, 2, 3, 4, 5};
        CHECK(fl::equal_container(v1, v2));
    }

    SUBCASE("equal_container different values") {
        fl::vector<int> v1 = {1, 2, 3, 4, 5};
        fl::vector<int> v2 = {1, 2, 3, 4, 6};
        CHECK_FALSE(fl::equal_container(v1, v2));
    }

    SUBCASE("equal_container different sizes") {
        fl::vector<int> v1 = {1, 2, 3};
        fl::vector<int> v2 = {1, 2, 3, 4};
        CHECK_FALSE(fl::equal_container(v1, v2));
    }

    SUBCASE("equal_container with predicate") {
        fl::vector<int> v1 = {1, 2, 3};
        fl::vector<int> v2 = {2, 4, 6};
        CHECK(fl::equal_container(v1, v2, [](int a, int b) { return a * 2 == b; }));
    }
}

TEST_CASE("fl::fill") {
    SUBCASE("fill empty range") {
        fl::vector<int> v;
        fl::fill(v.begin(), v.end(), 42);
        CHECK(v.empty());
    }

    SUBCASE("fill with value") {
        fl::vector<int> v(5);
        fl::fill(v.begin(), v.end(), 42);
        for (int i = 0; i < 5; i++) {
            CHECK_EQ(v[i], 42);
        }
    }

    SUBCASE("fill partial range") {
        fl::vector<int> v = {1, 2, 3, 4, 5};
        fl::fill(v.begin() + 1, v.begin() + 4, 99);
        CHECK_EQ(v[0], 1);
        CHECK_EQ(v[1], 99);
        CHECK_EQ(v[2], 99);
        CHECK_EQ(v[3], 99);
        CHECK_EQ(v[4], 5);
    }
}

TEST_CASE("fl::find") {
    SUBCASE("find in empty range") {
        fl::vector<int> v;
        auto it = fl::find(v.begin(), v.end(), 42);
        CHECK(it == v.end());
    }

    SUBCASE("find existing element") {
        fl::vector<int> v = {1, 2, 3, 4, 5};
        auto it = fl::find(v.begin(), v.end(), 3);
        CHECK(it != v.end());
        CHECK_EQ(*it, 3);
        CHECK_EQ(it - v.begin(), 2);
    }

    SUBCASE("find non-existing element") {
        fl::vector<int> v = {1, 2, 3, 4, 5};
        auto it = fl::find(v.begin(), v.end(), 10);
        CHECK(it == v.end());
    }

    SUBCASE("find first occurrence") {
        fl::vector<int> v = {1, 2, 3, 2, 5};
        auto it = fl::find(v.begin(), v.end(), 2);
        CHECK_EQ(it - v.begin(), 1);
    }
}

TEST_CASE("fl::find_if") {
    SUBCASE("find_if in empty range") {
        fl::vector<int> v;
        auto it = fl::find_if(v.begin(), v.end(), [](int x) { return x > 5; });
        CHECK(it == v.end());
    }

    SUBCASE("find_if existing element") {
        fl::vector<int> v = {1, 2, 3, 4, 5};
        auto it = fl::find_if(v.begin(), v.end(), [](int x) { return x > 3; });
        CHECK(it != v.end());
        CHECK_EQ(*it, 4);
    }

    SUBCASE("find_if non-existing element") {
        fl::vector<int> v = {1, 2, 3, 4, 5};
        auto it = fl::find_if(v.begin(), v.end(), [](int x) { return x > 10; });
        CHECK(it == v.end());
    }
}

TEST_CASE("fl::find_if_not") {
    SUBCASE("find_if_not in empty range") {
        fl::vector<int> v;
        auto it = fl::find_if_not(v.begin(), v.end(), [](int x) { return x > 5; });
        CHECK(it == v.end());
    }

    SUBCASE("find_if_not existing element") {
        fl::vector<int> v = {1, 2, 3, 4, 5};
        auto it = fl::find_if_not(v.begin(), v.end(), [](int x) { return x < 3; });
        CHECK(it != v.end());
        CHECK_EQ(*it, 3);
    }

    SUBCASE("find_if_not all match") {
        fl::vector<int> v = {1, 2, 3, 4, 5};
        auto it = fl::find_if_not(v.begin(), v.end(), [](int x) { return x < 10; });
        CHECK(it == v.end());
    }
}

TEST_CASE("fl::remove") {
    SUBCASE("remove from empty range") {
        fl::vector<int> v;
        auto new_end = fl::remove(v.begin(), v.end(), 42);
        CHECK(new_end == v.end());
    }

    SUBCASE("remove no matching elements") {
        fl::vector<int> v = {1, 2, 3, 4, 5};
        auto new_end = fl::remove(v.begin(), v.end(), 10);
        CHECK(new_end == v.end());
    }

    SUBCASE("remove matching elements") {
        fl::vector<int> v = {1, 2, 3, 2, 4, 2, 5};
        auto new_end = fl::remove(v.begin(), v.end(), 2);
        CHECK_EQ(new_end - v.begin(), 4);
        CHECK_EQ(v[0], 1);
        CHECK_EQ(v[1], 3);
        CHECK_EQ(v[2], 4);
        CHECK_EQ(v[3], 5);
    }

    SUBCASE("remove all elements") {
        fl::vector<int> v = {2, 2, 2, 2};
        auto new_end = fl::remove(v.begin(), v.end(), 2);
        CHECK(new_end == v.begin());
    }
}

TEST_CASE("fl::remove_if") {
    SUBCASE("remove_if from empty range") {
        fl::vector<int> v;
        auto new_end = fl::remove_if(v.begin(), v.end(), [](int x) { return x > 5; });
        CHECK(new_end == v.end());
    }

    SUBCASE("remove_if no matching elements") {
        fl::vector<int> v = {1, 2, 3, 4, 5};
        auto new_end = fl::remove_if(v.begin(), v.end(), [](int x) { return x > 10; });
        CHECK(new_end == v.end());
    }

    SUBCASE("remove_if matching elements") {
        fl::vector<int> v = {1, 2, 3, 4, 5, 6, 7};
        auto new_end = fl::remove_if(v.begin(), v.end(), [](int x) { return x % 2 == 0; });
        CHECK_EQ(new_end - v.begin(), 4);
        CHECK_EQ(v[0], 1);
        CHECK_EQ(v[1], 3);
        CHECK_EQ(v[2], 5);
        CHECK_EQ(v[3], 7);
    }
}

TEST_CASE("fl::sort") {
    SUBCASE("sort empty range") {
        fl::vector<int> v;
        fl::sort(v.begin(), v.end());
        CHECK(v.empty());
    }

    SUBCASE("sort single element") {
        fl::vector<int> v = {42};
        fl::sort(v.begin(), v.end());
        CHECK_EQ(v[0], 42);
    }

    SUBCASE("sort already sorted") {
        fl::vector<int> v = {1, 2, 3, 4, 5};
        fl::sort(v.begin(), v.end());
        CHECK_EQ(v[0], 1);
        CHECK_EQ(v[1], 2);
        CHECK_EQ(v[2], 3);
        CHECK_EQ(v[3], 4);
        CHECK_EQ(v[4], 5);
    }

    SUBCASE("sort reverse order") {
        fl::vector<int> v = {5, 4, 3, 2, 1};
        fl::sort(v.begin(), v.end());
        CHECK_EQ(v[0], 1);
        CHECK_EQ(v[1], 2);
        CHECK_EQ(v[2], 3);
        CHECK_EQ(v[3], 4);
        CHECK_EQ(v[4], 5);
    }

    SUBCASE("sort random order") {
        fl::vector<int> v = {3, 1, 4, 1, 5, 9, 2, 6, 5, 3};
        fl::sort(v.begin(), v.end());
        CHECK_EQ(v[0], 1);
        CHECK_EQ(v[1], 1);
        CHECK_EQ(v[2], 2);
        CHECK_EQ(v[3], 3);
        CHECK_EQ(v[4], 3);
        CHECK_EQ(v[5], 4);
        CHECK_EQ(v[6], 5);
        CHECK_EQ(v[7], 5);
        CHECK_EQ(v[8], 6);
        CHECK_EQ(v[9], 9);
    }

    SUBCASE("sort with custom comparator") {
        fl::vector<int> v = {1, 2, 3, 4, 5};
        fl::sort(v.begin(), v.end(), [](int a, int b) { return a > b; });
        CHECK_EQ(v[0], 5);
        CHECK_EQ(v[1], 4);
        CHECK_EQ(v[2], 3);
        CHECK_EQ(v[3], 2);
        CHECK_EQ(v[4], 1);
    }

    SUBCASE("sort larger array") {
        fl::vector<int> v;
        for (int i = 100; i > 0; i--) {
            v.push_back(i);
        }
        fl::sort(v.begin(), v.end());
        for (int i = 0; i < 100; i++) {
            CHECK_EQ(v[i], i + 1);
        }
    }
}

TEST_CASE("fl::stable_sort") {
    SUBCASE("stable_sort empty range") {
        fl::vector<int> v;
        fl::stable_sort(v.begin(), v.end());
        CHECK(v.empty());
    }

    SUBCASE("stable_sort single element") {
        fl::vector<int> v = {42};
        fl::stable_sort(v.begin(), v.end());
        CHECK_EQ(v[0], 42);
    }

    SUBCASE("stable_sort preserves order") {
        // Using pairs where second value tracks original position
        fl::vector<fl::pair<int, int>> v;
        v.push_back({3, 0});
        v.push_back({1, 1});
        v.push_back({3, 2});
        v.push_back({2, 3});
        v.push_back({3, 4});

        fl::stable_sort(v.begin(), v.end(),
            [](const fl::pair<int, int>& a, const fl::pair<int, int>& b) {
                return a.first < b.first;
            });

        CHECK_EQ(v[0].first, 1);
        CHECK_EQ(v[0].second, 1);
        CHECK_EQ(v[1].first, 2);
        CHECK_EQ(v[1].second, 3);
        CHECK_EQ(v[2].first, 3);
        CHECK_EQ(v[2].second, 0); // Original order preserved
        CHECK_EQ(v[3].first, 3);
        CHECK_EQ(v[3].second, 2);
        CHECK_EQ(v[4].first, 3);
        CHECK_EQ(v[4].second, 4);
    }

    SUBCASE("stable_sort reverse order") {
        fl::vector<int> v = {5, 4, 3, 2, 1};
        fl::stable_sort(v.begin(), v.end());
        CHECK_EQ(v[0], 1);
        CHECK_EQ(v[1], 2);
        CHECK_EQ(v[2], 3);
        CHECK_EQ(v[3], 4);
        CHECK_EQ(v[4], 5);
    }

    SUBCASE("stable_sort larger array") {
        fl::vector<int> v;
        for (int i = 100; i > 0; i--) {
            v.push_back(i);
        }
        fl::stable_sort(v.begin(), v.end());
        for (int i = 0; i < 100; i++) {
            CHECK_EQ(v[i], i + 1);
        }
    }
}

TEST_CASE("fl::shuffle") {
    SUBCASE("shuffle empty range") {
        fl::vector<int> v;
        fl::shuffle(v.begin(), v.end());
        CHECK(v.empty());
    }

    SUBCASE("shuffle single element") {
        fl::vector<int> v = {42};
        fl::shuffle(v.begin(), v.end());
        CHECK_EQ(v[0], 42);
    }

    SUBCASE("shuffle contains all elements") {
        fl::vector<int> original = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        fl::vector<int> v = original;

        fl::shuffle(v.begin(), v.end());

        // Check that all elements are still present
        fl::sort(v.begin(), v.end());
        CHECK(fl::equal(v.begin(), v.end(), original.begin()));
    }

    SUBCASE("shuffle with custom generator") {
        fl::vector<int> original = {1, 2, 3, 4, 5};
        fl::vector<int> v = original;

        fl_random rng(12345);
        fl::shuffle(v.begin(), v.end(), rng);

        // Check that all elements are still present
        fl::sort(v.begin(), v.end());
        CHECK(fl::equal(v.begin(), v.end(), original.begin()));
    }
}
