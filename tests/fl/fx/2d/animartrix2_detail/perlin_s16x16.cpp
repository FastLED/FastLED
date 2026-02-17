// Unit test for Perlin noise s16x16 implementations (scalar vs SIMD)
// This test was created to debug a bug where SIMD returns different results
// than scalar for the same input coordinates.

#include "test.h"
#include "fl/fx/2d/animartrix2_detail/perlin_s16x16.h"
#include "fl/fx/2d/animartrix2_detail/perlin_s16x16_simd.h"
#include <stdio.h>  // ok include: needed for fprintf debug output

using namespace fl;

// Standard Perlin permutation table
static const fl::u8 perm_table[256] = {
    151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,
    8,99,37,240,21,10,23,190,6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,
    35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,74,165,71,
    134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,
    55,46,245,40,244,102,143,54,65,25,63,161,1,216,80,73,209,76,132,187,208,89,
    18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,52,217,226,
    250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,
    189,28,42,223,183,170,213,119,248,152,2,44,154,163,70,221,153,101,155,167,43,
    172,9,129,22,39,253,19,98,108,110,79,113,224,232,178,185,112,104,218,246,97,
    228,251,34,242,193,238,210,144,12,191,179,162,241,81,51,145,235,249,14,239,
    107,49,192,214,31,181,199,106,157,184,84,204,176,115,121,50,45,127,4,150,254,
    138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
};

FL_TEST_CASE("perlin_s16x16 - scalar vs SIMD single point") {
    // Initialize fade LUT
    fl::i32 fade_lut[257];
    perlin_s16x16::init_fade_lut(fade_lut);

    // Test coordinates that triggered the bug in chasing_spirals
    // From debug output: nx=3155921, ny=3313496
    // Scalar result: -8740
    // SIMD result: 56796
    fl::i32 nx = 3155921;
    fl::i32 ny = 3313496;

    // Scalar evaluation
    fl::i32 scalar_result = perlin_s16x16::pnoise2d_raw(nx, ny, fade_lut, perm_table);

    // SIMD evaluation (batch of 4, we only care about first element)
    fl::i32 nx_batch[4] = {nx, 0, 0, 0};
    fl::i32 ny_batch[4] = {ny, 0, 0, 0};
    fl::i32 simd_result[4];
    perlin_s16x16_simd::pnoise2d_raw_simd4(nx_batch, ny_batch, fade_lut, perm_table, simd_result);

    // Print results for debugging
    fprintf(stderr, "\n=== Perlin s16x16 Test (nx=%d, ny=%d) ===\n", nx, ny);
    fprintf(stderr, "Scalar result: %d\n", scalar_result);
    fprintf(stderr, "SIMD result:   %d\n", simd_result[0]);
    fprintf(stderr, "Difference:    %d\n", simd_result[0] - scalar_result);
    fflush(stderr);

    // They should match exactly
    if (scalar_result != simd_result[0]) {
        fprintf(stderr, "❌ FAILURE: SIMD result does not match scalar! Scalar=%d, SIMD=%d, diff=%d\n",
                scalar_result, simd_result[0], simd_result[0] - scalar_result);
        fflush(stderr);
        FL_ASSERT(false, "SIMD result does not match scalar");
    }
}

