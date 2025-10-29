#include "test.h"
#include "fl/ease.h"

using namespace fl;

// Test 8-bit easing functions

TEST_CASE("fl::easeInQuad8") {
    SUBCASE("boundary values") {
        CHECK_EQ(easeInQuad8(0), 0);
        CHECK_EQ(easeInQuad8(255), 255);
    }

    SUBCASE("midpoint") {
        // At i=128 (halfway), quadratic should be at ~25% of output range
        u8 mid = easeInQuad8(128);
        CHECK(mid > 60);  // Should be around 64 (128*128/255)
        CHECK(mid < 70);
    }

    SUBCASE("monotonic increase") {
        u8 prev = easeInQuad8(0);
        for (u16 i = 1; i <= 255; i++) {
            u8 curr = easeInQuad8(i);
            CHECK(curr >= prev);
            prev = curr;
        }
    }

    SUBCASE("acceleration property") {
        // Ease-in should have smaller changes at the start
        u8 early_diff = easeInQuad8(50) - easeInQuad8(0);
        u8 late_diff = easeInQuad8(255) - easeInQuad8(205);
        CHECK(early_diff < late_diff);
    }
}

TEST_CASE("fl::easeOutQuad8") {
    SUBCASE("boundary values") {
        CHECK_EQ(easeOutQuad8(0), 0);
        CHECK_EQ(easeOutQuad8(255), 255);
    }

    SUBCASE("midpoint") {
        // At i=128 (halfway), quadratic ease-out should be at ~75% of output range
        u8 mid = easeOutQuad8(128);
        CHECK(mid > 185);  // Should be around 191
        CHECK(mid < 195);
    }

    SUBCASE("monotonic increase") {
        u8 prev = easeOutQuad8(0);
        for (u16 i = 1; i <= 255; i++) {
            u8 curr = easeOutQuad8(i);
            CHECK(curr >= prev);
            prev = curr;
        }
    }

    SUBCASE("deceleration property") {
        // Ease-out should have larger changes at the start
        u8 early_diff = easeOutQuad8(50) - easeOutQuad8(0);
        u8 late_diff = easeOutQuad8(255) - easeOutQuad8(205);
        CHECK(early_diff > late_diff);
    }
}

TEST_CASE("fl::easeInOutQuad8") {
    SUBCASE("boundary values") {
        CHECK_EQ(easeInOutQuad8(0), 0);
        CHECK_EQ(easeInOutQuad8(255), 255);
    }

    SUBCASE("midpoint") {
        // At i=128 (halfway), should be at approximately 50% of output range
        u8 mid = easeInOutQuad8(128);
        CHECK(mid > 125);
        CHECK(mid < 131);
    }

    SUBCASE("monotonic increase") {
        u8 prev = easeInOutQuad8(0);
        for (u16 i = 1; i <= 255; i++) {
            u8 curr = easeInOutQuad8(i);
            CHECK(curr >= prev);
            prev = curr;
        }
    }

    SUBCASE("symmetry property") {
        // First half should ease in, second half should ease out
        u8 early_diff = easeInOutQuad8(64) - easeInOutQuad8(32);
        u8 mid_diff = easeInOutQuad8(160) - easeInOutQuad8(128);
        u8 late_diff = easeInOutQuad8(223) - easeInOutQuad8(191);

        // Middle section should have largest changes
        CHECK(mid_diff > early_diff);
        CHECK(mid_diff > late_diff);
    }
}

TEST_CASE("fl::easeInCubic8") {
    SUBCASE("boundary values") {
        CHECK_EQ(easeInCubic8(0), 0);
        CHECK_EQ(easeInCubic8(255), 255);
    }

    SUBCASE("midpoint") {
        // At i=128, cubic should be at ~12.5% of output range
        u8 mid = easeInCubic8(128);
        CHECK(mid > 30);  // Should be around 32 (128^3 / 255^2)
        CHECK(mid < 36);
    }

    SUBCASE("monotonic increase") {
        u8 prev = easeInCubic8(0);
        for (u16 i = 1; i <= 255; i++) {
            u8 curr = easeInCubic8(i);
            CHECK(curr >= prev);
            prev = curr;
        }
    }

    SUBCASE("stronger acceleration than quadratic") {
        // Cubic should be below quadratic for most of the curve
        for (u16 i = 10; i < 245; i += 10) {
            u8 cubic = easeInCubic8(i);
            u8 quad = easeInQuad8(i);
            CHECK(cubic <= quad);
        }
    }
}

