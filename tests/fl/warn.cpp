#include "doctest.h"
#include "fl/log.h"
#include "fl/sketch_macros.h"
#include "fl/stl/strstream.h"

using namespace fl;

TEST_CASE("FL_WARN macros are defined") {
    SUBCASE("FASTLED_WARN is defined") {
        // FASTLED_WARN should be defined
        #ifdef FASTLED_WARN
        CHECK(true);
        #else
        CHECK(false);
        #endif
    }

    SUBCASE("FASTLED_WARN_IF is defined") {
        // FASTLED_WARN_IF should be defined
        #ifdef FASTLED_WARN_IF
        CHECK(true);
        #else
        CHECK(false);
        #endif
    }

    SUBCASE("FL_WARN is defined") {
        // FL_WARN should be defined
        #ifdef FL_WARN
        CHECK(true);
        #else
        CHECK(false);
        #endif
    }

    SUBCASE("FL_WARN_IF is defined") {
        // FL_WARN_IF should be defined
        #ifdef FL_WARN_IF
        CHECK(true);
        #else
        CHECK(false);
        #endif
    }
}

TEST_CASE("FL_WARN macro compiles and executes") {
    SUBCASE("FL_WARN with string literal") {
        // Should compile without errors
        FL_WARN("Test warning message");
        CHECK(true);
    }

    SUBCASE("FL_WARN with stream expression") {
        // Should compile with stream-style output
        int value = 42;
        FL_WARN("Value: " << value);
        CHECK(true);
    }

    SUBCASE("FL_WARN with multiple expressions") {
        // Should compile with multiple stream operations
        int x = 10;
        int y = 20;
        FL_WARN("x=" << x << ", y=" << y);
        CHECK(true);
    }

    SUBCASE("FL_WARN in conditional block") {
        // Should work in if statement
        if (true) {
            FL_WARN("Inside conditional");
        }
        CHECK(true);
    }

    SUBCASE("FL_WARN in loop") {
        // Should work in loop (but only warn once in real usage)
        for (int i = 0; i < 3; i++) {
            if (i == 1) {
                FL_WARN("Warning in loop iteration " << i);
            }
        }
        CHECK(true);
    }
}

TEST_CASE("FL_WARN_IF macro compiles and executes") {
    SUBCASE("FL_WARN_IF with true condition") {
        // Should execute warning when condition is true
        FL_WARN_IF(true, "Condition is true");
        CHECK(true);
    }

    SUBCASE("FL_WARN_IF with false condition") {
        // Should not execute warning when condition is false
        FL_WARN_IF(false, "Condition is false");
        CHECK(true);
    }

    SUBCASE("FL_WARN_IF with expression condition") {
        // Should work with complex conditions
        int value = 10;
        FL_WARN_IF(value > 5, "Value is greater than 5");
        CHECK(true);
    }

    SUBCASE("FL_WARN_IF with stream expression") {
        // Should work with stream-style message
        int value = 42;
        FL_WARN_IF(value != 0, "Non-zero value: " << value);
        CHECK(true);
    }

    SUBCASE("FL_WARN_IF in nested conditions") {
        // Should work in complex control flow
        bool flag = true;
        if (flag) {
            FL_WARN_IF(flag, "Flag is set");
        }
        CHECK(true);
    }
}

TEST_CASE("FASTLED_WARN macro compiles and executes") {
    SUBCASE("FASTLED_WARN with string literal") {
        // Should compile without errors
        FASTLED_WARN("Test FASTLED warning");
        CHECK(true);
    }

    SUBCASE("FASTLED_WARN with stream expression") {
        // Should compile with stream-style output
        int value = 99;
        FASTLED_WARN("FASTLED value: " << value);
        CHECK(true);
    }
}

TEST_CASE("FASTLED_WARN_IF macro compiles and executes") {
    SUBCASE("FASTLED_WARN_IF with true condition") {
        // Should execute warning when condition is true
        FASTLED_WARN_IF(true, "FASTLED condition is true");
        CHECK(true);
    }

    SUBCASE("FASTLED_WARN_IF with false condition") {
        // Should not execute warning when condition is false
        FASTLED_WARN_IF(false, "FASTLED condition is false");
        CHECK(true);
    }
}

