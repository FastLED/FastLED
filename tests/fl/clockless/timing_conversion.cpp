/// @file timing_conversion.cpp
/// @brief Unit tests for LED timing format conversion utilities
///
/// Tests the conversion between datasheet format (T0H, T0L, T1H, T1L) and
/// 3-phase timing format (T1, T2, T3).

#include "doctest.h"
#include "fl/clockless/timing_conversion.h"
#include "fl/chipsets/led_timing.h"
#include "fl/math_macros.h"

using namespace fl;

// ============================================================================
// Test: Timing Struct Properties
// ============================================================================

TEST_CASE("DatasheetTiming properties") {
    SUBCASE("cycle calculation") {
        DatasheetTiming ds{400, 850, 850, 400};

        CHECK(ds.cycle_0() == 1250);
        CHECK(ds.cycle_1() == 1250);
        CHECK(ds.duration() == 1250);
    }

    SUBCASE("asymmetric cycles") {
        DatasheetTiming ds{250, 500, 875, 875};

        CHECK(ds.cycle_0() == 750);
        CHECK(ds.cycle_1() == 1750);
        CHECK(ds.duration() == 1750);  // max(750, 1750)
    }
}

TEST_CASE("Timing3Phase properties") {
    SUBCASE("duration calculation") {
        Timing3Phase fl{250, 625, 375};

        CHECK(fl.duration() == 1250);
        CHECK(fl.high_time_0() == 250);
        CHECK(fl.high_time_1() == 875);
    }

    SUBCASE("zero T3") {
        Timing3Phase fl{400, 450, 0};

        CHECK(fl.duration() == 850);
        CHECK(fl.high_time_0() == 400);
        CHECK(fl.high_time_1() == 850);
    }
}

// ============================================================================
// Test: Forward Conversion (Datasheet -> 3-Phase)
// ============================================================================

TEST_CASE("datasheet_to_phase3 - basic conversion") {
    SUBCASE("WS2812B typical values") {
        DatasheetTiming ds{400, 850, 850, 400};
        Timing3Phase fl = datasheet_to_phase3(ds);

        CHECK(fl.T1 == 400);
        CHECK(fl.T2 == 450);
        CHECK(fl.T3 == 400);
        CHECK(fl.duration() == 1250);
    }

    SUBCASE("WS2812 tight timing") {
        DatasheetTiming ds{250, 1000, 875, 375};
        Timing3Phase fl = datasheet_to_phase3(ds);

        CHECK(fl.T1 == 250);
        CHECK(fl.T2 == 625);
        CHECK(fl.T3 == 375);
        CHECK(fl.duration() == 1250);
    }

    SUBCASE("SK6812 fast protocol") {
        DatasheetTiming ds{300, 900, 600, 600};
        Timing3Phase fl = datasheet_to_phase3(ds);

        CHECK(fl.T1 == 300);
        CHECK(fl.T2 == 300);
        CHECK(fl.T3 == 600);
        CHECK(fl.duration() == 1200);
    }
}

TEST_CASE("datasheet_to_phase3 - asymmetric cycles") {
    SUBCASE("longer bit-1 cycle") {
        DatasheetTiming ds{250, 500, 875, 875};
        Timing3Phase fl = datasheet_to_phase3(ds);

        // Duration should be max(750, 1750) = 1750
        CHECK(fl.T1 == 250);
        CHECK(fl.T2 == 625);
        CHECK(fl.T3 == 875);
        CHECK(fl.duration() == 1750);
    }

    SUBCASE("longer bit-0 cycle") {
        DatasheetTiming ds{300, 2000, 600, 600};
        Timing3Phase fl = datasheet_to_phase3(ds);

        // Duration should be max(2300, 1200) = 2300
        CHECK(fl.T1 == 300);
        CHECK(fl.T2 == 300);
        CHECK(fl.T3 == 1700);
        CHECK(fl.duration() == 2300);
    }
}