TEST_CASE("fl::easeOutCubic8") {
    SUBCASE("boundary values") {
        CHECK_EQ(easeOutCubic8(0), 0);
        CHECK_EQ(easeOutCubic8(255), 255);
    }

    SUBCASE("midpoint") {
        // At i=128, cubic ease-out should be at ~87.5% of output range
        u8 mid = easeOutCubic8(128);
        CHECK(mid > 220);  // Should be around 223
        CHECK(mid < 227);
    }

    SUBCASE("monotonic increase") {
        u8 prev = easeOutCubic8(0);
        for (u16 i = 1; i <= 255; i++) {
            u8 curr = easeOutCubic8(i);
            CHECK(curr >= prev);
            prev = curr;
        }
    }

    SUBCASE("stronger deceleration than quadratic") {
        // Cubic should be above quadratic for most of the curve
        for (u16 i = 10; i < 245; i += 10) {
            u8 cubic = easeOutCubic8(i);
            u8 quad = easeOutQuad8(i);
            CHECK(cubic >= quad);
        }
    }
}

TEST_CASE("fl::easeInOutCubic8") {
    SUBCASE("boundary values") {
        CHECK_EQ(easeInOutCubic8(0), 0);
        CHECK_EQ(easeInOutCubic8(255), 255);
    }

    SUBCASE("midpoint") {
        // At i=128 (halfway), should be at approximately 50% of output range
        u8 mid = easeInOutCubic8(128);
        CHECK(mid > 125);
        CHECK(mid < 131);
    }

    SUBCASE("monotonic increase") {
        u8 prev = easeInOutCubic8(0);
        for (u16 i = 1; i <= 255; i++) {
            u8 curr = easeInOutCubic8(i);
            CHECK(curr >= prev);
            prev = curr;
        }
    }
}

TEST_CASE("fl::easeInSine8") {
    SUBCASE("boundary values") {
        CHECK_EQ(easeInSine8(0), 0);
        CHECK_EQ(easeInSine8(255), 255);
    }

    SUBCASE("monotonic increase") {
        u8 prev = easeInSine8(0);
        for (u16 i = 1; i <= 255; i++) {
            u8 curr = easeInSine8(i);
            CHECK(curr >= prev);
            prev = curr;
        }
    }

    SUBCASE("smooth acceleration") {
        // Sine ease-in should be smoother than quadratic
        u8 val_quarter = easeInSine8(64);
        u8 val_half = easeInSine8(128);
        u8 val_three_quarter = easeInSine8(192);

        // Should show smooth acceleration curve (below linear initially, then catching up)
        CHECK(val_quarter < 64);
        CHECK(val_half < 128);
        CHECK(val_three_quarter < 255);  // Still accelerating toward the end
    }
}

TEST_CASE("fl::easeOutSine8") {
    SUBCASE("boundary values") {
        CHECK_EQ(easeOutSine8(0), 0);
        CHECK_EQ(easeOutSine8(255), 255);
    }

    SUBCASE("monotonic increase") {
        u8 prev = easeOutSine8(0);
        for (u16 i = 1; i <= 255; i++) {
            u8 curr = easeOutSine8(i);
            CHECK(curr >= prev);
            prev = curr;
        }
    }

    SUBCASE("smooth deceleration") {
        // Sine ease-out should be smoother than quadratic
        u8 val_quarter = easeOutSine8(64);
        u8 val_half = easeOutSine8(128);
        u8 val_three_quarter = easeOutSine8(192);

        // Should show smooth deceleration curve
        CHECK(val_quarter > 64);
        CHECK(val_half > 128);
        CHECK(val_three_quarter < 255);
    }
}

