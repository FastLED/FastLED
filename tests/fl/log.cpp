// ok cpp include
/// @file log.cpp
/// @brief Tests for logging, debug, and warning macros (fl/log.h)

#include "fl/log.h"
#include "fl/sketch_macros.h"
#include "test.h"
#include "fl/stl/cstring.h"
#include "fl/stl/strstream.h"

using namespace fl;

FL_TEST_CASE("fastled_file_offset - finds src/ prefix") {
    // Test case: Path with ".build/src/fl/dbg.h" should return "src/fl/dbg.h"
    const char* path1 = ".build/src/fl/dbg.h";
    const char* result1 = fastled_file_offset(path1);
    FL_CHECK(result1 == path1 + 7);  // Points to "src/fl/dbg.h"
    FL_CHECK(fl::strcmp(result1, "src/fl/dbg.h") == 0);

    // Test case: Path starting with "src/" should return unchanged
    const char* path2 = "src/platforms/esp32/led_strip.cpp";
    const char* result2 = fastled_file_offset(path2);
    FL_CHECK(result2 == path2);  // Points to start
    FL_CHECK(fl::strcmp(result2, "src/platforms/esp32/led_strip.cpp") == 0);

    // Test case: Nested build path should find first "src/"
    const char* path3 = "build/debug/src/fx/video.cpp";
    const char* result3 = fastled_file_offset(path3);
    FL_CHECK(result3 == path3 + 12);  // Points to "src/fx/video.cpp"
    FL_CHECK(fl::strcmp(result3, "src/fx/video.cpp") == 0);
}

FL_TEST_CASE("fastled_file_offset - fallback to last slash") {
    // Test case: No "src/" but has slashes - return after last slash
    const char* path = "foo/bar/blah.h";
    const char* result = fastled_file_offset(path);
    FL_CHECK(result == path + 8);  // Points to "blah.h"
    FL_CHECK(fl::strcmp(result, "blah.h") == 0);

    // Test case: Multiple slashes without "src/"
    const char* path2 = "include/fastled/core.h";
    const char* result2 = fastled_file_offset(path2);
    FL_CHECK(result2 == path2 + 16);  // Points to "core.h"
    FL_CHECK(fl::strcmp(result2, "core.h") == 0);
}

FL_TEST_CASE("fastled_file_offset - no slashes") {
    // Test case: No slashes at all - return original path
    const char* path = "simple.h";
    const char* result = fastled_file_offset(path);
    FL_CHECK(result == path);  // Points to original
    FL_CHECK(fl::strcmp(result, "simple.h") == 0);
}

FL_TEST_CASE("fastled_file_offset - edge cases") {
    // Test case: Empty string
    const char* empty = "";
    const char* result_empty = fastled_file_offset(empty);
    FL_CHECK(result_empty == empty);

    // Test case: Just a slash
    const char* slash = "/";
    const char* result_slash = fastled_file_offset(slash);
    FL_CHECK(result_slash == slash + 1);  // Points after the slash
    FL_CHECK(fl::strcmp(result_slash, "") == 0);

    // Test case: "src" without trailing slash (should not match)
    const char* no_slash = "buildsrcfile.cpp";
    const char* result_no_slash = fastled_file_offset(no_slash);
    FL_CHECK(result_no_slash == no_slash);  // No match, no slashes, returns original
    FL_CHECK(fl::strcmp(result_no_slash, "buildsrcfile.cpp") == 0);

    // Test case: Partial "sr" match (should not match)
    const char* partial = "foo/sr/bar.h";
    const char* result_partial = fastled_file_offset(partial);
    FL_CHECK(result_partial == partial + 7);  // Falls back to last slash
    FL_CHECK(fl::strcmp(result_partial, "bar.h") == 0);
}

