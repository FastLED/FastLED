#include "fl/insert_result.h"
#include "test.h"

using namespace fl;

FL_TEST_CASE("fl::InsertResult enum values") {
    FL_SUBCASE("kInserted value") {
        FL_CHECK_EQ(static_cast<int>(kInserted), 0);
        FL_CHECK_EQ(static_cast<int>(InsertResult::kInserted), 0);
    }

    FL_SUBCASE("kExists value") {
        FL_CHECK_EQ(static_cast<int>(kExists), 1);
        FL_CHECK_EQ(static_cast<int>(InsertResult::kExists), 1);
    }

    FL_SUBCASE("kMaxSize value") {
        FL_CHECK_EQ(static_cast<int>(kMaxSize), 2);
        FL_CHECK_EQ(static_cast<int>(InsertResult::kMaxSize), 2);
    }

    FL_SUBCASE("all values are distinct") {
        FL_CHECK(kInserted != kExists);
        FL_CHECK(kInserted != kMaxSize);
        FL_CHECK(kExists != kMaxSize);
    }

    FL_SUBCASE("values are sequential") {
        FL_CHECK_EQ(static_cast<int>(kInserted), 0);
        FL_CHECK_EQ(static_cast<int>(kExists), 1);
        FL_CHECK_EQ(static_cast<int>(kMaxSize), 2);
    }
}

FL_TEST_CASE("fl::InsertResult usage patterns") {
    FL_SUBCASE("assignment and comparison") {
        InsertResult result1 = kInserted;
        InsertResult result2 = kInserted;
        InsertResult result3 = kExists;

        FL_CHECK(result1 == result2);
        FL_CHECK(result1 != result3);
        FL_CHECK(result2 != result3);
    }

    FL_SUBCASE("switch statement") {
        InsertResult result = kExists;
        int outcome = 0;

        switch(result) {
            case kInserted: outcome = 1; break;
            case kExists: outcome = 2; break;
            case kMaxSize: outcome = 3; break;
        }

        FL_CHECK_EQ(outcome, 2);
    }

    FL_SUBCASE("conditional checks") {
        InsertResult result = kInserted;
        FL_CHECK(result == kInserted);
        FL_CHECK(result != kExists);
        FL_CHECK(result != kMaxSize);

        result = kMaxSize;
        FL_CHECK(result == kMaxSize);
        FL_CHECK(result != kInserted);
        FL_CHECK(result != kExists);
    }
}

FL_TEST_CASE("fl::InsertResult semantic meaning") {
    FL_SUBCASE("success case - kInserted") {
        // kInserted means the item was successfully inserted
        InsertResult result = kInserted;
        bool success = (result == kInserted);
        FL_CHECK(success);
    }

    FL_SUBCASE("already exists case - kExists") {
        // kExists means the item already existed in the container
        InsertResult result = kExists;
        bool already_present = (result == kExists);
        FL_CHECK(already_present);
    }

    FL_SUBCASE("container full case - kMaxSize") {
        // kMaxSize means the container was at max capacity
        InsertResult result = kMaxSize;
        bool container_full = (result == kMaxSize);
        FL_CHECK(container_full);
    }
}

FL_TEST_CASE("fl::InsertResult boolean conversion patterns") {
    FL_SUBCASE("success check pattern") {
        auto check_success = [](InsertResult result) -> bool {
            return result == kInserted;
        };

        FL_CHECK(check_success(kInserted) == true);
        FL_CHECK(check_success(kExists) == false);
        FL_CHECK(check_success(kMaxSize) == false);
    }

    FL_SUBCASE("failure check pattern") {
        auto check_failure = [](InsertResult result) -> bool {
            return result != kInserted;
        };

        FL_CHECK(check_failure(kInserted) == false);
        FL_CHECK(check_failure(kExists) == true);
        FL_CHECK(check_failure(kMaxSize) == true);
    }

    FL_SUBCASE("specific failure type checks") {
        auto is_duplicate = [](InsertResult result) -> bool {
            return result == kExists;
        };

        auto is_full = [](InsertResult result) -> bool {
            return result == kMaxSize;
        };

        InsertResult r1 = kExists;
        FL_CHECK(is_duplicate(r1) == true);
        FL_CHECK(is_full(r1) == false);

        InsertResult r2 = kMaxSize;
        FL_CHECK(is_duplicate(r2) == false);
        FL_CHECK(is_full(r2) == true);
    }
}

FL_TEST_CASE("fl::InsertResult array indexing") {
    FL_SUBCASE("use as array index") {
        // The sequential nature allows using as array index
        const char* messages[] = {
            "Inserted successfully",
            "Item already exists",
            "Container is at max size"
        };

        FL_CHECK_EQ(messages[kInserted], "Inserted successfully");
        FL_CHECK_EQ(messages[kExists], "Item already exists");
        FL_CHECK_EQ(messages[kMaxSize], "Container is at max size");
    }
}