FL_TEST_CASE("perlin_s16x16 - scalar vs SIMD batch") {
    // Initialize fade LUT
    fl::i32 fade_lut[257];
    perlin_s16x16::init_fade_lut(fade_lut);

    // Test multiple coordinates from the chasing_spirals debug output
    fl::i32 nx_batch[4] = {3155921, 3240935, 3419913, 3278154};
    fl::i32 ny_batch[4] = {3313496, 3148060, 3302123, 3266872};

    // Expected scalar results (from debug output)
    fl::i32 expected[4] = {-8740, 17879, -6960, 9452};

    // SIMD evaluation
    fl::i32 simd_result[4];
    perlin_s16x16_simd::pnoise2d_raw_simd4(nx_batch, ny_batch, fade_lut, perm_table, simd_result);

    fprintf(stderr, "\n=== Perlin s16x16 Batch Test ===\n");
    for (int i = 0; i < 4; i++) {
        // Compute scalar result for comparison
        fl::i32 scalar_result = perlin_s16x16::pnoise2d_raw(nx_batch[i], ny_batch[i], fade_lut, perm_table);

        fprintf(stderr, "Point %d: nx=%d, ny=%d\n", i, nx_batch[i], ny_batch[i]);
        fprintf(stderr, "  Scalar: %d (expected: %d)\n", scalar_result, expected[i]);
        fprintf(stderr, "  SIMD:   %d\n", simd_result[i]);
        fprintf(stderr, "  Diff:   %d\n", simd_result[i] - scalar_result);
        fflush(stderr);

        // Verify scalar matches expected
        if (scalar_result != expected[i]) {
            fprintf(stderr, "❌ Point %d: Scalar result %d does not match expected %d\n",
                    i, scalar_result, expected[i]);
            fflush(stderr);
            FL_ASSERT(false, "Scalar result does not match expected");
        }

        // Verify SIMD matches scalar
        if (scalar_result != simd_result[i]) {
            fprintf(stderr, "❌ Point %d: SIMD result %d does not match scalar %d (diff=%d)\n",
                    i, simd_result[i], scalar_result, simd_result[i] - scalar_result);
            fflush(stderr);
            FL_ASSERT(false, "SIMD result does not match scalar");
        }
    }
}

FL_TEST_CASE("perlin_s16x16 - various coordinates") {
    // Initialize fade LUT
    fl::i32 fade_lut[257];
    perlin_s16x16::init_fade_lut(fade_lut);

    // Test a variety of coordinates
    struct TestCase {
        fl::i32 nx, ny;
        const char *desc;
    };

    TestCase tests[] = {
        {0, 0, "Origin"},
        {65536, 65536, "1.0, 1.0"},
        {32768, 32768, "0.5, 0.5"},
        {-65536, -65536, "-1.0, -1.0"},
        {100000, 200000, "Arbitrary positive"},
        {-100000, -200000, "Arbitrary negative"},
        {3155921, 3313496, "Bug trigger case"},
    };

    fprintf(stderr, "\n=== Perlin s16x16 Various Coordinates Test ===\n");

    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        fl::i32 nx = tests[i].nx;
        fl::i32 ny = tests[i].ny;

        // Scalar evaluation
        fl::i32 scalar_result = perlin_s16x16::pnoise2d_raw(nx, ny, fade_lut, perm_table);

        // SIMD evaluation
        fl::i32 nx_batch[4] = {nx, 0, 0, 0};
        fl::i32 ny_batch[4] = {ny, 0, 0, 0};
        fl::i32 simd_result[4];
        perlin_s16x16_simd::pnoise2d_raw_simd4(nx_batch, ny_batch, fade_lut, perm_table, simd_result);

        fprintf(stderr, "Test %zu: %s (nx=%d, ny=%d)\n", i, tests[i].desc, nx, ny);
        fprintf(stderr, "  Scalar: %d\n", scalar_result);
        fprintf(stderr, "  SIMD:   %d\n", simd_result[0]);

        if (scalar_result != simd_result[0]) {
            fprintf(stderr, "  ❌ MISMATCH! Diff=%d\n", simd_result[0] - scalar_result);
        } else {
            fprintf(stderr, "  ✓ Match\n");
        }
        fflush(stderr);

        if (scalar_result != simd_result[0]) {
            fprintf(stderr, "❌ Test %zu (%s): SIMD result %d does not match scalar %d\n",
                    i, tests[i].desc, simd_result[0], scalar_result);
            fflush(stderr);
            FL_ASSERT(false, "SIMD result does not match scalar");
        }
    }
}