FL_TEST_CASE("fastled_file_offset - Windows backslash paths") {
    // Test case: Windows path with backslashes and "src\\"
    const char* win_path1 = "build\\debug\\src\\fx\\video.cpp";
    const char* result1 = fastled_file_offset(win_path1);
    FL_CHECK(result1 == win_path1 + 12);  // Points to "src\\fx\\video.cpp"
    FL_CHECK(fl::strcmp(result1, "src\\fx\\video.cpp") == 0);

    // Test case: Windows absolute path with "src\\"
    const char* win_path2 = "C:\\Users\\test\\src\\file.cpp";
    const char* result2 = fastled_file_offset(win_path2);
    FL_CHECK(result2 == win_path2 + 14);  // Points to "src\\file.cpp"
    FL_CHECK(fl::strcmp(result2, "src\\file.cpp") == 0);

    // Test case: Path with backslash but no src (fallback to last backslash)
    const char* win_path3 = "foo\\bar\\baz.h";
    const char* result3 = fastled_file_offset(win_path3);
    FL_CHECK(result3 == win_path3 + 8);  // Points to "baz.h"
    FL_CHECK(fl::strcmp(result3, "baz.h") == 0);

    // Test case: Mixed forward slash and backslash with src/
    const char* mixed1 = "C:\\build\\src/file.cpp";
    const char* result_mixed1 = fastled_file_offset(mixed1);
    FL_CHECK(result_mixed1 == mixed1 + 9);  // Points to "src/file.cpp"
    FL_CHECK(fl::strcmp(result_mixed1, "src/file.cpp") == 0);

    // Test case: Mixed slashes without src (use last separator)
    const char* mixed2 = "foo\\bar/baz.h";
    const char* result_mixed2 = fastled_file_offset(mixed2);
    FL_CHECK(result_mixed2 == mixed2 + 8);  // Points to "baz.h"
    FL_CHECK(fl::strcmp(result_mixed2, "baz.h") == 0);
}

// ============================================================================
// Test Suite: FL_DBG Debug Macros (merged from dbg.cpp)
// ============================================================================

FL_TEST_CASE("FL_DBG macro compilation") {
    FL_SUBCASE("compiles with various types") {
        FL_DBG("Simple string");
        FL_DBG("Value: " << 42);
        FL_DBG("Float: " << 3.14f);
        FL_DBG("Multiple: " << 1 << " " << 2 << " " << 3);
        FL_CHECK(true);
    }

    FL_SUBCASE("FL_DBG_IF compiles with conditions") {
        bool condition = true;
        FL_DBG_IF(condition, "Conditional message");
        FL_DBG_IF(false, "Should not print");
        FL_DBG_IF(1 == 1, "True condition");
        FL_CHECK(true);
    }
}

FL_TEST_CASE("FASTLED_DBG macro compilation") {
    FL_SUBCASE("compiles with various types") {
        FASTLED_DBG("Simple string");
        FASTLED_DBG("Value: " << 42);
        FL_CHECK(true);
    }

    FL_SUBCASE("FASTLED_DBG_IF compiles with conditions") {
        FASTLED_DBG_IF(true, "Message");
        FASTLED_DBG_IF(false, "Should not print");
        FL_CHECK(true);
    }
}

FL_TEST_CASE("FL_DBG_NO_OP macro") {
    FL_SUBCASE("compiles without errors") {
        FL_DBG_NO_OP("Test message" << 42 << " more");
        FL_CHECK(true);
    }
}

FL_TEST_CASE("debug macro configuration") {
    FL_SUBCASE("FASTLED_HAS_DBG is defined") {
        #if defined(FASTLED_HAS_DBG)
            FL_CHECK((FASTLED_HAS_DBG == 0 || FASTLED_HAS_DBG == 1));
        #else
            FL_CHECK(false);
        #endif
    }

    FL_SUBCASE("FASTLED_FORCE_DBG behavior") {
        #if defined(FASTLED_FORCE_DBG)
            FL_CHECK(FASTLED_FORCE_DBG == 1);
        #endif
        FL_CHECK(true);
    }
}

// ============================================================================
// Test Suite: FL_WARN / FASTLED_WARN Warning Macros (merged from warn.cpp)
// ============================================================================