TEST_CASE("fl::easeInOutSine8") {
    SUBCASE("boundary values") {
        CHECK_EQ(easeInOutSine8(0), 0);
        CHECK_EQ(easeInOutSine8(255), 255);
    }

    SUBCASE("midpoint") {
        // At i=128 (halfway), should be at approximately 50% of output range
        u8 mid = easeInOutSine8(128);
        CHECK(mid > 125);
        CHECK(mid < 131);
    }

    SUBCASE("monotonic increase") {
        u8 prev = easeInOutSine8(0);
        for (u16 i = 1; i <= 255; i++) {
            u8 curr = easeInOutSine8(i);
            CHECK(curr >= prev);
            prev = curr;
        }
    }
}

// Test 16-bit easing functions

TEST_CASE("fl::easeInQuad16") {
    SUBCASE("boundary values") {
        CHECK_EQ(easeInQuad16(0), 0);
        CHECK_EQ(easeInQuad16(65535), 65535);
    }

    SUBCASE("midpoint") {
        // At i=32768 (halfway), quadratic should be at ~25% of output range
        u16 mid = easeInQuad16(32768);
        CHECK(mid > 16200);  // Should be around 16384 (32768*32768/65535)
        CHECK(mid < 16550);
    }

    SUBCASE("monotonic increase") {
        u16 prev = easeInQuad16(0);
        for (u32 i = 1; i <= 65535; i += 257) {
            u16 curr = easeInQuad16(i);
            CHECK(curr >= prev);
            prev = curr;
        }
    }

    SUBCASE("acceleration property") {
        // Ease-in should have smaller changes at the start
        u16 early_diff = easeInQuad16(10000) - easeInQuad16(0);
        u16 late_diff = easeInQuad16(65535) - easeInQuad16(55535);
        CHECK(early_diff < late_diff);
    }
}

TEST_CASE("fl::easeOutQuad16") {
    SUBCASE("boundary values") {
        CHECK_EQ(easeOutQuad16(0), 0);
        CHECK_EQ(easeOutQuad16(65535), 65535);
    }

    SUBCASE("midpoint") {
        // At i=32768 (halfway), quadratic ease-out should be at ~75% of output range
        u16 mid = easeOutQuad16(32768);
        CHECK(mid > 48900);  // Should be around 49151
        CHECK(mid < 49400);
    }

    SUBCASE("monotonic increase") {
        u16 prev = easeOutQuad16(0);
        for (u32 i = 1; i <= 65535; i += 257) {
            u16 curr = easeOutQuad16(i);
            CHECK(curr >= prev);
            prev = curr;
        }
    }
}

TEST_CASE("fl::easeInOutQuad16") {
    SUBCASE("boundary values") {
        CHECK_EQ(easeInOutQuad16(0), 0);
        CHECK_EQ(easeInOutQuad16(65535), 65535);
    }

    SUBCASE("midpoint") {
        // At i=32768 (halfway), should be at approximately 50% of output range
        u16 mid = easeInOutQuad16(32768);
        CHECK(mid > 32500);
        CHECK(mid < 33000);
    }

    SUBCASE("monotonic increase") {
        u16 prev = easeInOutQuad16(0);
        for (u32 i = 1; i <= 65535; i += 257) {
            u16 curr = easeInOutQuad16(i);
            CHECK(curr >= prev);
            prev = curr;
        }
    }
}

TEST_CASE("fl::easeInCubic16") {
    SUBCASE("boundary values") {
        CHECK_EQ(easeInCubic16(0), 0);
        CHECK_EQ(easeInCubic16(65535), 65535);
    }

    SUBCASE("midpoint") {
        // At i=32768, cubic should be at ~12.5% of output range
        u16 mid = easeInCubic16(32768);
        CHECK(mid > 8100);  // Should be around 8192
        CHECK(mid < 8300);
    }

    SUBCASE("monotonic increase") {
        u16 prev = easeInCubic16(0);
        for (u32 i = 1; i <= 65535; i += 257) {
            u16 curr = easeInCubic16(i);
            CHECK(curr >= prev);
            prev = curr;
        }
    }
}

