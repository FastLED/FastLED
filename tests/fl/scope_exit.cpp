// ok cpp include
/// @file scope_exit.cpp
/// @brief Tests for fl::scope_exit (P0052-style scope guard)

#include "test.h"
#include "fl/scope_exit.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

FL_TEST_CASE("scope_exit - basic execution") {
    int count = 0;
    {
        auto guard = make_scope_exit([&] { ++count; });
        FL_CHECK_EQ(count, 0);
    }
    FL_CHECK_EQ(count, 1);
}

FL_TEST_CASE("scope_exit - release prevents execution") {
    int count = 0;
    {
        auto guard = make_scope_exit([&] { ++count; });
        guard.release();
    }
    FL_CHECK_EQ(count, 0);
}

FL_TEST_CASE("scope_exit - move transfers ownership") {
    int count = 0;
    {
        auto guard1 = make_scope_exit([&] { ++count; });
        auto guard2 = fl::move(guard1);
        // guard1 moved-from — should not fire
    }
    // guard2 fires once
    FL_CHECK_EQ(count, 1);
}

FL_TEST_CASE("scope_exit - multiple guards run in reverse order") {
    fl::string order;
    {
        auto g1 = make_scope_exit([&] { order += "1"; });
        auto g2 = make_scope_exit([&] { order += "2"; });
        auto g3 = make_scope_exit([&] { order += "3"; });
    }
    // Destructors run in reverse declaration order
    FL_CHECK_EQ(order, fl::string("321"));
}

} // FL_TEST_FILE
