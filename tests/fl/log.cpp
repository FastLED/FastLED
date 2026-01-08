/// @file log.cpp
/// @brief Test for logging utility functions

#include "fl/log.h"
#include "doctest.h"
#include "fl/stl/cstring.h"

using namespace fl;

TEST_CASE("fastled_file_offset - finds src/ prefix") {
    // Test case: Path with ".build/src/fl/dbg.h" should return "src/fl/dbg.h"
    const char* path1 = ".build/src/fl/dbg.h";
    const char* result1 = fastled_file_offset(path1);
    CHECK(result1 == path1 + 7);  // Points to "src/fl/dbg.h"
    CHECK(fl::strcmp(result1, "src/fl/dbg.h") == 0);

    // Test case: Path starting with "src/" should return unchanged
    const char* path2 = "src/platforms/esp32/led_strip.cpp";
    const char* result2 = fastled_file_offset(path2);
    CHECK(result2 == path2);  // Points to start
    CHECK(fl::strcmp(result2, "src/platforms/esp32/led_strip.cpp") == 0);

    // Test case: Nested build path should find first "src/"
    const char* path3 = "build/debug/src/fx/video.cpp";
    const char* result3 = fastled_file_offset(path3);
    CHECK(result3 == path3 + 12);  // Points to "src/fx/video.cpp"
    CHECK(fl::strcmp(result3, "src/fx/video.cpp") == 0);
}

TEST_CASE("fastled_file_offset - fallback to last slash") {
    // Test case: No "src/" but has slashes - return after last slash
    const char* path = "foo/bar/blah.h";
    const char* result = fastled_file_offset(path);
    CHECK(result == path + 8);  // Points to "blah.h"
    CHECK(fl::strcmp(result, "blah.h") == 0);

    // Test case: Multiple slashes without "src/"
    const char* path2 = "include/fastled/core.h";
    const char* result2 = fastled_file_offset(path2);
    CHECK(result2 == path2 + 16);  // Points to "core.h"
    CHECK(fl::strcmp(result2, "core.h") == 0);
}

TEST_CASE("fastled_file_offset - no slashes") {
    // Test case: No slashes at all - return original path
    const char* path = "simple.h";
    const char* result = fastled_file_offset(path);
    CHECK(result == path);  // Points to original
    CHECK(fl::strcmp(result, "simple.h") == 0);
}

TEST_CASE("fastled_file_offset - edge cases") {
    // Test case: Empty string
    const char* empty = "";
    const char* result_empty = fastled_file_offset(empty);
    CHECK(result_empty == empty);

    // Test case: Just a slash
    const char* slash = "/";
    const char* result_slash = fastled_file_offset(slash);
    CHECK(result_slash == slash + 1);  // Points after the slash
    CHECK(fl::strcmp(result_slash, "") == 0);

    // Test case: "src" without trailing slash (should not match)
    const char* no_slash = "buildsrcfile.cpp";
    const char* result_no_slash = fastled_file_offset(no_slash);
    CHECK(result_no_slash == no_slash);  // No match, no slashes, returns original
    CHECK(fl::strcmp(result_no_slash, "buildsrcfile.cpp") == 0);

    // Test case: Partial "sr" match (should not match)
    const char* partial = "foo/sr/bar.h";
    const char* result_partial = fastled_file_offset(partial);
    CHECK(result_partial == partial + 7);  // Falls back to last slash
    CHECK(fl::strcmp(result_partial, "bar.h") == 0);
}

TEST_CASE("fastled_file_offset - Windows backslash paths") {
    // Test case: Windows path with backslashes and "src\\"
    const char* win_path1 = "build\\debug\\src\\fx\\video.cpp";
    const char* result1 = fastled_file_offset(win_path1);
    CHECK(result1 == win_path1 + 12);  // Points to "src\\fx\\video.cpp"
    CHECK(fl::strcmp(result1, "src\\fx\\video.cpp") == 0);

    // Test case: Windows absolute path with "src\\"
    const char* win_path2 = "C:\\Users\\test\\src\\file.cpp";
    const char* result2 = fastled_file_offset(win_path2);
    CHECK(result2 == win_path2 + 14);  // Points to "src\\file.cpp"
    CHECK(fl::strcmp(result2, "src\\file.cpp") == 0);

    // Test case: Path with backslash but no src (fallback to last backslash)
    const char* win_path3 = "foo\\bar\\baz.h";
    const char* result3 = fastled_file_offset(win_path3);
    CHECK(result3 == win_path3 + 8);  // Points to "baz.h"
    CHECK(fl::strcmp(result3, "baz.h") == 0);

    // Test case: Mixed forward slash and backslash with src/
    const char* mixed1 = "C:\\build\\src/file.cpp";
    const char* result_mixed1 = fastled_file_offset(mixed1);
    CHECK(result_mixed1 == mixed1 + 9);  // Points to "src/file.cpp"
    CHECK(fl::strcmp(result_mixed1, "src/file.cpp") == 0);

    // Test case: Mixed slashes without src (use last separator)
    const char* mixed2 = "foo\\bar/baz.h";
    const char* result_mixed2 = fastled_file_offset(mixed2);
    CHECK(result_mixed2 == mixed2 + 8);  // Points to "baz.h"
    CHECK(fl::strcmp(result_mixed2, "baz.h") == 0);
}
