#include "fl/clamp.h"
#include "fl/stl/stdint.h"
#include "doctest.h"

using namespace fl;

TEST_CASE("fl::clamp") {
    SUBCASE("integer types") {
        // Basic functionality
        CHECK_EQ(clamp(5, 0, 10), 5);
        CHECK_EQ(clamp(-5, 0, 10), 0);
        CHECK_EQ(clamp(15, 0, 10), 10);

        // Boundary values
        CHECK_EQ(clamp(0, 0, 10), 0);
        CHECK_EQ(clamp(10, 0, 10), 10);

        // Negative ranges
        CHECK_EQ(clamp(-5, -10, -1), -5);
        CHECK_EQ(clamp(-15, -10, -1), -10);
        CHECK_EQ(clamp(0, -10, -1), -1);

        // Same min and max
        CHECK_EQ(clamp(5, 7, 7), 7);
        CHECK_EQ(clamp(10, 7, 7), 7);
    }

    SUBCASE("uint8_t") {
        uint8_t val1 = 128;
        uint8_t val2 = 0;
        uint8_t val3 = 255;

        CHECK_EQ(clamp(val1, uint8_t(50), uint8_t(200)), uint8_t(128));
        CHECK_EQ(clamp(val2, uint8_t(50), uint8_t(200)), uint8_t(50));
        CHECK_EQ(clamp(val3, uint8_t(50), uint8_t(200)), uint8_t(200));
    }

    SUBCASE("int8_t") {
        int8_t val1 = 0;
        int8_t val2 = -100;
        int8_t val3 = 100;

        CHECK_EQ(clamp(val1, int8_t(-50), int8_t(50)), int8_t(0));
        CHECK_EQ(clamp(val2, int8_t(-50), int8_t(50)), int8_t(-50));
        CHECK_EQ(clamp(val3, int8_t(-50), int8_t(50)), int8_t(50));
    }

    SUBCASE("uint16_t") {
        uint16_t val1 = 1000;
        uint16_t val2 = 0;
        uint16_t val3 = 65535;

        CHECK_EQ(clamp(val1, uint16_t(100), uint16_t(2000)), uint16_t(1000));
        CHECK_EQ(clamp(val2, uint16_t(100), uint16_t(2000)), uint16_t(100));
        CHECK_EQ(clamp(val3, uint16_t(100), uint16_t(2000)), uint16_t(2000));
    }

    SUBCASE("int16_t") {
        int16_t val1 = 0;
        int16_t val2 = -30000;
        int16_t val3 = 30000;

        CHECK_EQ(clamp(val1, int16_t(-1000), int16_t(1000)), int16_t(0));
        CHECK_EQ(clamp(val2, int16_t(-1000), int16_t(1000)), int16_t(-1000));
        CHECK_EQ(clamp(val3, int16_t(-1000), int16_t(1000)), int16_t(1000));
    }

    SUBCASE("uint32_t") {
        uint32_t val1 = 500000;
        uint32_t val2 = 0;
        uint32_t val3 = 2000000;

        CHECK_EQ(clamp(val1, uint32_t(100000), uint32_t(1000000)), uint32_t(500000));
        CHECK_EQ(clamp(val2, uint32_t(100000), uint32_t(1000000)), uint32_t(100000));
        CHECK_EQ(clamp(val3, uint32_t(100000), uint32_t(1000000)), uint32_t(1000000));
    }

    SUBCASE("int32_t") {
        int32_t val1 = 0;
        int32_t val2 = -2000000;
        int32_t val3 = 2000000;

        CHECK_EQ(clamp(val1, int32_t(-1000000), int32_t(1000000)), int32_t(0));
        CHECK_EQ(clamp(val2, int32_t(-1000000), int32_t(1000000)), int32_t(-1000000));
        CHECK_EQ(clamp(val3, int32_t(-1000000), int32_t(1000000)), int32_t(1000000));
    }

    SUBCASE("float") {
        // Basic functionality
        CHECK_EQ(clamp(5.5f, 0.0f, 10.0f), 5.5f);
        CHECK_EQ(clamp(-5.5f, 0.0f, 10.0f), 0.0f);
        CHECK_EQ(clamp(15.5f, 0.0f, 10.0f), 10.0f);

        // Boundary values
        CHECK_EQ(clamp(0.0f, 0.0f, 10.0f), 0.0f);
        CHECK_EQ(clamp(10.0f, 0.0f, 10.0f), 10.0f);

        // Negative ranges
        CHECK_EQ(clamp(-5.5f, -10.0f, -1.0f), -5.5f);
        CHECK_EQ(clamp(-15.5f, -10.0f, -1.0f), -10.0f);
        CHECK_EQ(clamp(0.0f, -10.0f, -1.0f), -1.0f);

        // Very small epsilon values
        CHECK_EQ(clamp(0.001f, 0.0f, 1.0f), 0.001f);
        CHECK_EQ(clamp(0.999f, 0.0f, 1.0f), 0.999f);
    }

    SUBCASE("double") {
        // Basic functionality
        CHECK_EQ(clamp(5.5, 0.0, 10.0), 5.5);
        CHECK_EQ(clamp(-5.5, 0.0, 10.0), 0.0);
        CHECK_EQ(clamp(15.5, 0.0, 10.0), 10.0);

        // Boundary values
        CHECK_EQ(clamp(0.0, 0.0, 10.0), 0.0);
        CHECK_EQ(clamp(10.0, 0.0, 10.0), 10.0);

        // Negative ranges
        CHECK_EQ(clamp(-5.5, -10.0, -1.0), -5.5);
        CHECK_EQ(clamp(-15.5, -10.0, -1.0), -10.0);
        CHECK_EQ(clamp(0.0, -10.0, -1.0), -1.0);

        // High precision values
        CHECK_EQ(clamp(0.123456789, 0.0, 1.0), 0.123456789);
        CHECK_EQ(clamp(0.987654321, 0.0, 1.0), 0.987654321);
    }

    SUBCASE("edge cases") {
        // Zero range (min == max)
        CHECK_EQ(clamp(5, 7, 7), 7);
        CHECK_EQ(clamp(5.5f, 7.0f, 7.0f), 7.0f);

        // Large values
        CHECK_EQ(clamp(1000000, 0, 999999), 999999);
        CHECK_EQ(clamp(-1000000, -999999, 0), -999999);

        // Value exactly at boundaries
        CHECK_EQ(clamp(0, 0, 100), 0);
        CHECK_EQ(clamp(100, 0, 100), 100);
        CHECK_EQ(clamp(0.0f, 0.0f, 1.0f), 0.0f);
        CHECK_EQ(clamp(1.0f, 0.0f, 1.0f), 1.0f);
    }

    // Note: clamp is not currently marked constexpr in fl/clamp.h
    // If it becomes constexpr in the future, compile-time tests could be added
}
