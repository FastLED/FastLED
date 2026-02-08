#include "fl/stl/algorithm.h"
#include "fl/stl/vector.h"
#include "fl/stl/new.h"
#include "test.h"
#include "fl/stl/allocator.h"
#include "fl/stl/pair.h"
#include "fl/stl/random.h"

using namespace fl;

FL_TEST_CASE("fl::reverse") {
    FL_SUBCASE("reverse empty vector") {
        fl::vector<int> v;
        fl::reverse(v.begin(), v.end());
        FL_CHECK(v.empty());
    }

    FL_SUBCASE("reverse single element") {
        fl::vector<int> v = {42};
        fl::reverse(v.begin(), v.end());
        FL_CHECK_EQ(v[0], 42);
    }

    FL_SUBCASE("reverse multiple elements") {
        fl::vector<int> v = {1, 2, 3, 4, 5};
        fl::reverse(v.begin(), v.end());
        FL_CHECK_EQ(v[0], 5);
        FL_CHECK_EQ(v[1], 4);
        FL_CHECK_EQ(v[2], 3);
        FL_CHECK_EQ(v[3], 2);
        FL_CHECK_EQ(v[4], 1);
    }

    FL_SUBCASE("reverse even number of elements") {
        fl::vector<int> v = {1, 2, 3, 4};
        fl::reverse(v.begin(), v.end());
        FL_CHECK_EQ(v[0], 4);
        FL_CHECK_EQ(v[1], 3);
        FL_CHECK_EQ(v[2], 2);
        FL_CHECK_EQ(v[3], 1);
    }
}

FL_TEST_CASE("fl::max_element") {
    FL_SUBCASE("max_element empty range") {
        fl::vector<int> v;
        auto it = fl::max_element(v.begin(), v.end());
        FL_CHECK(it == v.end());
    }

    FL_SUBCASE("max_element single element") {
        fl::vector<int> v = {42};
        auto it = fl::max_element(v.begin(), v.end());
        FL_CHECK_EQ(*it, 42);
    }

    FL_SUBCASE("max_element finds maximum") {
        fl::vector<int> v = {3, 7, 2, 9, 1, 5};
        auto it = fl::max_element(v.begin(), v.end());
        FL_CHECK_EQ(*it, 9);
    }

    FL_SUBCASE("max_element with duplicates") {
        fl::vector<int> v = {1, 9, 3, 9, 2};
        auto it = fl::max_element(v.begin(), v.end());
        FL_CHECK_EQ(*it, 9);
        // Should find first occurrence
        FL_CHECK_EQ(it - v.begin(), 1);
    }

    FL_SUBCASE("max_element with custom comparator") {
        fl::vector<int> v = {3, 7, 2, 9, 1, 5};
        auto it = fl::max_element(v.begin(), v.end(), [](int a, int b) { return a > b; });
        FL_CHECK_EQ(*it, 1); // Min with reversed comparator
    }
}

FL_TEST_CASE("fl::min_element") {
    FL_SUBCASE("min_element empty range") {
        fl::vector<int> v;
        auto it = fl::min_element(v.begin(), v.end());
        FL_CHECK(it == v.end());
    }

    FL_SUBCASE("min_element single element") {
        fl::vector<int> v = {42};
        auto it = fl::min_element(v.begin(), v.end());
        FL_CHECK_EQ(*it, 42);
    }

    FL_SUBCASE("min_element finds minimum") {
        fl::vector<int> v = {3, 7, 2, 9, 1, 5};
        auto it = fl::min_element(v.begin(), v.end());
        FL_CHECK_EQ(*it, 1);
    }

    FL_SUBCASE("min_element with duplicates") {
        fl::vector<int> v = {3, 1, 7, 1, 2};
        auto it = fl::min_element(v.begin(), v.end());
        FL_CHECK_EQ(*it, 1);
        // Should find first occurrence
        FL_CHECK_EQ(it - v.begin(), 1);
    }

    FL_SUBCASE("min_element with custom comparator") {
        fl::vector<int> v = {3, 7, 2, 9, 1, 5};
        auto it = fl::min_element(v.begin(), v.end(), [](int a, int b) { return a > b; });
        FL_CHECK_EQ(*it, 9); // Max with reversed comparator
    }
}