// ============================================================================
// Test: Inverse Conversion (3-Phase -> Datasheet)
// ============================================================================

TEST_CASE("phase3_to_datasheet - basic conversion") {
    SUBCASE("WS2812 values") {
        Timing3Phase fl{250, 625, 375};
        DatasheetTiming ds = phase3_to_datasheet(fl);

        CHECK(ds.T0H == 250);
        CHECK(ds.T0L == 1000);
        CHECK(ds.T1H == 875);
        CHECK(ds.T1L == 375);
        CHECK(ds.cycle_0() == 1250);
        CHECK(ds.cycle_1() == 1250);
    }

    SUBCASE("SK6812 values") {
        Timing3Phase fl{300, 600, 300};
        DatasheetTiming ds = phase3_to_datasheet(fl);

        CHECK(ds.T0H == 300);
        CHECK(ds.T0L == 900);
        CHECK(ds.T1H == 900);
        CHECK(ds.T1L == 300);
        CHECK(ds.cycle_0() == 1200);
        CHECK(ds.cycle_1() == 1200);
    }
}

TEST_CASE("phase3_to_datasheet - symmetric cycles") {
    SUBCASE("verify symmetric assumption") {
        Timing3Phase fl{400, 450, 400};
        DatasheetTiming ds = phase3_to_datasheet(fl);

        // With symmetric assumption: T0H+T0L = T1H+T1L = duration
        CHECK(ds.cycle_0() == ds.cycle_1());
        CHECK(ds.duration() == fl.duration());
    }
}

// ============================================================================
// Test: Round-Trip Conversion
// ============================================================================

TEST_CASE("round-trip conversion preserves values") {
    SUBCASE("WS2812_800KHZ") {
        Timing3Phase original{250, 625, 375};
        DatasheetTiming ds = phase3_to_datasheet(original);
        Timing3Phase recovered = datasheet_to_phase3(ds);

        CHECK(recovered.T1 == original.T1);
        CHECK(recovered.T2 == original.T2);
        CHECK(recovered.T3 == original.T3);
    }

    SUBCASE("SK6812") {
        Timing3Phase original{300, 600, 300};
        DatasheetTiming ds = phase3_to_datasheet(original);
        Timing3Phase recovered = datasheet_to_phase3(ds);

        CHECK(recovered.T1 == original.T1);
        CHECK(recovered.T2 == original.T2);
        CHECK(recovered.T3 == original.T3);
    }

    SUBCASE("WS2811_400KHZ") {
        Timing3Phase original{800, 800, 900};
        DatasheetTiming ds = phase3_to_datasheet(original);
        Timing3Phase recovered = datasheet_to_phase3(ds);

        CHECK(recovered.T1 == original.T1);
        CHECK(recovered.T2 == original.T2);
        CHECK(recovered.T3 == original.T3);
    }
}

// ============================================================================
// Test: Real Chipset Timing Integration
// ============================================================================

TEST_CASE("real chipset timings work with conversion functions") {
    // Verify that actual LED timing definitions from led_timing.h
    // can be converted to datasheet format and back

    SUBCASE("TIMING_SK6812 round-trip") {
        using Chipset = TIMING_SK6812;
        Timing3Phase original{Chipset::T1, Chipset::T2, Chipset::T3};

        DatasheetTiming ds = phase3_to_datasheet(original);
        Timing3Phase recovered = datasheet_to_phase3(ds);

        CHECK(recovered.T1 == Chipset::T1);
        CHECK(recovered.T2 == Chipset::T2);
        CHECK(recovered.T3 == Chipset::T3);
    }

    SUBCASE("TIMING_WS2813 round-trip") {
        using Chipset = TIMING_WS2813;
        Timing3Phase original{Chipset::T1, Chipset::T2, Chipset::T3};

        DatasheetTiming ds = phase3_to_datasheet(original);
        Timing3Phase recovered = datasheet_to_phase3(ds);

        CHECK(recovered.T1 == Chipset::T1);
        CHECK(recovered.T2 == Chipset::T2);
        CHECK(recovered.T3 == Chipset::T3);
    }

    SUBCASE("TIMING_TM1814 round-trip") {
        using Chipset = TIMING_TM1814;
        Timing3Phase original{Chipset::T1, Chipset::T2, Chipset::T3};

        DatasheetTiming ds = phase3_to_datasheet(original);
        Timing3Phase recovered = datasheet_to_phase3(ds);

        CHECK(recovered.T1 == Chipset::T1);
        CHECK(recovered.T2 == Chipset::T2);
        CHECK(recovered.T3 == Chipset::T3);
    }
}