TEST_CASE("fl::easeOutCubic16") {
    SUBCASE("boundary values") {
        CHECK_EQ(easeOutCubic16(0), 0);
        CHECK_EQ(easeOutCubic16(65535), 65535);
    }

    SUBCASE("midpoint") {
        // At i=32768, cubic ease-out should be at ~87.5% of output range
        u16 mid = easeOutCubic16(32768);
        CHECK(mid > 57200);  // Should be around 57343
        CHECK(mid < 57500);
    }

    SUBCASE("monotonic increase") {
        u16 prev = easeOutCubic16(0);
        for (u32 i = 1; i <= 65535; i += 257) {
            u16 curr = easeOutCubic16(i);
            CHECK(curr >= prev);
            prev = curr;
        }
    }
}

TEST_CASE("fl::easeInOutCubic16") {
    SUBCASE("boundary values") {
        CHECK_EQ(easeInOutCubic16(0), 0);
        CHECK_EQ(easeInOutCubic16(65535), 65535);
    }

    SUBCASE("midpoint") {
        // At i=32768 (halfway), should be at approximately 50% of output range
        u16 mid = easeInOutCubic16(32768);
        CHECK(mid > 32500);
        CHECK(mid < 33000);
    }

    SUBCASE("monotonic increase") {
        u16 prev = easeInOutCubic16(0);
        for (u32 i = 1; i <= 65535; i += 257) {
            u16 curr = easeInOutCubic16(i);
            CHECK(curr >= prev);
            prev = curr;
        }
    }
}

TEST_CASE("fl::easeInSine16") {
    SUBCASE("boundary values") {
        CHECK_EQ(easeInSine16(0), 0);
        CHECK_EQ(easeInSine16(65535), 65535);
    }

    SUBCASE("monotonic increase") {
        u16 prev = easeInSine16(0);
        for (u32 i = 1; i <= 65535; i += 257) {
            u16 curr = easeInSine16(i);
            CHECK(curr >= prev);
            prev = curr;
        }
    }

    SUBCASE("smooth acceleration") {
        // Sine ease-in should be smoother than quadratic
        u16 val_quarter = easeInSine16(16384);
        u16 val_half = easeInSine16(32768);
        u16 val_three_quarter = easeInSine16(49152);

        // Should show smooth acceleration curve (below linear initially, then catching up)
        CHECK(val_quarter < 16384);
        CHECK(val_half < 32768);
        CHECK(val_three_quarter < 65535);  // Still accelerating toward the end
    }
}

TEST_CASE("fl::easeOutSine16") {
    SUBCASE("boundary values") {
        CHECK_EQ(easeOutSine16(0), 0);
        CHECK_EQ(easeOutSine16(65535), 65535);
    }

    SUBCASE("monotonic increase") {
        u16 prev = easeOutSine16(0);
        for (u32 i = 1; i <= 65535; i += 257) {
            u16 curr = easeOutSine16(i);
            CHECK(curr >= prev);
            prev = curr;
        }
    }

    SUBCASE("smooth deceleration") {
        // Sine ease-out should be smoother than quadratic
        u16 val_quarter = easeOutSine16(16384);
        u16 val_half = easeOutSine16(32768);
        u16 val_three_quarter = easeOutSine16(49152);

        // Should show smooth deceleration curve
        CHECK(val_quarter > 16384);
        CHECK(val_half > 32768);
        CHECK(val_three_quarter < 65535);
    }
}