TEST_CASE("warning macros are safe in all contexts") {
    SUBCASE("multiple FL_WARN calls in sequence") {
        // Should handle multiple warnings without issues
        FL_WARN("First warning");
        FL_WARN("Second warning");
        FL_WARN("Third warning");
        CHECK(true);
    }

    SUBCASE("mixed FASTLED_WARN and FL_WARN") {
        // Should work together
        FASTLED_WARN("FASTLED message");
        FL_WARN("FL message");
        CHECK(true);
    }

    SUBCASE("FL_WARN in function") {
        // Should work when called from function
        auto warn_func = []() {
            FL_WARN("Warning from lambda");
        };
        warn_func();
        CHECK(true);
    }

    SUBCASE("FL_WARN_IF with side-effect-free condition") {
        // Condition should not cause side effects
        int counter = 0;
        FL_WARN_IF(counter == 0, "Counter is zero");
        CHECK_EQ(counter, 0);
    }
}

TEST_CASE("warning macro behavior on memory-constrained platforms") {
    SUBCASE("FL_WARN compiles on constrained platforms") {
        // On memory-constrained platforms (SKETCH_HAS_LOTS_OF_MEMORY == false),
        // FL_WARN expands to a no-op do-while loop
        // This test verifies the macro compiles correctly
        #if !SKETCH_HAS_LOTS_OF_MEMORY
        // This is the no-op version
        FL_WARN("This should be a no-op");
        #else
        // This is the active version (uses FASTLED_WARN)
        FL_WARN("This should output");
        #endif
        CHECK(true);
    }

    SUBCASE("FL_WARN_IF compiles on constrained platforms") {
        // Verify FL_WARN_IF also compiles correctly
        #if !SKETCH_HAS_LOTS_OF_MEMORY
        FL_WARN_IF(true, "This should be a no-op");
        #else
        FL_WARN_IF(true, "This should output");
        #endif
        CHECK(true);
    }
}

TEST_CASE("warning macros with complex expressions") {
    SUBCASE("FL_WARN with arithmetic expression") {
        int a = 10, b = 20;
        FL_WARN("Sum: " << (a + b) << ", Product: " << (a * b));
        CHECK(true);
    }

    SUBCASE("FL_WARN with string concatenation") {
        const char* prefix = "Warning: ";
        const char* message = "something happened";
        FL_WARN(prefix << message);
        CHECK(true);
    }

    SUBCASE("FL_WARN_IF with compound boolean expression") {
        int x = 5, y = 10;
        FL_WARN_IF(x < y && y > 0, "Both conditions met");
        CHECK(true);
    }

    SUBCASE("FL_WARN with multiple values") {
        int value1 = 255;
        int value2 = 100;
        FL_WARN("Value1: " << value1 << ", Value2: " << value2);
        CHECK(true);
    }
}

TEST_CASE("warning macros do not interfere with control flow") {
    SUBCASE("FL_WARN does not break if-else chain") {
        int result = 0;
        if (false) {
            result = 1;
        } else {
            FL_WARN("In else block");
            result = 2;
        }
        CHECK_EQ(result, 2);
    }

    SUBCASE("FL_WARN_IF does not affect loop iteration") {
        int count = 0;
        for (int i = 0; i < 5; i++) {
            FL_WARN_IF(i == 2, "Iteration 2");
            count++;
        }
        CHECK_EQ(count, 5);
    }

    SUBCASE("FL_WARN in switch statement") {
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
        CHECK_EQ(result, 2);
    }
}

TEST_CASE("warning macro edge cases") {
    SUBCASE("empty message (should still compile)") {
        // Even empty warnings should compile
        FL_WARN("");
        CHECK(true);
    }

    SUBCASE("very long message") {
        // Should handle long strings
        FL_WARN("This is a very long warning message that contains a lot of text "
                "and spans multiple lines in the source code but is still just "
                "one continuous string literal for testing purposes");
        CHECK(true);
    }

    SUBCASE("FL_WARN_IF with always-false condition") {
        // Should not cause issues even if never triggered
        FL_WARN_IF(false, "Never shown");
        CHECK(true);
    }

    SUBCASE("FL_WARN_IF with compile-time constant") {
        // Should work with constexpr conditions
        constexpr bool always_true = true;
        FL_WARN_IF(always_true, "Constexpr condition");
        CHECK(true);
    }
}
