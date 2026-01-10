#include "fl/convert.h"
#include "doctest.h"
#include "fl/int.h"

using namespace fl;

// Test the single function in convert.h
TEST_CASE("convert_fastled_timings_to_timedeltas") {
    SUBCASE("basic conversion") {
        u16 T1 = 100;
        u16 T2 = 200;
        u16 T3 = 300;
        u16 T0H, T0L, T1H, T1L;

        convert_fastled_timings_to_timedeltas(T1, T2, T3, &T0H, &T0L, &T1H, &T1L);

        // T0H = T1
        CHECK_EQ(T0H, 100);
        // T0L = T2 + T3
        CHECK_EQ(T0L, 500);
        // T1H = T1 + T2
        CHECK_EQ(T1H, 300);
        // T1L = T3
        CHECK_EQ(T1L, 300);
    }

    SUBCASE("zero values") {
        u16 T0H, T0L, T1H, T1L;

        convert_fastled_timings_to_timedeltas(0, 0, 0, &T0H, &T0L, &T1H, &T1L);

        CHECK_EQ(T0H, 0);
        CHECK_EQ(T0L, 0);
        CHECK_EQ(T1H, 0);
        CHECK_EQ(T1L, 0);
    }

    SUBCASE("maximum values") {
        u16 T1 = 0xFFFF;
        u16 T2 = 0;
        u16 T3 = 0;
        u16 T0H, T0L, T1H, T1L;

        convert_fastled_timings_to_timedeltas(T1, T2, T3, &T0H, &T0L, &T1H, &T1L);

        CHECK_EQ(T0H, 0xFFFF);
        CHECK_EQ(T0L, 0);
        CHECK_EQ(T1H, 0xFFFF);
        CHECK_EQ(T1L, 0);
    }

    SUBCASE("typical LED timing values (WS2812B-like)") {
        // Typical FastLED timings for WS2812B:
        // T1 = 350ns (high time for both 0 and 1)
        // T2 = 350ns (additional high time for 1-bit)
        // T3 = 550ns (low time)
        u16 T1 = 350;
        u16 T2 = 350;
        u16 T3 = 550;
        u16 T0H, T0L, T1H, T1L;

        convert_fastled_timings_to_timedeltas(T1, T2, T3, &T0H, &T0L, &T1H, &T1L);

        // For 0-bit:
        CHECK_EQ(T0H, 350);  // T0H = 350ns
        CHECK_EQ(T0L, 900);  // T0L = 350 + 550 = 900ns

        // For 1-bit:
        CHECK_EQ(T1H, 700);  // T1H = 350 + 350 = 700ns
        CHECK_EQ(T1L, 550);  // T1L = 550ns

        // Verify total period is the same for both bits
        u16 period_0 = T0H + T0L;
        u16 period_1 = T1H + T1L;
        CHECK_EQ(period_0, period_1);
        CHECK_EQ(period_0, 1250);  // Total period = 1250ns
    }

    SUBCASE("edge case - T2 and T3 sum") {
        // Test that T0L correctly sums T2 + T3
        u16 T1 = 10;
        u16 T2 = 20000;
        u16 T3 = 30000;
        u16 T0H, T0L, T1H, T1L;

        convert_fastled_timings_to_timedeltas(T1, T2, T3, &T0H, &T0L, &T1H, &T1L);

        CHECK_EQ(T0H, 10);
        CHECK_EQ(T0L, 50000);  // 20000 + 30000
        CHECK_EQ(T1H, 20010);  // 10 + 20000
        CHECK_EQ(T1L, 30000);
    }

    SUBCASE("overflow handling with wraparound") {
        // Test behavior when sum would exceed u16 range
        // The function doesn't check for overflow, so wraparound occurs
        u16 T1 = 0x8000;  // 32768
        u16 T2 = 0x8000;  // 32768
        u16 T3 = 0x1000;  // 4096
        u16 T0H, T0L, T1H, T1L;

        convert_fastled_timings_to_timedeltas(T1, T2, T3, &T0H, &T0L, &T1H, &T1L);

        CHECK_EQ(T0H, 0x8000);
        // T0L = 0x8000 + 0x1000 = 0x9000 (no overflow)
        CHECK_EQ(T0L, 0x9000);
        // T1H = 0x8000 + 0x8000 = 0x10000, wraps to 0x0000
        CHECK_EQ(T1H, 0);
        CHECK_EQ(T1L, 0x1000);
    }

    SUBCASE("realistic APA102 timing") {
        // APA102 uses different timing structure
        u16 T1 = 250;
        u16 T2 = 250;
        u16 T3 = 500;
        u16 T0H, T0L, T1H, T1L;

        convert_fastled_timings_to_timedeltas(T1, T2, T3, &T0H, &T0L, &T1H, &T1L);

        CHECK_EQ(T0H, 250);
        CHECK_EQ(T0L, 750);  // 250 + 500
        CHECK_EQ(T1H, 500);  // 250 + 250
        CHECK_EQ(T1L, 500);
    }
}

// Additional test to verify the function doesn't modify input parameters
TEST_CASE("convert_fastled_timings_to_timedeltas input preservation") {
    u16 T1 = 123;
    u16 T2 = 456;
    u16 T3 = 789;
    u16 T0H, T0L, T1H, T1L;

    u16 T1_orig = T1;
    u16 T2_orig = T2;
    u16 T3_orig = T3;

    convert_fastled_timings_to_timedeltas(T1, T2, T3, &T0H, &T0L, &T1H, &T1L);

    // Verify inputs weren't modified
    CHECK_EQ(T1, T1_orig);
    CHECK_EQ(T2, T2_orig);
    CHECK_EQ(T3, T3_orig);
}
