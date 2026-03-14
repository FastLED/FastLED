// ok cpp include
/// @file blur_gaussian.cpp
/// @brief Tests for SKIPSM Gaussian blur

#include "test.h"
#include "fl/gfx/blur.h"
#include "fl/gfx/crgb.h"
#include "fl/gfx/crgb16.h"

using namespace fl;

FL_TEST_CASE("blurGaussian 3x3 - uniform interior unchanged") {
    // Center pixel of a uniform image should be unchanged after blur.
    // Edge pixels may darken due to missing out-of-bounds neighbors.
    const int W = 5, H = 5;
    CRGB pixels[W * H];
    for (int i = 0; i < W * H; ++i) {
        pixels[i] = CRGB(100, 100, 100);
    }

    gfx::Canvas<CRGB> canvas(fl::span<CRGB>(pixels, W * H), W, H);
    gfx::blurGaussian<1, 1>(canvas);

    // Interior pixels (1 pixel from edge) should be exactly preserved.
    for (int y = 1; y < H - 1; ++y) {
        for (int x = 1; x < W - 1; ++x) {
            FL_CHECK_EQ(pixels[y * W + x].r, 100);
            FL_CHECK_EQ(pixels[y * W + x].g, 100);
            FL_CHECK_EQ(pixels[y * W + x].b, 100);
        }
    }
}

FL_TEST_CASE("blurGaussian 3x3 - single bright pixel spreads") {
    // A single bright pixel in the center of a black image should spread.
    const int W = 5, H = 5;
    CRGB pixels[W * H];
    for (int i = 0; i < W * H; ++i) {
        pixels[i] = CRGB(0, 0, 0);
    }
    // Center pixel at (2,2)
    pixels[2 * W + 2] = CRGB(255, 0, 0);

    gfx::Canvas<CRGB> canvas(fl::span<CRGB>(pixels, W * H), W, H);
    gfx::blurGaussian<1, 1>(canvas);

    int cx = 2 * W + 2;
    // Center should be dimmer than 255 (spread to neighbors).
    FL_CHECK(pixels[cx].r < 255);
    FL_CHECK(pixels[cx].r > 0);

    // Cardinal neighbors should have picked up some red.
    FL_CHECK(pixels[1 * W + 2].r > 0); // top
    FL_CHECK(pixels[2 * W + 1].r > 0); // left
    FL_CHECK(pixels[2 * W + 3].r > 0); // right
    FL_CHECK(pixels[3 * W + 2].r > 0); // bottom

    // Green and blue should remain zero everywhere.
    for (int i = 0; i < W * H; ++i) {
        FL_CHECK_EQ(pixels[i].g, 0);
        FL_CHECK_EQ(pixels[i].b, 0);
    }
}

FL_TEST_CASE("blurGaussian - dim factor reduces brightness") {
    const int W = 5, H = 5;
    CRGB pixels[W * H];
    for (int i = 0; i < W * H; ++i) {
        pixels[i] = CRGB(200, 200, 200);
    }

    gfx::Canvas<CRGB> canvas(fl::span<CRGB>(pixels, W * H), W, H);
    gfx::blurGaussian<1, 1>(canvas, alpha8(128)); // ~50% brightness

    // Interior pixel should be dimmed.
    int center = 2 * W + 2;
    FL_CHECK(pixels[center].r < 200);
    FL_CHECK(pixels[center].r > 0);
}

FL_TEST_CASE("blurGaussian - horizontal only") {
    const int W = 5, H = 5;
    CRGB pixels[W * H];
    for (int i = 0; i < W * H; ++i) {
        pixels[i] = CRGB(0, 0, 0);
    }
    // Center pixel at (2,2)
    pixels[2 * W + 2] = CRGB(255, 0, 0);

    gfx::Canvas<CRGB> canvas(fl::span<CRGB>(pixels, W * H), W, H);
    gfx::blurGaussian<1, 0>(canvas); // horizontal only

    // Horizontal neighbors should have red.
    FL_CHECK(pixels[2 * W + 1].r > 0); // left
    FL_CHECK(pixels[2 * W + 3].r > 0); // right

    // Vertical neighbors should remain black (no vertical blur).
    FL_CHECK_EQ(pixels[1 * W + 2].r, 0); // top
    FL_CHECK_EQ(pixels[3 * W + 2].r, 0); // bottom
}

FL_TEST_CASE("blurGaussian 5x5 kernel - interior unchanged") {
    // Use a large enough grid so interior pixels have full kernel coverage.
    const int W = 9, H = 9;
    CRGB pixels[W * H];
    for (int i = 0; i < W * H; ++i) {
        pixels[i] = CRGB(128, 64, 32);
    }

    gfx::Canvas<CRGB> canvas(fl::span<CRGB>(pixels, W * H), W, H);
    gfx::blurGaussian<2, 2>(canvas);

    // Interior pixels (radius=2 from edge) should be exactly preserved.
    for (int y = 2; y < H - 2; ++y) {
        for (int x = 2; x < W - 2; ++x) {
            FL_CHECK_EQ(pixels[y * W + x].r, 128);
            FL_CHECK_EQ(pixels[y * W + x].g, 64);
            FL_CHECK_EQ(pixels[y * W + x].b, 32);
        }
    }
}