FL_TEST_CASE("fl::equal") {
    FL_SUBCASE("equal empty ranges") {
        fl::vector<int> v1, v2;
        FL_CHECK(fl::equal(v1.begin(), v1.end(), v2.begin()));
    }

    FL_SUBCASE("equal identical ranges") {
        fl::vector<int> v1 = {1, 2, 3, 4, 5};
        fl::vector<int> v2 = {1, 2, 3, 4, 5};
        FL_CHECK(fl::equal(v1.begin(), v1.end(), v2.begin()));
    }

    FL_SUBCASE("equal different ranges") {
        fl::vector<int> v1 = {1, 2, 3, 4, 5};
        fl::vector<int> v2 = {1, 2, 3, 4, 6};
        FL_CHECK_FALSE(fl::equal(v1.begin(), v1.end(), v2.begin()));
    }

    FL_SUBCASE("equal with custom predicate") {
        fl::vector<int> v1 = {1, 2, 3};
        fl::vector<int> v2 = {2, 4, 6};
        FL_CHECK(fl::equal(v1.begin(), v1.end(), v2.begin(),
            [](int a, int b) { return a * 2 == b; }));
    }

    FL_SUBCASE("equal with both ranges checked") {
        fl::vector<int> v1 = {1, 2, 3};
        fl::vector<int> v2 = {1, 2, 3};
        FL_CHECK(fl::equal(v1.begin(), v1.end(), v2.begin(), v2.end()));
    }

    FL_SUBCASE("equal different sizes") {
        fl::vector<int> v1 = {1, 2, 3};
        fl::vector<int> v2 = {1, 2, 3, 4};
        FL_CHECK_FALSE(fl::equal(v1.begin(), v1.end(), v2.begin(), v2.end()));
    }
}

FL_TEST_CASE("fl::lexicographical_compare") {
    FL_SUBCASE("lexicographical empty ranges") {
        fl::vector<int> v1, v2;
        FL_CHECK_FALSE(fl::lexicographical_compare(v1.begin(), v1.end(), v2.begin(), v2.end()));
    }

    FL_SUBCASE("lexicographical first is less") {
        fl::vector<int> v1 = {1, 2, 3};
        fl::vector<int> v2 = {1, 2, 4};
        FL_CHECK(fl::lexicographical_compare(v1.begin(), v1.end(), v2.begin(), v2.end()));
    }

    FL_SUBCASE("lexicographical first is greater") {
        fl::vector<int> v1 = {1, 2, 5};
        fl::vector<int> v2 = {1, 2, 4};
        FL_CHECK_FALSE(fl::lexicographical_compare(v1.begin(), v1.end(), v2.begin(), v2.end()));
    }

    FL_SUBCASE("lexicographical first is prefix") {
        fl::vector<int> v1 = {1, 2};
        fl::vector<int> v2 = {1, 2, 3};
        FL_CHECK(fl::lexicographical_compare(v1.begin(), v1.end(), v2.begin(), v2.end()));
    }

    FL_SUBCASE("lexicographical second is prefix") {
        fl::vector<int> v1 = {1, 2, 3};
        fl::vector<int> v2 = {1, 2};
        FL_CHECK_FALSE(fl::lexicographical_compare(v1.begin(), v1.end(), v2.begin(), v2.end()));
    }

    FL_SUBCASE("lexicographical equal ranges") {
        fl::vector<int> v1 = {1, 2, 3};
        fl::vector<int> v2 = {1, 2, 3};
        FL_CHECK_FALSE(fl::lexicographical_compare(v1.begin(), v1.end(), v2.begin(), v2.end()));
    }

    FL_SUBCASE("lexicographical with custom comparator") {
        fl::vector<int> v1 = {3, 2, 1};
        fl::vector<int> v2 = {3, 2, 0};
        FL_CHECK(fl::lexicographical_compare(v1.begin(), v1.end(), v2.begin(), v2.end(),
            [](int a, int b) { return a > b; }));
    }
}

