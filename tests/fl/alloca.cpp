#include "fl/alloca.h"
#include "fl/stl/stdint.h"
#include "doctest.h"
#include "fl/int.h"

using namespace fl;

// Test basic usage of FASTLED_STACK_ARRAY with different types
TEST_CASE("FASTLED_STACK_ARRAY basic usage") {
    SUBCASE("uint8_t array") {
        FASTLED_STACK_ARRAY(uint8_t, buffer, 10);
        // Verify zero initialization
        for (int i = 0; i < 10; i++) {
            CHECK_EQ(buffer[i], 0);
        }
        // Verify writability
        buffer[0] = 42;
        CHECK_EQ(buffer[0], 42);
    }

    SUBCASE("uint16_t array") {
        FASTLED_STACK_ARRAY(uint16_t, buffer, 5);
        // Verify zero initialization
        for (int i = 0; i < 5; i++) {
            CHECK_EQ(buffer[i], 0);
        }
        // Verify writability
        buffer[2] = 1000;
        CHECK_EQ(buffer[2], 1000);
    }

    SUBCASE("uint32_t array") {
        FASTLED_STACK_ARRAY(uint32_t, buffer, 8);
        // Verify zero initialization
        for (int i = 0; i < 8; i++) {
            CHECK_EQ(buffer[i], 0u);
        }
        // Verify writability
        buffer[7] = 0xDEADBEEF;
        CHECK_EQ(buffer[7], 0xDEADBEEFu);
    }

    SUBCASE("int32_t array") {
        FASTLED_STACK_ARRAY(int32_t, buffer, 6);
        // Verify zero initialization
        for (int i = 0; i < 6; i++) {
            CHECK_EQ(buffer[i], 0);
        }
        // Verify writability with negative numbers
        buffer[3] = -42;
        CHECK_EQ(buffer[3], -42);
    }
}

// Test with different sizes
TEST_CASE("FASTLED_STACK_ARRAY different sizes") {
    SUBCASE("size 1") {
        FASTLED_STACK_ARRAY(int, buffer, 1);
        CHECK_EQ(buffer[0], 0);
        buffer[0] = 99;
        CHECK_EQ(buffer[0], 99);
    }

    SUBCASE("size 16") {
        FASTLED_STACK_ARRAY(uint8_t, buffer, 16);
        for (int i = 0; i < 16; i++) {
            CHECK_EQ(buffer[i], 0);
        }
        // Write pattern
        for (int i = 0; i < 16; i++) {
            buffer[i] = static_cast<uint8_t>(i);
        }
        // Verify pattern
        for (int i = 0; i < 16; i++) {
            CHECK_EQ(buffer[i], static_cast<uint8_t>(i));
        }
    }

    SUBCASE("size 64") {
        FASTLED_STACK_ARRAY(uint8_t, buffer, 64);
        for (int i = 0; i < 64; i++) {
            CHECK_EQ(buffer[i], 0);
        }
        // Spot check
        buffer[0] = 10;
        buffer[31] = 20;
        buffer[63] = 30;
        CHECK_EQ(buffer[0], 10);
        CHECK_EQ(buffer[31], 20);
        CHECK_EQ(buffer[63], 30);
    }

    SUBCASE("size 256") {
        FASTLED_STACK_ARRAY(uint8_t, buffer, 256);
        // Check all are zero initialized
        bool all_zero = true;
        for (int i = 0; i < 256; i++) {
            if (buffer[i] != 0) {
                all_zero = false;
                break;
            }
        }
        CHECK(all_zero);

        // Write to several locations
        buffer[0] = 1;
        buffer[100] = 2;
        buffer[200] = 3;
        buffer[255] = 4;
        CHECK_EQ(buffer[0], 1);
        CHECK_EQ(buffer[100], 2);
        CHECK_EQ(buffer[200], 3);
        CHECK_EQ(buffer[255], 4);
    }
}