FL_TEST_CASE("blurGaussian 7x7 and 9x9 kernels") {
    // Use a 20x20 grid so interior pixels are far from edges.
    const int W = 20, H = 20;
    CRGB pixels[W * H];
    for (int i = 0; i < W * H; ++i) {
        pixels[i] = CRGB(50, 50, 50);
    }

    gfx::Canvas<CRGB> canvas(fl::span<CRGB>(pixels, W * H), W, H);

    // 7x7 (radius=3)
    gfx::blurGaussian<3, 3>(canvas);
    // Interior pixel should be preserved.
    FL_CHECK_EQ(pixels[10 * W + 10].r, 50);

    // Reset
    for (int i = 0; i < W * H; ++i) {
        pixels[i] = CRGB(50, 50, 50);
    }

    // 9x9 (radius=4)
    gfx::blurGaussian<4, 4>(canvas);
    FL_CHECK_EQ(pixels[10 * W + 10].r, 50);
}

// ── CRGB16 tests ─────────────────────────────────────────────────────────

FL_TEST_CASE("blurGaussian CRGB16 3x3 - uniform interior unchanged") {
    const int W = 5, H = 5;
    CRGB16 pixels[W * H];
    u8x8 val(100);
    for (int i = 0; i < W * H; ++i) {
        pixels[i] = CRGB16(val, val, val);
    }

    gfx::Canvas<CRGB16> canvas(fl::span<CRGB16>(pixels, W * H), W, H);
    gfx::blurGaussian<1, 1>(canvas);

    // Interior pixels should be exactly preserved.
    for (int y = 1; y < H - 1; ++y) {
        for (int x = 1; x < W - 1; ++x) {
            FL_CHECK_EQ(pixels[y * W + x].r.raw(), val.raw());
            FL_CHECK_EQ(pixels[y * W + x].g.raw(), val.raw());
            FL_CHECK_EQ(pixels[y * W + x].b.raw(), val.raw());
        }
    }
}

FL_TEST_CASE("blurGaussian CRGB16 3x3 - single bright pixel spreads") {
    const int W = 5, H = 5;
    CRGB16 pixels[W * H];
    u8x8 zero(0);
    for (int i = 0; i < W * H; ++i) {
        pixels[i] = CRGB16(zero, zero, zero);
    }
    // Center pixel at (2,2) with max red
    pixels[2 * W + 2] = CRGB16(u8x8(255), zero, zero);

    gfx::Canvas<CRGB16> canvas(fl::span<CRGB16>(pixels, W * H), W, H);
    gfx::blurGaussian<1, 1>(canvas);

    int cx = 2 * W + 2;
    // Center should be dimmer (spread to neighbors).
    FL_CHECK(pixels[cx].r.raw() < u8x8(255).raw());
    FL_CHECK(pixels[cx].r.raw() > 0);

    // Cardinal neighbors should have picked up some red.
    FL_CHECK(pixels[1 * W + 2].r.raw() > 0); // top
    FL_CHECK(pixels[2 * W + 1].r.raw() > 0); // left
    FL_CHECK(pixels[2 * W + 3].r.raw() > 0); // right
    FL_CHECK(pixels[3 * W + 2].r.raw() > 0); // bottom

    // Green and blue should remain zero everywhere.
    for (int i = 0; i < W * H; ++i) {
        FL_CHECK_EQ(pixels[i].g.raw(), 0);
        FL_CHECK_EQ(pixels[i].b.raw(), 0);
    }
}

FL_TEST_CASE("blurGaussian CRGB16 - high-precision dim factor") {
    const int W = 5, H = 5;
    CRGB16 pixels[W * H];
    u8x8 val(200);
    for (int i = 0; i < W * H; ++i) {
        pixels[i] = CRGB16(val, val, val);
    }

    gfx::Canvas<CRGB16> canvas(fl::span<CRGB16>(pixels, W * H), W, H);
    // Dim by ~50% using alpha16 for higher precision
    gfx::blurGaussian<1, 1>(canvas, alpha16(32768));

    // Interior pixel should be roughly half brightness.
    int center = 2 * W + 2;
    FL_CHECK(pixels[center].r.raw() < val.raw());
    FL_CHECK(pixels[center].r.raw() > 0);
    // Should be approximately val * 0.5 = 100.0 → raw ~25600
    u16 expected_approx = val.raw() / 2;
    u16 actual = pixels[center].r.raw();
    // Allow ±5% tolerance for fixed-point rounding
    FL_CHECK(actual > expected_approx * 9 / 10);
    FL_CHECK(actual < expected_approx * 11 / 10);
}

FL_TEST_CASE("blurGaussian CRGB16 5x5 kernel - interior unchanged") {
    const int W = 9, H = 9;
    CRGB16 pixels[W * H];
    u8x8 rv(128), gv(64), bv(32);
    for (int i = 0; i < W * H; ++i) {
        pixels[i] = CRGB16(rv, gv, bv);
    }

    gfx::Canvas<CRGB16> canvas(fl::span<CRGB16>(pixels, W * H), W, H);
    gfx::blurGaussian<2, 2>(canvas);

    // Interior pixels (radius=2 from edge) should be preserved.
    for (int y = 2; y < H - 2; ++y) {
        for (int x = 2; x < W - 2; ++x) {
            FL_CHECK_EQ(pixels[y * W + x].r.raw(), rv.raw());
            FL_CHECK_EQ(pixels[y * W + x].g.raw(), gv.raw());
            FL_CHECK_EQ(pixels[y * W + x].b.raw(), bv.raw());
        }
    }
}

FL_TEST_FILE(FL_FILEPATH) {

} // FL_TEST_FILE
