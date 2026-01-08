#include "fl/insert_result.h"
#include "doctest.h"

using namespace fl;

TEST_CASE("fl::InsertResult enum values") {
    SUBCASE("kInserted value") {
        CHECK_EQ(static_cast<int>(kInserted), 0);
        CHECK_EQ(static_cast<int>(InsertResult::kInserted), 0);
    }

    SUBCASE("kExists value") {
        CHECK_EQ(static_cast<int>(kExists), 1);
        CHECK_EQ(static_cast<int>(InsertResult::kExists), 1);
    }

    SUBCASE("kMaxSize value") {
        CHECK_EQ(static_cast<int>(kMaxSize), 2);
        CHECK_EQ(static_cast<int>(InsertResult::kMaxSize), 2);
    }

    SUBCASE("all values are distinct") {
        CHECK(kInserted != kExists);
        CHECK(kInserted != kMaxSize);
        CHECK(kExists != kMaxSize);
    }

    SUBCASE("values are sequential") {
        CHECK_EQ(static_cast<int>(kInserted), 0);
        CHECK_EQ(static_cast<int>(kExists), 1);
        CHECK_EQ(static_cast<int>(kMaxSize), 2);
    }
}

TEST_CASE("fl::InsertResult usage patterns") {
    SUBCASE("assignment and comparison") {
        InsertResult result1 = kInserted;
        InsertResult result2 = kInserted;
        InsertResult result3 = kExists;

        CHECK(result1 == result2);
        CHECK(result1 != result3);
        CHECK(result2 != result3);
    }

    SUBCASE("switch statement") {
        InsertResult result = kExists;
        int outcome = 0;

        switch(result) {
            case kInserted: outcome = 1; break;
            case kExists: outcome = 2; break;
            case kMaxSize: outcome = 3; break;
        }

        CHECK_EQ(outcome, 2);
    }

    SUBCASE("conditional checks") {
        InsertResult result = kInserted;
        CHECK(result == kInserted);
        CHECK(result != kExists);
        CHECK(result != kMaxSize);

        result = kMaxSize;
        CHECK(result == kMaxSize);
        CHECK(result != kInserted);
        CHECK(result != kExists);
    }
}

TEST_CASE("fl::InsertResult semantic meaning") {
    SUBCASE("success case - kInserted") {
        // kInserted means the item was successfully inserted
        InsertResult result = kInserted;
        bool success = (result == kInserted);
        CHECK(success);
    }

    SUBCASE("already exists case - kExists") {
        // kExists means the item already existed in the container
        InsertResult result = kExists;
        bool already_present = (result == kExists);
        CHECK(already_present);
    }

    SUBCASE("container full case - kMaxSize") {
        // kMaxSize means the container was at max capacity
        InsertResult result = kMaxSize;
        bool container_full = (result == kMaxSize);
        CHECK(container_full);
    }
}

TEST_CASE("fl::InsertResult boolean conversion patterns") {
    SUBCASE("success check pattern") {
        auto check_success = [](InsertResult result) -> bool {
            return result == kInserted;
        };

        CHECK(check_success(kInserted) == true);
        CHECK(check_success(kExists) == false);
        CHECK(check_success(kMaxSize) == false);
    }

    SUBCASE("failure check pattern") {
        auto check_failure = [](InsertResult result) -> bool {
            return result != kInserted;
        };

        CHECK(check_failure(kInserted) == false);
        CHECK(check_failure(kExists) == true);
        CHECK(check_failure(kMaxSize) == true);
    }

    SUBCASE("specific failure type checks") {
        auto is_duplicate = [](InsertResult result) -> bool {
            return result == kExists;
        };

        auto is_full = [](InsertResult result) -> bool {
            return result == kMaxSize;
        };

        InsertResult r1 = kExists;
        CHECK(is_duplicate(r1) == true);
        CHECK(is_full(r1) == false);

        InsertResult r2 = kMaxSize;
        CHECK(is_duplicate(r2) == false);
        CHECK(is_full(r2) == true);
    }
}

TEST_CASE("fl::InsertResult array indexing") {
    SUBCASE("use as array index") {
        // The sequential nature allows using as array index
        const char* messages[] = {
            "Inserted successfully",
            "Item already exists",
            "Container is at max size"
        };

        CHECK_EQ(messages[kInserted], "Inserted successfully");
        CHECK_EQ(messages[kExists], "Item already exists");
        CHECK_EQ(messages[kMaxSize], "Container is at max size");
    }
}
