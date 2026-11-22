
#include "fl/stl/vector.h"
#include "test.h"

using namespace fl;

typedef SortedHeapVector<int> SHV;

FL_TEST_CASE("fl::insert_result enum values") {
    FL_SUBCASE("inserted value") {
        FL_CHECK_EQ(static_cast<int>(SHV::inserted), 0);
    }

    FL_SUBCASE("exists value") {
        FL_CHECK_EQ(static_cast<int>(SHV::exists), 1);
    }

    FL_SUBCASE("max_size value") {
        FL_CHECK_EQ(static_cast<int>(SHV::at_capacity), 2);
    }

    FL_SUBCASE("all values are distinct") {
        FL_CHECK(SHV::inserted != SHV::exists);
        FL_CHECK(SHV::inserted != SHV::at_capacity);
        FL_CHECK(SHV::exists != SHV::at_capacity);
    }
}

FL_TEST_CASE("fl::insert_result usage patterns") {
    FL_SUBCASE("assignment and comparison") {
        SHV::insert_result r1 = SHV::inserted;
        SHV::insert_result r2 = SHV::inserted;
        SHV::insert_result r3 = SHV::exists;

        FL_CHECK(r1 == r2);
        FL_CHECK(r1 != r3);
    }

    FL_SUBCASE("switch statement") {
        SHV::insert_result result = SHV::exists;
        int outcome = 0;

        switch(result) {
            case SHV::inserted: outcome = 1; break;
            case SHV::exists: outcome = 2; break;
            case SHV::at_capacity: outcome = 3; break;
        }

        FL_CHECK_EQ(outcome, 2);
    }

    FL_SUBCASE("conditional checks") {
        SHV::insert_result result = SHV::inserted;
        FL_CHECK(result == SHV::inserted);
        FL_CHECK(result != SHV::exists);
        FL_CHECK(result != SHV::at_capacity);
    }
}