FL_TEST_CASE("FL_WARN macros are defined") {
    FL_SUBCASE("FASTLED_WARN is defined") {
        #ifdef FASTLED_WARN
        FL_CHECK(true);
        #else
        FL_CHECK(false);
        #endif
    }

    FL_SUBCASE("FASTLED_WARN_IF is defined") {
        #ifdef FASTLED_WARN_IF
        FL_CHECK(true);
        #else
        FL_CHECK(false);
        #endif
    }

    FL_SUBCASE("FL_WARN is defined") {
        #ifdef FL_WARN
        FL_CHECK(true);
        #else
        FL_CHECK(false);
        #endif
    }

    FL_SUBCASE("FL_WARN_IF is defined") {
        #ifdef FL_WARN_IF
        FL_CHECK(true);
        #else
        FL_CHECK(false);
        #endif
    }
}

FL_TEST_CASE("FL_WARN macro compiles and executes") {
    FL_SUBCASE("FL_WARN with string literal") {
        FL_WARN("Test warning message");
        FL_CHECK(true);
    }

    FL_SUBCASE("FL_WARN with stream expression") {
        int value = 42;
        FL_WARN("Value: " << value);
        FL_CHECK(true);
    }

    FL_SUBCASE("FL_WARN with multiple expressions") {
        int x = 10;
        int y = 20;
        FL_WARN("x=" << x << ", y=" << y);
        FL_CHECK(true);
    }

    FL_SUBCASE("FL_WARN in conditional block") {
        if (true) {
            FL_WARN("Inside conditional");
        }
        FL_CHECK(true);
    }

    FL_SUBCASE("FL_WARN in loop") {
        for (int i = 0; i < 3; i++) {
            if (i == 1) {
                FL_WARN("Warning in loop iteration " << i);
            }
        }
        FL_CHECK(true);
    }
}

FL_TEST_CASE("FL_WARN_IF macro compiles and executes") {
    FL_SUBCASE("FL_WARN_IF with true condition") {
        FL_WARN_IF(true, "Condition is true");
        FL_CHECK(true);
    }

    FL_SUBCASE("FL_WARN_IF with false condition") {
        FL_WARN_IF(false, "Condition is false");
        FL_CHECK(true);
    }

    FL_SUBCASE("FL_WARN_IF with expression condition") {
        int value = 10;
        FL_WARN_IF(value > 5, "Value is greater than 5");
        FL_CHECK(true);
    }

    FL_SUBCASE("FL_WARN_IF with stream expression") {
        int value = 42;
        FL_WARN_IF(value != 0, "Non-zero value: " << value);
        FL_CHECK(true);
    }

    FL_SUBCASE("FL_WARN_IF in nested conditions") {
        bool flag = true;
        if (flag) {
            FL_WARN_IF(flag, "Flag is set");
        }
        FL_CHECK(true);
    }
}

FL_TEST_CASE("FASTLED_WARN macro compiles and executes") {
    FL_SUBCASE("FASTLED_WARN with string literal") {
        FASTLED_WARN("Test FASTLED warning");
        FL_CHECK(true);
    }

    FL_SUBCASE("FASTLED_WARN with stream expression") {
        int value = 99;
        FASTLED_WARN("FASTLED value: " << value);
        FL_CHECK(true);
    }
}

FL_TEST_CASE("FASTLED_WARN_IF macro compiles and executes") {
    FL_SUBCASE("FASTLED_WARN_IF with true condition") {
        FASTLED_WARN_IF(true, "FASTLED condition is true");
        FL_CHECK(true);
    }

    FL_SUBCASE("FASTLED_WARN_IF with false condition") {
        FASTLED_WARN_IF(false, "FASTLED condition is false");
        FL_CHECK(true);
    }
}

