#include "doctest.h"
#include "fl/log.h"
#include "fl/stl/cstring.h"
#include "fl/stl/strstream.h"

using namespace fl;

TEST_CASE("fl::fastled_file_offset") {
    SUBCASE("extracts path after 'src/'") {
        const char* path1 = "some/path/src/fl/dbg.h";
        CHECK_EQ(fl::strcmp(fastled_file_offset(path1), "src/fl/dbg.h"), 0);

        const char* path2 = ".build/src/fl/math.h";
        CHECK_EQ(fl::strcmp(fastled_file_offset(path2), "src/fl/math.h"), 0);

        const char* path3 = "/home/user/project/src/test.cpp";
        CHECK_EQ(fl::strcmp(fastled_file_offset(path3), "src/test.cpp"), 0);
    }

    SUBCASE("falls back to last slash when no 'src/' found") {
        const char* path1 = "include/fl/header.h";
        CHECK_EQ(fl::strcmp(fastled_file_offset(path1), "header.h"), 0);

        const char* path2 = "path/to/file.cpp";
        CHECK_EQ(fl::strcmp(fastled_file_offset(path2), "file.cpp"), 0);

        const char* path3 = "a/b/c/d/e.h";
        CHECK_EQ(fl::strcmp(fastled_file_offset(path3), "e.h"), 0);
    }

    SUBCASE("returns original path when no slashes found") {
        const char* path1 = "simple.h";
        CHECK_EQ(fl::strcmp(fastled_file_offset(path1), "simple.h"), 0);

        const char* path2 = "test.cpp";
        CHECK_EQ(fl::strcmp(fastled_file_offset(path2), "test.cpp"), 0);
    }

    SUBCASE("handles edge cases") {
        // Empty string
        const char* empty = "";
        CHECK_EQ(fl::strcmp(fastled_file_offset(empty), ""), 0);

        // Single slash
        const char* single_slash = "/";
        CHECK_EQ(fl::strcmp(fastled_file_offset(single_slash), ""), 0);

        // Path ending with slash
        const char* ending_slash = "path/to/dir/";
        CHECK_EQ(fl::strcmp(fastled_file_offset(ending_slash), ""), 0);

        // Multiple src/ occurrences - should use first
        const char* multi_src = "other/src/fl/src/test.h";
        CHECK_EQ(fl::strcmp(fastled_file_offset(multi_src), "src/fl/src/test.h"), 0);
    }

    SUBCASE("handles paths with 'src' but not 'src/'") {
        const char* path1 = "resource/file.h";
        // Should not match "src" within "resource", falls back to last slash
        CHECK_EQ(fl::strcmp(fastled_file_offset(path1), "file.h"), 0);

        const char* path2 = "source/code.cpp";
        // Should not match "src" within "source", falls back to last slash
        CHECK_EQ(fl::strcmp(fastled_file_offset(path2), "code.cpp"), 0);
    }
}

TEST_CASE("FL_DBG macro compilation") {
    // Test that FL_DBG macro compiles correctly
    // We can't easily test its runtime behavior since it outputs to println()
    // But we can verify it compiles with various types

    SUBCASE("compiles with various types") {
        // These should compile without errors
        FL_DBG("Simple string");
        FL_DBG("Value: " << 42);
        FL_DBG("Float: " << 3.14f);
        FL_DBG("Multiple: " << 1 << " " << 2 << " " << 3);

        // Verify code compiles
        CHECK(true);
    }

    SUBCASE("FL_DBG_IF compiles with conditions") {
        bool condition = true;
        FL_DBG_IF(condition, "Conditional message");
        FL_DBG_IF(false, "Should not print");
        FL_DBG_IF(1 == 1, "True condition");

        // Verify code compiles
        CHECK(true);
    }
}

TEST_CASE("FASTLED_DBG macro compilation") {
    SUBCASE("compiles with various types") {
        // Test the long-form macro name
        FASTLED_DBG("Simple string");
        FASTLED_DBG("Value: " << 42);

        // Verify code compiles
        CHECK(true);
    }

    SUBCASE("FASTLED_DBG_IF compiles with conditions") {
        FASTLED_DBG_IF(true, "Message");
        FASTLED_DBG_IF(false, "Should not print");

        // Verify code compiles
        CHECK(true);
    }
}

TEST_CASE("FL_DBG_NO_OP macro") {
    // Test that the no-op macro compiles and doesn't execute
    SUBCASE("compiles without errors") {
        // This should compile but not execute the stream operations
        FL_DBG_NO_OP("Test message" << 42 << " more");

        // Verify code compiles
        CHECK(true);
    }
}

TEST_CASE("debug macro configuration") {
    // Test that macro configuration constants are defined
    SUBCASE("FASTLED_HAS_DBG is defined") {
        #if defined(FASTLED_HAS_DBG)
            // Either 0 or 1, but should be defined
            CHECK((FASTLED_HAS_DBG == 0 || FASTLED_HAS_DBG == 1));
        #else
            // Should always be defined by the header
            CHECK(false);
        #endif
    }

    SUBCASE("FASTLED_FORCE_DBG behavior") {
        #if defined(FASTLED_FORCE_DBG)
            // If forced, it should be enabled
            CHECK(FASTLED_FORCE_DBG == 1);
        #endif

        // Test always passes to verify conditional compilation
        CHECK(true);
    }
}