FL_TEST_CASE("fl::equal_container") {
    FL_SUBCASE("equal_container identical") {
        fl::vector<int> v1 = {1, 2, 3, 4, 5};
        fl::vector<int> v2 = {1, 2, 3, 4, 5};
        FL_CHECK(fl::equal_container(v1, v2));
    }

    FL_SUBCASE("equal_container different values") {
        fl::vector<int> v1 = {1, 2, 3, 4, 5};
        fl::vector<int> v2 = {1, 2, 3, 4, 6};
        FL_CHECK_FALSE(fl::equal_container(v1, v2));
    }

    FL_SUBCASE("equal_container different sizes") {
        fl::vector<int> v1 = {1, 2, 3};
        fl::vector<int> v2 = {1, 2, 3, 4};
        FL_CHECK_FALSE(fl::equal_container(v1, v2));
    }

    FL_SUBCASE("equal_container with predicate") {
        fl::vector<int> v1 = {1, 2, 3};
        fl::vector<int> v2 = {2, 4, 6};
        FL_CHECK(fl::equal_container(v1, v2, [](int a, int b) { return a * 2 == b; }));
    }
}

FL_TEST_CASE("fl::fill") {
    FL_SUBCASE("fill empty range") {
        fl::vector<int> v;
        fl::fill(v.begin(), v.end(), 42);
        FL_CHECK(v.empty());
    }

    FL_SUBCASE("fill with value") {
        fl::vector<int> v(5);
        fl::fill(v.begin(), v.end(), 42);
        for (int i = 0; i < 5; i++) {
            FL_CHECK_EQ(v[i], 42);
        }
    }

    FL_SUBCASE("fill partial range") {
        fl::vector<int> v = {1, 2, 3, 4, 5};
        fl::fill(v.begin() + 1, v.begin() + 4, 99);
        FL_CHECK_EQ(v[0], 1);
        FL_CHECK_EQ(v[1], 99);
        FL_CHECK_EQ(v[2], 99);
        FL_CHECK_EQ(v[3], 99);
        FL_CHECK_EQ(v[4], 5);
    }
}

FL_TEST_CASE("fl::find") {
    FL_SUBCASE("find in empty range") {
        fl::vector<int> v;
        auto it = fl::find(v.begin(), v.end(), 42);
        FL_CHECK(it == v.end());
    }

    FL_SUBCASE("find existing element") {
        fl::vector<int> v = {1, 2, 3, 4, 5};
        auto it = fl::find(v.begin(), v.end(), 3);
        FL_CHECK(it != v.end());
        FL_CHECK_EQ(*it, 3);
        FL_CHECK_EQ(it - v.begin(), 2);
    }

    FL_SUBCASE("find non-existing element") {
        fl::vector<int> v = {1, 2, 3, 4, 5};
        auto it = fl::find(v.begin(), v.end(), 10);
        FL_CHECK(it == v.end());
    }

    FL_SUBCASE("find first occurrence") {
        fl::vector<int> v = {1, 2, 3, 2, 5};
        auto it = fl::find(v.begin(), v.end(), 2);
        FL_CHECK_EQ(it - v.begin(), 1);
    }
}

FL_TEST_CASE("fl::find_if") {
    FL_SUBCASE("find_if in empty range") {
        fl::vector<int> v;
        auto it = fl::find_if(v.begin(), v.end(), [](int x) { return x > 5; });
        FL_CHECK(it == v.end());
    }

    FL_SUBCASE("find_if existing element") {
        fl::vector<int> v = {1, 2, 3, 4, 5};
        auto it = fl::find_if(v.begin(), v.end(), [](int x) { return x > 3; });
        FL_CHECK(it != v.end());
        FL_CHECK_EQ(*it, 4);
    }

    FL_SUBCASE("find_if non-existing element") {
        fl::vector<int> v = {1, 2, 3, 4, 5};
        auto it = fl::find_if(v.begin(), v.end(), [](int x) { return x > 10; });
        FL_CHECK(it == v.end());
    }
}

FL_TEST_CASE("fl::find_if_not") {
    FL_SUBCASE("find_if_not in empty range") {
        fl::vector<int> v;
        auto it = fl::find_if_not(v.begin(), v.end(), [](int x) { return x > 5; });
        FL_CHECK(it == v.end());
    }

    FL_SUBCASE("find_if_not existing element") {
        fl::vector<int> v = {1, 2, 3, 4, 5};
        auto it = fl::find_if_not(v.begin(), v.end(), [](int x) { return x < 3; });
        FL_CHECK(it != v.end());
        FL_CHECK_EQ(*it, 3);
    }

    FL_SUBCASE("find_if_not all match") {
        fl::vector<int> v = {1, 2, 3, 4, 5};
        auto it = fl::find_if_not(v.begin(), v.end(), [](int x) { return x < 10; });
        FL_CHECK(it == v.end());
    }
}

