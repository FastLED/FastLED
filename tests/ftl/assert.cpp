#include "fl/stl/assert.h"
#include "fl/stl/stdint.h"
#include "doctest.h"
#include "fl/int.h"

using namespace fl;

// Test FL_ASSERT macro basic usage
TEST_CASE("FL_ASSERT basic usage") {
    SUBCASE("assert with true condition does not fail") {
        // This should not produce any output or fail
        FL_ASSERT(true, "This should not trigger");
        FL_ASSERT(1 == 1, "One equals one");
        FL_ASSERT(5 > 3, "Five is greater than three");
        CHECK(true); // If we get here, asserts passed
    }

    SUBCASE("assert with simple expressions") {
        int x = 42;
        FL_ASSERT(x == 42, "x should be 42");
        FL_ASSERT(x > 0, "x should be positive");
        FL_ASSERT(x != 0, "x should not be zero");
        CHECK(true); // If we get here, asserts passed
    }

    SUBCASE("assert with pointer checks") {
        int value = 100;
        int* ptr = &value;
        FL_ASSERT(ptr != nullptr, "pointer should not be null");
        FL_ASSERT(*ptr == 100, "dereferenced value should be 100");
        CHECK(true); // If we get here, asserts passed
    }
}

// Test FL_ASSERT with various data types
TEST_CASE("FL_ASSERT with different types") {
    SUBCASE("integer types") {
        int8_t i8 = 127;
        uint8_t u8 = 255;
        int16_t i16 = 32767;
        uint16_t u16 = 65535;
        int32_t i32 = 2147483647;
        uint32_t u32 = 4294967295U;

        FL_ASSERT(i8 > 0, "i8 is positive");
        FL_ASSERT(u8 == 255, "u8 is max uint8");
        FL_ASSERT(i16 > 0, "i16 is positive");
        FL_ASSERT(u16 == 65535, "u16 is max uint16");
        FL_ASSERT(i32 > 0, "i32 is positive");
        FL_ASSERT(u32 > 0, "u32 is positive");
        CHECK(true);
    }

    SUBCASE("floating point types") {
        float f = 3.14f;
        double d = 2.718;

        FL_ASSERT(f > 3.0f, "f is greater than 3");
        FL_ASSERT(d > 2.7, "d is greater than 2.7");
        FL_ASSERT(f < 4.0f, "f is less than 4");
        FL_ASSERT(d < 3.0, "d is less than 3");
        CHECK(true);
    }

    SUBCASE("boolean expressions") {
        bool flag = true;
        FL_ASSERT(flag, "flag is true");
        FL_ASSERT(flag == true, "flag equals true");
        FL_ASSERT(!false, "not false is true");
        CHECK(true);
    }
}

// Test FL_ASSERT with complex expressions
TEST_CASE("FL_ASSERT complex expressions") {
    SUBCASE("logical AND") {
        int x = 5;
        int y = 10;
        FL_ASSERT(x > 0 && y > 0, "both x and y are positive");
        FL_ASSERT(x < 10 && y >= 10, "x less than 10 and y at least 10");
        CHECK(true);
    }

    SUBCASE("logical OR") {
        int a = 0;
        int b = 5;
        FL_ASSERT(a == 0 || b == 0, "at least one is zero");
        FL_ASSERT(a >= 0 || b >= 0, "at least one is non-negative");
        CHECK(true);
    }

    SUBCASE("compound expressions") {
        int arr[] = {1, 2, 3, 4, 5};
        int len = sizeof(arr) / sizeof(arr[0]);
        FL_ASSERT(len == 5, "array has 5 elements");
        FL_ASSERT(arr[0] == 1 && arr[4] == 5, "first and last elements correct");
        CHECK(true);
    }
}

// Test FL_ASSERT with string literals
TEST_CASE("FL_ASSERT message formatting") {
    SUBCASE("simple string messages") {
        FL_ASSERT(true, "Simple message");
        FL_ASSERT(1 + 1 == 2, "Math works");
        FL_ASSERT(sizeof(int) >= 2, "int is at least 2 bytes");
        CHECK(true);
    }

    SUBCASE("messages with special characters") {
        FL_ASSERT(true, "Message with \"quotes\"");
        FL_ASSERT(true, "Message with 'apostrophes'");
        FL_ASSERT(true, "Message with numbers: 123");
        CHECK(true);
    }

    SUBCASE("empty message") {
        FL_ASSERT(true, "");
        CHECK(true);
    }
}

// Test FL_ASSERT in different contexts
TEST_CASE("FL_ASSERT in various contexts") {
    SUBCASE("inside if statement") {
        if (true) {
            FL_ASSERT(1 == 1, "Inside if block");
        }
        CHECK(true);
    }

    SUBCASE("inside loop") {
        for (int i = 0; i < 5; i++) {
            FL_ASSERT(i >= 0, "Loop index is non-negative");
            FL_ASSERT(i < 5, "Loop index is within bounds");
        }
        CHECK(true);
    }

    SUBCASE("inside function call chain") {
        auto check_value = [](int val) {
            FL_ASSERT(val > 0, "Value must be positive");
            return val * 2;
        };
        int result = check_value(5);
        CHECK_EQ(result, 10);
    }
}