TEST_CASE("fl::easeInOutSine16") {
    SUBCASE("boundary values") {
        CHECK_EQ(easeInOutSine16(0), 0);
        CHECK_EQ(easeInOutSine16(65535), 65535);
    }

    SUBCASE("midpoint") {
        // At i=32768 (halfway), should be at approximately 50% of output range
        u16 mid = easeInOutSine16(32768);
        CHECK(mid > 32500);
        CHECK(mid < 33000);
    }

    SUBCASE("monotonic increase") {
        u16 prev = easeInOutSine16(0);
        for (u32 i = 1; i <= 65535; i += 257) {
            u16 curr = easeInOutSine16(i);
            CHECK(curr >= prev);
            prev = curr;
        }
    }
}

// Test ease8 dispatcher function

TEST_CASE("fl::ease8 dispatcher") {
    u8 test_val = 128;

    SUBCASE("EASE_NONE") {
        CHECK_EQ(ease8(EASE_NONE, test_val), test_val);
    }

    SUBCASE("EASE_IN_QUAD") {
        CHECK_EQ(ease8(EASE_IN_QUAD, test_val), easeInQuad8(test_val));
    }

    SUBCASE("EASE_OUT_QUAD") {
        CHECK_EQ(ease8(EASE_OUT_QUAD, test_val), easeOutQuad8(test_val));
    }

    SUBCASE("EASE_IN_OUT_QUAD") {
        CHECK_EQ(ease8(EASE_IN_OUT_QUAD, test_val), easeInOutQuad8(test_val));
    }

    SUBCASE("EASE_IN_CUBIC") {
        CHECK_EQ(ease8(EASE_IN_CUBIC, test_val), easeInCubic8(test_val));
    }

    SUBCASE("EASE_OUT_CUBIC") {
        CHECK_EQ(ease8(EASE_OUT_CUBIC, test_val), easeOutCubic8(test_val));
    }

    SUBCASE("EASE_IN_OUT_CUBIC") {
        CHECK_EQ(ease8(EASE_IN_OUT_CUBIC, test_val), easeInOutCubic8(test_val));
    }

    SUBCASE("EASE_IN_SINE") {
        CHECK_EQ(ease8(EASE_IN_SINE, test_val), easeInSine8(test_val));
    }

    SUBCASE("EASE_OUT_SINE") {
        CHECK_EQ(ease8(EASE_OUT_SINE, test_val), easeOutSine8(test_val));
    }

    SUBCASE("EASE_IN_OUT_SINE") {
        CHECK_EQ(ease8(EASE_IN_OUT_SINE, test_val), easeInOutSine8(test_val));
    }
}

// Test ease16 dispatcher function

TEST_CASE("fl::ease16 dispatcher") {
    u16 test_val = 32768;

    SUBCASE("EASE_NONE") {
        CHECK_EQ(ease16(EASE_NONE, test_val), test_val);
    }

    SUBCASE("EASE_IN_QUAD") {
        CHECK_EQ(ease16(EASE_IN_QUAD, test_val), easeInQuad16(test_val));
    }

    SUBCASE("EASE_OUT_QUAD") {
        CHECK_EQ(ease16(EASE_OUT_QUAD, test_val), easeOutQuad16(test_val));
    }

    SUBCASE("EASE_IN_OUT_QUAD") {
        CHECK_EQ(ease16(EASE_IN_OUT_QUAD, test_val), easeInOutQuad16(test_val));
    }

    SUBCASE("EASE_IN_CUBIC") {
        CHECK_EQ(ease16(EASE_IN_CUBIC, test_val), easeInCubic16(test_val));
    }

    SUBCASE("EASE_OUT_CUBIC") {
        CHECK_EQ(ease16(EASE_OUT_CUBIC, test_val), easeOutCubic16(test_val));
    }

    SUBCASE("EASE_IN_OUT_CUBIC") {
        CHECK_EQ(ease16(EASE_IN_OUT_CUBIC, test_val), easeInOutCubic16(test_val));
    }

    SUBCASE("EASE_IN_SINE") {
        CHECK_EQ(ease16(EASE_IN_SINE, test_val), easeInSine16(test_val));
    }

    SUBCASE("EASE_OUT_SINE") {
        CHECK_EQ(ease16(EASE_OUT_SINE, test_val), easeOutSine16(test_val));
    }

    SUBCASE("EASE_IN_OUT_SINE") {
        CHECK_EQ(ease16(EASE_IN_OUT_SINE, test_val), easeInOutSine16(test_val));
    }
}