// Test with floating point types
TEST_CASE("FASTLED_STACK_ARRAY floating point types") {
    SUBCASE("float array") {
        FASTLED_STACK_ARRAY(float, buffer, 10);
        // Verify zero initialization
        for (int i = 0; i < 10; i++) {
            CHECK_EQ(buffer[i], 0.0f);
        }
        // Verify writability
        buffer[0] = 3.14f;
        buffer[5] = -2.71f;
        CHECK_EQ(buffer[0], doctest::Approx(3.14f));
        CHECK_EQ(buffer[5], doctest::Approx(-2.71f));
    }

    SUBCASE("double array") {
        FASTLED_STACK_ARRAY(double, buffer, 5);
        // Verify zero initialization
        for (int i = 0; i < 5; i++) {
            CHECK_EQ(buffer[i], 0.0);
        }
        // Verify writability
        buffer[2] = 1.41421356;
        CHECK_EQ(buffer[2], doctest::Approx(1.41421356));
    }
}

// Test with struct types
TEST_CASE("FASTLED_STACK_ARRAY struct types") {
    struct Point {
        int x;
        int y;
    };

    SUBCASE("struct array") {
        FASTLED_STACK_ARRAY(Point, buffer, 5);
        // Verify zero initialization
        for (int i = 0; i < 5; i++) {
            CHECK_EQ(buffer[i].x, 0);
            CHECK_EQ(buffer[i].y, 0);
        }
        // Verify writability
        buffer[0].x = 10;
        buffer[0].y = 20;
        buffer[3].x = -5;
        buffer[3].y = 15;
        CHECK_EQ(buffer[0].x, 10);
        CHECK_EQ(buffer[0].y, 20);
        CHECK_EQ(buffer[3].x, -5);
        CHECK_EQ(buffer[3].y, 15);
    }

    struct RGB {
        uint8_t r;
        uint8_t g;
        uint8_t b;
    };

    SUBCASE("RGB struct array") {
        FASTLED_STACK_ARRAY(RGB, colors, 8);
        // Verify zero initialization
        for (int i = 0; i < 8; i++) {
            CHECK_EQ(colors[i].r, 0);
            CHECK_EQ(colors[i].g, 0);
            CHECK_EQ(colors[i].b, 0);
        }
        // Set some colors
        colors[0].r = 255;
        colors[1].g = 128;
        colors[2].b = 64;
        CHECK_EQ(colors[0].r, 255);
        CHECK_EQ(colors[0].g, 0);
        CHECK_EQ(colors[0].b, 0);
        CHECK_EQ(colors[1].r, 0);
        CHECK_EQ(colors[1].g, 128);
        CHECK_EQ(colors[1].b, 0);
    }
}

// Test with pointer types
TEST_CASE("FASTLED_STACK_ARRAY pointer types") {
    SUBCASE("pointer array") {
        FASTLED_STACK_ARRAY(int*, buffer, 5);
        // Verify zero initialization (null pointers)
        for (int i = 0; i < 5; i++) {
            CHECK_EQ(buffer[i], nullptr);
        }
        // Create some integers and store pointers
        int a = 10, b = 20, c = 30;
        buffer[0] = &a;
        buffer[1] = &b;
        buffer[2] = &c;
        CHECK_EQ(*buffer[0], 10);
        CHECK_EQ(*buffer[1], 20);
        CHECK_EQ(*buffer[2], 30);
    }
}

// Test zero initialization is complete
TEST_CASE("FASTLED_STACK_ARRAY zero initialization") {
    SUBCASE("verify all bytes are zero") {
        FASTLED_STACK_ARRAY(uint8_t, buffer, 100);
        bool all_zero = true;
        for (int i = 0; i < 100; i++) {
            if (buffer[i] != 0) {
                all_zero = false;
                break;
            }
        }
        CHECK(all_zero);
    }

    SUBCASE("verify multi-byte types are zero") {
        FASTLED_STACK_ARRAY(uint32_t, buffer, 25);
        bool all_zero = true;
        for (int i = 0; i < 25; i++) {
            if (buffer[i] != 0) {
                all_zero = false;
                break;
            }
        }
        CHECK(all_zero);
    }
}