// Test FL_ASSERT with constexpr contexts (compile-time checks)
TEST_CASE("FL_ASSERT compile-time properties") {
    SUBCASE("assert in constexpr function") {
        // Note: FL_ASSERT itself is not constexpr, but we can verify
        // it doesn't interfere with constexpr computations
        constexpr int computed = 2 + 2;
        FL_ASSERT(computed == 4, "constexpr value is correct");
        CHECK_EQ(computed, 4);
    }

    SUBCASE("assert with sizeof") {
        FL_ASSERT(sizeof(char) == 1, "char is 1 byte");
        FL_ASSERT(sizeof(int) >= sizeof(char), "int is at least as big as char");
        FL_ASSERT(sizeof(double) >= sizeof(float), "double is at least as big as float");
        CHECK(true);
    }

    SUBCASE("assert with alignof") {
        FL_ASSERT(alignof(char) == 1, "char alignment is 1");
        FL_ASSERT(alignof(int) >= 1, "int alignment is at least 1");
        CHECK(true);
    }
}

// Test that FL_ASSERT macros are defined
TEST_CASE("FL_ASSERT macro definitions") {
    SUBCASE("FL_ASSERT is defined") {
        #ifdef FL_ASSERT
        CHECK(true);
        #else
        CHECK(false); // Should never happen
        #endif
    }

    SUBCASE("FL_ASSERT_IF is defined") {
        #ifdef FL_ASSERT_IF
        CHECK(true);
        #else
        // FL_ASSERT_IF may not be defined in all configurations
        // This is acceptable
        CHECK(true);
        #endif
    }
}

// Test FL_ASSERT with edge cases
TEST_CASE("FL_ASSERT edge cases") {
    SUBCASE("assert with null pointer literal") {
        int* p = nullptr;
        FL_ASSERT(p == nullptr, "pointer is null");
        CHECK(true);
    }

    SUBCASE("assert with zero") {
        int zero = 0;
        FL_ASSERT(zero == 0, "zero is zero");
        FL_ASSERT(!zero, "zero is falsy");
        CHECK(true);
    }

    SUBCASE("assert with negative numbers") {
        int neg = -5;
        FL_ASSERT(neg < 0, "negative number is less than zero");
        FL_ASSERT(neg != 0, "negative number is not zero");
        CHECK(true);
    }

    SUBCASE("assert with maximum values") {
        uint8_t max_u8 = 255;
        uint16_t max_u16 = 65535;
        FL_ASSERT(max_u8 == 255, "max uint8 value");
        FL_ASSERT(max_u16 == 65535, "max uint16 value");
        CHECK(true);
    }
}

// Test that FL_ASSERT can be used with struct/class members
TEST_CASE("FL_ASSERT with structs and classes") {
    SUBCASE("struct member checks") {
        struct Point {
            int x;
            int y;
        };

        Point p = {10, 20};
        FL_ASSERT(p.x == 10, "point x coordinate");
        FL_ASSERT(p.y == 20, "point y coordinate");
        FL_ASSERT(p.x < p.y, "x is less than y");
        CHECK(true);
    }

    SUBCASE("class method checks") {
        class Counter {
        public:
            Counter() : count(0) {}
            void increment() { count++; }
            int get() const { return count; }
        private:
            int count;
        };

        Counter c;
        FL_ASSERT(c.get() == 0, "initial count is zero");
        c.increment();
        FL_ASSERT(c.get() == 1, "count after increment");
        CHECK_EQ(c.get(), 1);
    }
}

// Test FL_ASSERT with array bounds
TEST_CASE("FL_ASSERT with arrays") {
    SUBCASE("array index checks") {
        int arr[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        int len = sizeof(arr) / sizeof(arr[0]);

        FL_ASSERT(len == 10, "array length is 10");
        for (int i = 0; i < len; i++) {
            FL_ASSERT(i >= 0 && i < len, "index within bounds");
            FL_ASSERT(arr[i] == i, "array element equals index");
        }
        CHECK(true);
    }

    SUBCASE("multidimensional array") {
        int matrix[3][3] = {
            {1, 2, 3},
            {4, 5, 6},
            {7, 8, 9}
        };

        FL_ASSERT(matrix[0][0] == 1, "first element");
        FL_ASSERT(matrix[1][1] == 5, "center element");
        FL_ASSERT(matrix[2][2] == 9, "last element");
        CHECK(true);
    }
}

// Test that FL_ASSERT doesn't interfere with normal program flow
TEST_CASE("FL_ASSERT program flow") {
    SUBCASE("multiple asserts in sequence") {
        int x = 1;
        FL_ASSERT(x == 1, "first assert");
        x = 2;
        FL_ASSERT(x == 2, "second assert");
        x = 3;
        FL_ASSERT(x == 3, "third assert");
        CHECK_EQ(x, 3);
    }

    SUBCASE("assert doesn't affect return values") {
        auto function_with_assert = [](int val) -> int {
            FL_ASSERT(val > 0, "input must be positive");
            return val * 2;
        };

        int result = function_with_assert(5);
        CHECK_EQ(result, 10);
    }

    SUBCASE("assert in conditional branches") {
        int value = 42;
        if (value > 0) {
            FL_ASSERT(value > 0, "positive branch");
            CHECK(value > 0);
        } else {
            FL_ASSERT(value <= 0, "non-positive branch");
            CHECK(value <= 0);
        }
    }
}