FL_TEST_CASE("warning macros are safe in all contexts") {
    FL_SUBCASE("multiple FL_WARN calls in sequence") {
        FL_WARN("First warning");
        FL_WARN("Second warning");
        FL_WARN("Third warning");
        FL_CHECK(true);
    }

    FL_SUBCASE("mixed FASTLED_WARN and FL_WARN") {
        FASTLED_WARN("FASTLED message");
        FL_WARN("FL message");
        FL_CHECK(true);
    }

    FL_SUBCASE("FL_WARN in function") {
        auto warn_func = []() {
            FL_WARN("Warning from lambda");
        };
        warn_func();
        FL_CHECK(true);
    }

    FL_SUBCASE("FL_WARN_IF with side-effect-free condition") {
        int counter = 0;
        FL_WARN_IF(counter == 0, "Counter is zero");
        FL_CHECK_EQ(counter, 0);
    }
}

FL_TEST_CASE("warning macro behavior on memory-constrained platforms") {
    FL_SUBCASE("FL_WARN compiles on constrained platforms") {
        #if !SKETCH_HAS_LOTS_OF_MEMORY
        FL_WARN("This should be a no-op");
        #else
        FL_WARN("This should output");
        #endif
        FL_CHECK(true);
    }

    FL_SUBCASE("FL_WARN_IF compiles on constrained platforms") {
        #if !SKETCH_HAS_LOTS_OF_MEMORY
        FL_WARN_IF(true, "This should be a no-op");
        #else
        FL_WARN_IF(true, "This should output");
        #endif
        FL_CHECK(true);
    }
}

FL_TEST_CASE("warning macros with complex expressions") {
    FL_SUBCASE("FL_WARN with arithmetic expression") {
        int a = 10, b = 20;
        FL_WARN("Sum: " << (a + b) << ", Product: " << (a * b));
        FL_CHECK(true);
    }

    FL_SUBCASE("FL_WARN with string concatenation") {
        const char* prefix = "Warning: ";
        const char* message = "something happened";
        FL_WARN(prefix << message);
        FL_CHECK(true);
    }

    FL_SUBCASE("FL_WARN_IF with compound boolean expression") {
        int x = 5, y = 10;
        FL_WARN_IF(x < y && y > 0, "Both conditions met");
        FL_CHECK(true);
    }

    FL_SUBCASE("FL_WARN with multiple values") {
        int value1 = 255;
        int value2 = 100;
        FL_WARN("Value1: " << value1 << ", Value2: " << value2);
        FL_CHECK(true);
    }
}

FL_TEST_CASE("warning macros do not interfere with control flow") {
    FL_SUBCASE("FL_WARN does not break if-else chain") {
        int result = 0;
        if (false) {
            result = 1;
        } else {
            FL_WARN("In else block");
            result = 2;
        }
        FL_CHECK_EQ(result, 2);
    }

    FL_SUBCASE("FL_WARN_IF does not affect loop iteration") {
        int count = 0;
        for (int i = 0; i < 5; i++) {
            FL_WARN_IF(i == 2, "Iteration 2");
            count++;
        }
        FL_CHECK_EQ(count, 5);
    }

    FL_SUBCASE("FL_WARN in switch statement") {
        int value = 2;
        int result = 0;
        switch (value) {
            case 1:
                result = 1;
                break;
            case 2:
                FL_WARN("Case 2");
                result = 2;
                break;
            default:
                result = 0;
                break;
        }
        FL_CHECK_EQ(result, 2);
    }
}

FL_TEST_CASE("warning macro edge cases") {
    FL_SUBCASE("empty message (should still compile)") {
        FL_WARN("");
        FL_CHECK(true);
    }

    FL_SUBCASE("very long message") {
        FL_WARN("This is a very long warning message that contains a lot of text "
                "and spans multiple lines in the source code but is still just "
                "one continuous string literal for testing purposes");
        FL_CHECK(true);
    }

    FL_SUBCASE("FL_WARN_IF with always-false condition") {
        FL_WARN_IF(false, "Never shown");
        FL_CHECK(true);
    }

    FL_SUBCASE("FL_WARN_IF with compile-time constant") {
        constexpr bool always_true = true;
        FL_WARN_IF(always_true, "Constexpr condition");
        FL_CHECK(true);
    }
}

// Grouped tests
#include "trace.hpp"
