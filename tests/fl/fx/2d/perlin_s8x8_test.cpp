// ok standalone - tests implementation details of perlin noise variants
#include "fl/fx/2d/animartrix2.hpp"
#include "fl/fx/2d/animartrix.hpp"
#include "test.h"

using namespace fl;

FL_TEST_CASE("perlin_s8x8 - basic functionality") {
    // Initialize fade LUT
    i32 fade_lut[257];
    fl::perlin_s8x8::init_fade_lut(fade_lut);

    // Check LUT values are reasonable
    FL_CHECK(fade_lut[0] == 0);  // At t=0, fade should be 0
    FL_CHECK(fade_lut[256] > 0); // At t=1, fade should be > 0

    // Test perlin noise with known coordinates
    const u8 *perm = animartrix_detail::PERLIN_NOISE;
    s16x16 fx(1.5f);
    s16x16 fy(2.3f);

    i32 result = fl::perlin_s8x8::pnoise2d_raw(
        fx.raw(), fy.raw(), fade_lut, perm);

    // Result should be in valid range for s16x16
    FL_CHECK(result >= -(1 << 16));
    FL_CHECK(result <= (1 << 16));

    FL_MESSAGE("perlin_s8x8 basic test passed - result at (1.5, 2.3) = ", result);
}

FL_TEST_CASE("perlin_s8x8 vs perlin_s16x16 - consistency check") {
    // Compare Q8 vs Q24 output
    i32 fade_lut_q8[257];
    i32 fade_lut_q24[257];

    fl::perlin_s8x8::init_fade_lut(fade_lut_q8);
    fl::perlin_s16x16::init_fade_lut(fade_lut_q24);

    const u8 *perm = animartrix_detail::PERLIN_NOISE;

    // Test a few coordinates
    s16x16 test_coords[] = {
        s16x16(0.0f), s16x16(0.5f), s16x16(1.0f),
        s16x16(1.5f), s16x16(2.0f), s16x16(10.5f)
    };

    int max_diff = 0;
    for (auto fx : test_coords) {
        for (auto fy : test_coords) {
            i32 result_q8 = fl::perlin_s8x8::pnoise2d_raw(
                fx.raw(), fy.raw(), fade_lut_q8, perm);
            i32 result_q24 = fl::perlin_s16x16::pnoise2d_raw(
                fx.raw(), fy.raw(), fade_lut_q24, perm);

            int diff = static_cast<int>(result_q8 - result_q24);
            if (diff < 0) diff = -diff;
            if (diff > max_diff) max_diff = diff;
        }
    }

    FL_MESSAGE("perlin_s8x8 vs perlin_s16x16 max difference: ", max_diff);
    FL_MESSAGE("  (", static_cast<float>(max_diff) / 655.36f, "% of full range)");

    // Q8 should be reasonably close to Q24 (within 10% of range)
    FL_CHECK_MESSAGE(max_diff < 6554,  // ~10% of 65536
                  "Q8 perlin should be within 10% of Q24 perlin");
}