FL_TEST_CASE("fl::remove") {
    FL_SUBCASE("remove from empty range") {
        fl::vector<int> v;
        auto new_end = fl::remove(v.begin(), v.end(), 42);
        FL_CHECK(new_end == v.end());
    }

    FL_SUBCASE("remove no matching elements") {
        fl::vector<int> v = {1, 2, 3, 4, 5};
        auto new_end = fl::remove(v.begin(), v.end(), 10);
        FL_CHECK(new_end == v.end());
    }

    FL_SUBCASE("remove matching elements") {
        fl::vector<int> v = {1, 2, 3, 2, 4, 2, 5};
        auto new_end = fl::remove(v.begin(), v.end(), 2);
        FL_CHECK_EQ(new_end - v.begin(), 4);
        FL_CHECK_EQ(v[0], 1);
        FL_CHECK_EQ(v[1], 3);
        FL_CHECK_EQ(v[2], 4);
        FL_CHECK_EQ(v[3], 5);
    }

    FL_SUBCASE("remove all elements") {
        fl::vector<int> v = {2, 2, 2, 2};
        auto new_end = fl::remove(v.begin(), v.end(), 2);
        FL_CHECK(new_end == v.begin());
    }
}

FL_TEST_CASE("fl::remove_if") {
    FL_SUBCASE("remove_if from empty range") {
        fl::vector<int> v;
        auto new_end = fl::remove_if(v.begin(), v.end(), [](int x) { return x > 5; });
        FL_CHECK(new_end == v.end());
    }

    FL_SUBCASE("remove_if no matching elements") {
        fl::vector<int> v = {1, 2, 3, 4, 5};
        auto new_end = fl::remove_if(v.begin(), v.end(), [](int x) { return x > 10; });
        FL_CHECK(new_end == v.end());
    }

    FL_SUBCASE("remove_if matching elements") {
        fl::vector<int> v = {1, 2, 3, 4, 5, 6, 7};
        auto new_end = fl::remove_if(v.begin(), v.end(), [](int x) { return x % 2 == 0; });
        FL_CHECK_EQ(new_end - v.begin(), 4);
        FL_CHECK_EQ(v[0], 1);
        FL_CHECK_EQ(v[1], 3);
        FL_CHECK_EQ(v[2], 5);
        FL_CHECK_EQ(v[3], 7);
    }
}

FL_TEST_CASE("fl::sort") {
    FL_SUBCASE("sort empty range") {
        fl::vector<int> v;
        fl::sort(v.begin(), v.end());
        FL_CHECK(v.empty());
    }

    FL_SUBCASE("sort single element") {
        fl::vector<int> v = {42};
        fl::sort(v.begin(), v.end());
        FL_CHECK_EQ(v[0], 42);
    }

    FL_SUBCASE("sort already sorted") {
        fl::vector<int> v = {1, 2, 3, 4, 5};
        fl::sort(v.begin(), v.end());
        FL_CHECK_EQ(v[0], 1);
        FL_CHECK_EQ(v[1], 2);
        FL_CHECK_EQ(v[2], 3);
        FL_CHECK_EQ(v[3], 4);
        FL_CHECK_EQ(v[4], 5);
    }

    FL_SUBCASE("sort reverse order") {
        fl::vector<int> v = {5, 4, 3, 2, 1};
        fl::sort(v.begin(), v.end());
        FL_CHECK_EQ(v[0], 1);
        FL_CHECK_EQ(v[1], 2);
        FL_CHECK_EQ(v[2], 3);
        FL_CHECK_EQ(v[3], 4);
        FL_CHECK_EQ(v[4], 5);
    }

    FL_SUBCASE("sort random order") {
        fl::vector<int> v = {3, 1, 4, 1, 5, 9, 2, 6, 5, 3};
        fl::sort(v.begin(), v.end());
        FL_CHECK_EQ(v[0], 1);
        FL_CHECK_EQ(v[1], 1);
        FL_CHECK_EQ(v[2], 2);
        FL_CHECK_EQ(v[3], 3);
        FL_CHECK_EQ(v[4], 3);
        FL_CHECK_EQ(v[5], 4);
        FL_CHECK_EQ(v[6], 5);
        FL_CHECK_EQ(v[7], 5);
        FL_CHECK_EQ(v[8], 6);
        FL_CHECK_EQ(v[9], 9);
    }

    FL_SUBCASE("sort with custom comparator") {
        fl::vector<int> v = {1, 2, 3, 4, 5};
        fl::sort(v.begin(), v.end(), [](int a, int b) { return a > b; });
        FL_CHECK_EQ(v[0], 5);
        FL_CHECK_EQ(v[1], 4);
        FL_CHECK_EQ(v[2], 3);
        FL_CHECK_EQ(v[3], 2);
        FL_CHECK_EQ(v[4], 1);
    }

    FL_SUBCASE("sort larger array") {
        fl::vector<int> v;
        for (int i = 100; i > 0; i--) {
            v.push_back(i);
        }
        fl::sort(v.begin(), v.end());
        for (int i = 0; i < 100; i++) {
            FL_CHECK_EQ(v[i], i + 1);
        }
    }
}

