
// g++ --std=c++11 test.cpp

#include "test.h"

#include "test.h"
#include "fl/dbg.h"

TEST_CASE("fastled_file_offset") {
    const char* src = "src/fl/dbg.h";
    const char* result = fastled_file_offset(src);
    CHECK(fl::strcmp(result, "src/fl/dbg.h") == 0);
    const char* src2 = "blah/blah/blah.h";
    const char* result2 = fastled_file_offset(src2);
    CHECK(fl::strcmp(result2, "blah.h") == 0);
}
