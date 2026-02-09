// Test for fl::back_inserter and iterator utilities

#include "fl/stl/iterator.h"
#include "fl/stl/vector.h"
#include "fl/stl/new.h"
#include "test.h"
#include "fl/stl/vector.h"

FL_TEST_CASE("back_inserter with vector") {
    fl::vector<int> vec;

    FL_SUBCASE("Basic insertion") {
        auto inserter = fl::back_inserter(vec);
        *inserter = 10;
        *inserter = 20;
        *inserter = 30;

        FL_CHECK(vec.size() == 3);
        FL_CHECK(vec[0] == 10);
        FL_CHECK(vec[1] == 20);
        FL_CHECK(vec[2] == 30);
    }

    FL_SUBCASE("Pre-increment") {
        auto inserter = fl::back_inserter(vec);
        ++inserter = 10;

        FL_CHECK(vec.size() == 1);
        FL_CHECK(vec[0] == 10);
    }

    FL_SUBCASE("Post-increment") {
        auto inserter = fl::back_inserter(vec);
        inserter++ = 10;

        FL_CHECK(vec.size() == 1);
        FL_CHECK(vec[0] == 10);
    }

    FL_SUBCASE("Dereference") {
        auto inserter = fl::back_inserter(vec);
        *inserter = 10;
        *inserter = 20;

        FL_CHECK(vec.size() == 2);
        FL_CHECK(vec[0] == 10);
        FL_CHECK(vec[1] == 20);
    }
}

FL_TEST_CASE("back_inserter with FixedVector") {
    fl::FixedVector<int, 5> vec;

    FL_SUBCASE("Basic insertion") {
        auto inserter = fl::back_inserter(vec);
        *inserter = 10;
        *inserter = 20;
        *inserter = 30;

        FL_CHECK(vec.size() == 3);
        FL_CHECK(vec[0] == 10);
        FL_CHECK(vec[1] == 20);
        FL_CHECK(vec[2] == 30);
    }
}

FL_TEST_CASE("back_inserter with InlinedVector") {
    fl::InlinedVector<int, 3> vec;

    FL_SUBCASE("Basic insertion within inline capacity") {
        auto inserter = fl::back_inserter(vec);
        *inserter = 10;
        *inserter = 20;

        FL_CHECK(vec.size() == 2);
        FL_CHECK(vec[0] == 10);
        FL_CHECK(vec[1] == 20);
    }

    FL_SUBCASE("Insertion beyond inline capacity (heap allocation)") {
        auto inserter = fl::back_inserter(vec);
        *inserter = 10;
        *inserter = 20;
        *inserter = 30;
        *inserter = 40;

        FL_CHECK(vec.size() == 4);
        FL_CHECK(vec[0] == 10);
        FL_CHECK(vec[1] == 20);
        FL_CHECK(vec[2] == 30);
        FL_CHECK(vec[3] == 40);
    }
}

FL_TEST_CASE("back_inserter with move semantics") {
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

    FL_SUBCASE("Move insertion") {
        auto inserter = fl::back_inserter(vec);
        *inserter = MoveOnly(42);

        FL_CHECK(vec.size() == 1);
        FL_CHECK(vec[0].value == 42);
    }
}

// Test integration with algorithms
FL_TEST_CASE("back_inserter with algorithm integration") {
    fl::vector<int> source;
    source.push_back(1);
    source.push_back(2);
    source.push_back(3);

    fl::vector<int> dest;

    FL_SUBCASE("Manual copy using back_inserter") {
        auto inserter = fl::back_inserter(dest);
        for (auto it = source.begin(); it != source.end(); ++it) {
            *inserter++ = *it;
        }

        FL_CHECK(dest.size() == 3);
        FL_CHECK(dest[0] == 1);
        FL_CHECK(dest[1] == 2);
        FL_CHECK(dest[2] == 3);
    }
}