// Test usage in different contexts
TEST_CASE("FASTLED_STACK_ARRAY usage contexts") {
    SUBCASE("inside if statement") {
        bool condition = true;
        if (condition) {
            FASTLED_STACK_ARRAY(int, buffer, 5);
            buffer[0] = 42;
            CHECK_EQ(buffer[0], 42);
        }
    }

    SUBCASE("inside loop") {
        for (int iter = 0; iter < 3; iter++) {
            FASTLED_STACK_ARRAY(int, buffer, 4);
            // Should be zero initialized each iteration
            CHECK_EQ(buffer[0], 0);
            CHECK_EQ(buffer[1], 0);
            buffer[0] = iter;
            CHECK_EQ(buffer[0], iter);
        }
    }

    SUBCASE("multiple arrays in same scope") {
        FASTLED_STACK_ARRAY(uint8_t, buffer1, 10);
        FASTLED_STACK_ARRAY(uint16_t, buffer2, 5);
        FASTLED_STACK_ARRAY(uint32_t, buffer3, 3);

        // All should be zero initialized
        CHECK_EQ(buffer1[0], 0);
        CHECK_EQ(buffer2[0], 0);
        CHECK_EQ(buffer3[0], 0u);

        // Write to each
        buffer1[0] = 1;
        buffer2[0] = 2;
        buffer3[0] = 3;

        CHECK_EQ(buffer1[0], 1);
        CHECK_EQ(buffer2[0], 2);
        CHECK_EQ(buffer3[0], 3u);
    }
}

// Test with variable size (runtime size)
TEST_CASE("FASTLED_STACK_ARRAY variable size") {
    SUBCASE("size from variable") {
        int size = 10;
        FASTLED_STACK_ARRAY(int, buffer, size);
        for (int i = 0; i < size; i++) {
            CHECK_EQ(buffer[i], 0);
        }
        buffer[size - 1] = 99;
        CHECK_EQ(buffer[size - 1], 99);
    }

    SUBCASE("size from expression") {
        int base = 5;
        FASTLED_STACK_ARRAY(uint8_t, buffer, base * 2);
        for (int i = 0; i < base * 2; i++) {
            CHECK_EQ(buffer[i], 0);
        }
    }

    SUBCASE("size from function call") {
        auto get_size = []() { return 7; };
        int size = get_size();
        FASTLED_STACK_ARRAY(int, buffer, size);
        for (int i = 0; i < size; i++) {
            CHECK_EQ(buffer[i], 0);
        }
    }
}

// Test that macro is defined
TEST_CASE("FASTLED_STACK_ARRAY macro definition") {
    SUBCASE("FASTLED_STACK_ARRAY is defined") {
        #ifdef FASTLED_STACK_ARRAY
        CHECK(true);
        #else
        CHECK(false); // Should never reach here
        #endif
    }

    SUBCASE("FASTLED_VARIABLE_LENGTH_ARRAY_NEEDS_EMULATION is defined") {
        #ifdef FASTLED_VARIABLE_LENGTH_ARRAY_NEEDS_EMULATION
        CHECK(true);
        #else
        CHECK(false); // Should never reach here
        #endif
    }
}

// Test passing to functions
TEST_CASE("FASTLED_STACK_ARRAY function parameters") {
    auto fill_array = [](uint8_t* arr, int size, uint8_t value) {
        for (int i = 0; i < size; i++) {
            arr[i] = value;
        }
    };

    auto sum_array = [](const int* arr, int size) {
        int sum = 0;
        for (int i = 0; i < size; i++) {
            sum += arr[i];
        }
        return sum;
    };

    SUBCASE("pass to function for writing") {
        FASTLED_STACK_ARRAY(uint8_t, buffer, 10);
        fill_array(buffer, 10, 42);
        for (int i = 0; i < 10; i++) {
            CHECK_EQ(buffer[i], 42);
        }
    }

    SUBCASE("pass to function for reading") {
        FASTLED_STACK_ARRAY(int, buffer, 5);
        for (int i = 0; i < 5; i++) {
            buffer[i] = i + 1; // 1, 2, 3, 4, 5
        }
        int sum = sum_array(buffer, 5);
        CHECK_EQ(sum, 15); // 1+2+3+4+5 = 15
    }
}