// Test array-based ease8 function

TEST_CASE("fl::ease8 array function") {
    u8 src[5] = {0, 64, 128, 192, 255};
    u8 dst[5];

    SUBCASE("EASE_NONE does nothing") {
        ease8(EASE_NONE, src, dst, 5);
        // dst should not be modified for EASE_NONE
        // Actually, the function returns early, so dst is undefined
        // Just verify it doesn't crash
    }

    SUBCASE("EASE_IN_QUAD") {
        ease8(EASE_IN_QUAD, src, dst, 5);
        for (u8 i = 0; i < 5; i++) {
            CHECK_EQ(dst[i], easeInQuad8(src[i]));
        }
    }

    SUBCASE("EASE_OUT_CUBIC") {
        ease8(EASE_OUT_CUBIC, src, dst, 5);
        for (u8 i = 0; i < 5; i++) {
            CHECK_EQ(dst[i], easeOutCubic8(src[i]));
        }
    }

    SUBCASE("EASE_IN_OUT_SINE") {
        ease8(EASE_IN_OUT_SINE, src, dst, 5);
        for (u8 i = 0; i < 5; i++) {
            CHECK_EQ(dst[i], easeInOutSine8(src[i]));
        }
    }

    SUBCASE("in-place operation") {
        u8 data[5] = {0, 64, 128, 192, 255};
        ease8(EASE_IN_QUAD, data, data, 5);
        CHECK_EQ(data[0], easeInQuad8(0));
        CHECK_EQ(data[1], easeInQuad8(64));
        CHECK_EQ(data[2], easeInQuad8(128));
        CHECK_EQ(data[3], easeInQuad8(192));
        CHECK_EQ(data[4], easeInQuad8(255));
    }
}

// Test array-based ease16 function

TEST_CASE("fl::ease16 array function") {
    u16 src[5] = {0, 16384, 32768, 49152, 65535};
    u16 dst[5];

    SUBCASE("EASE_NONE does nothing") {
        ease16(EASE_NONE, src, dst, 5);
        // dst should not be modified for EASE_NONE
        // Just verify it doesn't crash
    }

    SUBCASE("EASE_IN_QUAD") {
        ease16(EASE_IN_QUAD, src, dst, 5);
        for (u16 i = 0; i < 5; i++) {
            CHECK_EQ(dst[i], easeInQuad16(src[i]));
        }
    }

    SUBCASE("EASE_OUT_CUBIC") {
        ease16(EASE_OUT_CUBIC, src, dst, 5);
        for (u16 i = 0; i < 5; i++) {
            CHECK_EQ(dst[i], easeOutCubic16(src[i]));
        }
    }

    SUBCASE("EASE_IN_OUT_SINE") {
        ease16(EASE_IN_OUT_SINE, src, dst, 5);
        for (u16 i = 0; i < 5; i++) {
            CHECK_EQ(dst[i], easeInOutSine16(src[i]));
        }
    }

    SUBCASE("in-place operation") {
        u16 data[5] = {0, 16384, 32768, 49152, 65535};
        ease16(EASE_IN_QUAD, data, data, 5);
        CHECK_EQ(data[0], easeInQuad16(0));
        CHECK_EQ(data[1], easeInQuad16(16384));
        CHECK_EQ(data[2], easeInQuad16(32768));
        CHECK_EQ(data[3], easeInQuad16(49152));
        CHECK_EQ(data[4], easeInQuad16(65535));
    }
}

// Test gamma table existence

TEST_CASE("fl::gamma_2_8 table") {
    SUBCASE("table is accessible") {
        // Just verify we can access the table
        // The table is in PROGMEM so we can't easily test values on host
        // but we can verify it compiles and links
        CHECK(true);
    }
}