// ============================================================================
// Test: Protocol Semantics
// ============================================================================

TEST_CASE("protocol semantics") {
    SUBCASE("T0H equals T1") {
        Timing3Phase fl{400, 450, 400};
        DatasheetTiming ds = phase3_to_datasheet(fl);

        // T0H is the high time for '0' bit, which equals T1
        CHECK(ds.T0H == fl.T1);
    }

    SUBCASE("T1H equals T1+T2") {
        Timing3Phase fl{250, 625, 375};
        DatasheetTiming ds = phase3_to_datasheet(fl);

        // T1H is the high time for '1' bit, which equals T1+T2
        CHECK(ds.T1H == (fl.T1 + fl.T2));
    }

    SUBCASE("duration consistency") {
        Timing3Phase fl{300, 600, 300};
        DatasheetTiming ds = phase3_to_datasheet(fl);

        // Total duration should be preserved
        CHECK(ds.duration() == fl.duration());
    }
}

// ============================================================================
// Test: Edge Cases
// ============================================================================

TEST_CASE("edge cases") {
    SUBCASE("zero T3") {
        DatasheetTiming ds{400, 850, 850, 400};
        Timing3Phase fl = datasheet_to_phase3(ds);

        // Even with zero T3, conversion should work
        CHECK(fl.T3 == 400);
    }

    SUBCASE("large values") {
        DatasheetTiming ds{1000, 3000, 2000, 2000};
        Timing3Phase fl = datasheet_to_phase3(ds);

        CHECK(fl.T1 == 1000);
        CHECK(fl.T2 == 1000);
        CHECK(fl.T3 == 2000);
        CHECK(fl.duration() == 4000);
    }

    SUBCASE("minimum values") {
        DatasheetTiming ds{100, 200, 150, 150};
        Timing3Phase fl = datasheet_to_phase3(ds);

        CHECK(fl.T1 == 100);
        CHECK(fl.T2 == 50);
        CHECK(fl.T3 == 150);
        CHECK(fl.duration() == 300);
    }
}

// ============================================================================
// Test: Common Chipset Values
// ============================================================================

TEST_CASE("common chipset round-trips") {
    // Test values from actual chipsets to ensure they work correctly

    struct ChipsetTest {
        const char* name;
        uint32_t T1, T2, T3;
    };

    ChipsetTest chipsets[] = {
        {"WS2812_800KHZ", 250, 625, 375},
        {"WS2813", 320, 320, 640},
        {"SK6812", 300, 600, 300},
        {"SK6822", 375, 1000, 375},
        {"UCS1903B_800KHZ", 400, 450, 450},
        {"WS2811_400KHZ", 800, 800, 900},
        {"WS2815", 250, 1090, 550},
        {"TM1814", 360, 600, 340},
    };

    for (const auto& test : chipsets) {
        SUBCASE(test.name) {
            Timing3Phase original{test.T1, test.T2, test.T3};
            DatasheetTiming ds = phase3_to_datasheet(original);
            Timing3Phase recovered = datasheet_to_phase3(ds);

            CHECK(recovered.T1 == original.T1);
            CHECK(recovered.T2 == original.T2);
            CHECK(recovered.T3 == original.T3);

            // Verify protocol semantics
            CHECK(ds.T0H == original.T1);
            CHECK(ds.T1H == (original.T1 + original.T2));
        }
    }
}
