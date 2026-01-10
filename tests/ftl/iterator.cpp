// Test for fl::back_inserter and iterator utilities

#include "fl/stl/iterator.h"
#include "fl/stl/vector.h"
#include "fl/stl/new.h"
#include "doctest.h"
#include "fl/stl/vector.h"

TEST_CASE("back_inserter with vector") {
    fl::vector<int> vec;

    SUBCASE("Basic insertion") {
        auto inserter = fl::back_inserter(vec);
        *inserter = 10;
        *inserter = 20;
        *inserter = 30;

        CHECK(vec.size() == 3);
        CHECK(vec[0] == 10);
        CHECK(vec[1] == 20);
        CHECK(vec[2] == 30);
    }

    SUBCASE("Pre-increment") {
        auto inserter = fl::back_inserter(vec);
        ++inserter = 10;

        CHECK(vec.size() == 1);
        CHECK(vec[0] == 10);
    }

    SUBCASE("Post-increment") {
        auto inserter = fl::back_inserter(vec);
        inserter++ = 10;

        CHECK(vec.size() == 1);
        CHECK(vec[0] == 10);
    }

    SUBCASE("Dereference") {
        auto inserter = fl::back_inserter(vec);
        *inserter = 10;
        *inserter = 20;

        CHECK(vec.size() == 2);
        CHECK(vec[0] == 10);
        CHECK(vec[1] == 20);
    }
}

TEST_CASE("back_inserter with FixedVector") {
    fl::FixedVector<int, 5> vec;

    SUBCASE("Basic insertion") {
        auto inserter = fl::back_inserter(vec);
        *inserter = 10;
        *inserter = 20;
        *inserter = 30;

        CHECK(vec.size() == 3);
        CHECK(vec[0] == 10);
        CHECK(vec[1] == 20);
        CHECK(vec[2] == 30);
    }
}

TEST_CASE("back_inserter with InlinedVector") {
    fl::InlinedVector<int, 3> vec;

    SUBCASE("Basic insertion within inline capacity") {
        auto inserter = fl::back_inserter(vec);
        *inserter = 10;
        *inserter = 20;

        CHECK(vec.size() == 2);
        CHECK(vec[0] == 10);
        CHECK(vec[1] == 20);
    }

    SUBCASE("Insertion beyond inline capacity (heap allocation)") {
        auto inserter = fl::back_inserter(vec);
        *inserter = 10;
        *inserter = 20;
        *inserter = 30;
        *inserter = 40;

        CHECK(vec.size() == 4);
        CHECK(vec[0] == 10);
        CHECK(vec[1] == 20);
        CHECK(vec[2] == 30);
        CHECK(vec[3] == 40);
    }
}

TEST_CASE("back_inserter with move semantics") {
    struct MoveOnly {
        int value;

        MoveOnly(int v) : value(v) {}

        // Delete copy constructor and copy assignment
        MoveOnly(const MoveOnly&) = delete;
        MoveOnly& operator=(const MoveOnly&) = delete;

        // Move constructor and move assignment
        MoveOnly(MoveOnly&& other) : value(other.value) {
            other.value = -1;
        }

        MoveOnly& operator=(MoveOnly&& other) {
            value = other.value;
            other.value = -1;
            return *this;
        }
    };

    fl::vector<MoveOnly> vec;

    SUBCASE("Move insertion") {
        auto inserter = fl::back_inserter(vec);
        *inserter = MoveOnly(42);

        CHECK(vec.size() == 1);
        CHECK(vec[0].value == 42);
    }
}

// Test integration with algorithms
TEST_CASE("back_inserter with algorithm integration") {
    fl::vector<int> source;
    source.push_back(1);
    source.push_back(2);
    source.push_back(3);

    fl::vector<int> dest;

    SUBCASE("Manual copy using back_inserter") {
        auto inserter = fl::back_inserter(dest);
        for (auto it = source.begin(); it != source.end(); ++it) {
            *inserter++ = *it;
        }

        CHECK(dest.size() == 3);
        CHECK(dest[0] == 1);
        CHECK(dest[1] == 2);
        CHECK(dest[2] == 3);
    }
}