FL_TEST_CASE("fl::stable_sort") {
    FL_SUBCASE("stable_sort empty range") {
        fl::vector<int> v;
        fl::stable_sort(v.begin(), v.end());
        FL_CHECK(v.empty());
    }

    FL_SUBCASE("stable_sort single element") {
        fl::vector<int> v = {42};
        fl::stable_sort(v.begin(), v.end());
        FL_CHECK_EQ(v[0], 42);
    }

    FL_SUBCASE("stable_sort preserves order") {
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

        FL_CHECK_EQ(v[0].first, 1);
        FL_CHECK_EQ(v[0].second, 1);
        FL_CHECK_EQ(v[1].first, 2);
        FL_CHECK_EQ(v[1].second, 3);
        FL_CHECK_EQ(v[2].first, 3);
        FL_CHECK_EQ(v[2].second, 0); // Original order preserved
        FL_CHECK_EQ(v[3].first, 3);
        FL_CHECK_EQ(v[3].second, 2);
        FL_CHECK_EQ(v[4].first, 3);
        FL_CHECK_EQ(v[4].second, 4);
    }

    FL_SUBCASE("stable_sort reverse order") {
        fl::vector<int> v = {5, 4, 3, 2, 1};
        fl::stable_sort(v.begin(), v.end());
        FL_CHECK_EQ(v[0], 1);
        FL_CHECK_EQ(v[1], 2);
        FL_CHECK_EQ(v[2], 3);
        FL_CHECK_EQ(v[3], 4);
        FL_CHECK_EQ(v[4], 5);
    }

    FL_SUBCASE("stable_sort larger array") {
        fl::vector<int> v;
        for (int i = 100; i > 0; i--) {
            v.push_back(i);
        }
        fl::stable_sort(v.begin(), v.end());
        for (int i = 0; i < 100; i++) {
            FL_CHECK_EQ(v[i], i + 1);
        }
    }
}

FL_TEST_CASE("fl::shuffle") {
    FL_SUBCASE("shuffle empty range") {
        fl::vector<int> v;
        fl::shuffle(v.begin(), v.end());
        FL_CHECK(v.empty());
    }

    FL_SUBCASE("shuffle single element") {
        fl::vector<int> v = {42};
        fl::shuffle(v.begin(), v.end());
        FL_CHECK_EQ(v[0], 42);
    }

    FL_SUBCASE("shuffle contains all elements") {
        fl::vector<int> original = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        fl::vector<int> v = original;

        fl::shuffle(v.begin(), v.end());

        // Check that all elements are still present
        fl::sort(v.begin(), v.end());
        FL_CHECK(fl::equal(v.begin(), v.end(), original.begin()));
    }

    FL_SUBCASE("shuffle with custom generator") {
        fl::vector<int> original = {1, 2, 3, 4, 5};
        fl::vector<int> v = original;

        fl_random rng(12345);
        fl::shuffle(v.begin(), v.end(), rng);

        // Check that all elements are still present
        fl::sort(v.begin(), v.end());
        FL_CHECK(fl::equal(v.begin(), v.end(), original.begin()));
    }
}