// Test edge cases
TEST_CASE("FASTLED_STACK_ARRAY edge cases") {
    SUBCASE("size 1 edge case") {
        FASTLED_STACK_ARRAY(uint64_t, buffer, 1);
        CHECK_EQ(buffer[0], 0ull);
        buffer[0] = 0xFFFFFFFFFFFFFFFFull;
        CHECK_EQ(buffer[0], 0xFFFFFFFFFFFFFFFFull);
    }

    SUBCASE("large struct") {
        struct LargeStruct {
            uint8_t data[64];
            int id;
        };
        FASTLED_STACK_ARRAY(LargeStruct, buffer, 2);
        // Verify zero initialization
        CHECK_EQ(buffer[0].id, 0);
        CHECK_EQ(buffer[1].id, 0);
        for (int i = 0; i < 64; i++) {
            CHECK_EQ(buffer[0].data[i], 0);
            CHECK_EQ(buffer[1].data[i], 0);
        }
        // Verify writability
        buffer[0].id = 100;
        buffer[0].data[0] = 255;
        CHECK_EQ(buffer[0].id, 100);
        CHECK_EQ(buffer[0].data[0], 255);
    }
}

// Test with signed types
TEST_CASE("FASTLED_STACK_ARRAY signed types") {
    SUBCASE("int8_t array") {
        FASTLED_STACK_ARRAY(int8_t, buffer, 10);
        for (int i = 0; i < 10; i++) {
            CHECK_EQ(buffer[i], 0);
        }
        buffer[0] = -128;
        buffer[1] = 127;
        CHECK_EQ(buffer[0], -128);
        CHECK_EQ(buffer[1], 127);
    }

    SUBCASE("int16_t array") {
        FASTLED_STACK_ARRAY(int16_t, buffer, 8);
        for (int i = 0; i < 8; i++) {
            CHECK_EQ(buffer[i], 0);
        }
        buffer[0] = -32768;
        buffer[1] = 32767;
        CHECK_EQ(buffer[0], -32768);
        CHECK_EQ(buffer[1], 32767);
    }

    SUBCASE("int64_t array") {
        FASTLED_STACK_ARRAY(int64_t, buffer, 4);
        for (int i = 0; i < 4; i++) {
            CHECK_EQ(buffer[i], 0ll);
        }
        buffer[0] = -9223372036854775807ll - 1; // INT64_MIN
        buffer[1] = 9223372036854775807ll;      // INT64_MAX
        CHECK_EQ(buffer[0], -9223372036854775807ll - 1);
        CHECK_EQ(buffer[1], 9223372036854775807ll);
    }
}

// Test sizeof and pointer arithmetic
TEST_CASE("FASTLED_STACK_ARRAY pointer operations") {
    SUBCASE("pointer arithmetic") {
        FASTLED_STACK_ARRAY(int, buffer, 5);
        int* ptr = buffer;
        for (int i = 0; i < 5; i++) {
            CHECK_EQ(ptr[i], 0);
            ptr[i] = i * 10;
        }
        CHECK_EQ(buffer[0], 0);
        CHECK_EQ(buffer[1], 10);
        CHECK_EQ(buffer[2], 20);
        CHECK_EQ(buffer[3], 30);
        CHECK_EQ(buffer[4], 40);
    }

    SUBCASE("pointer increment") {
        FASTLED_STACK_ARRAY(uint8_t, buffer, 10);
        uint8_t* ptr = buffer;
        for (int i = 0; i < 10; i++) {
            *ptr = static_cast<uint8_t>(i);
            ptr++;
        }
        for (int i = 0; i < 10; i++) {
            CHECK_EQ(buffer[i], static_cast<uint8_t>(i));
        }
    }
}
