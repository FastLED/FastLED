
// g++ --std=c++11 test.cpp

#include "test.h"

#include "test.h"
#include "fl/dbg.h"

#include "fl/namespace.h"

using namespace fl;

TEST_CASE("fastled_file_offset") {
    const char* src = "src/fl/dbg.h";
    const char* result = fastled_file_offset(src);
    CHECK(strcmp(result, "src/fl/dbg.h") == 0);
    const char* src2 = "blah/blah/blah.h";
    const char* result2 = fastled_file_offset(src2);
    CHECK(strcmp(result2, "blah.h") == 0);
}
