// ok cpp include
/// @file blur.cpp
/// @brief Tests for SKIPSM Gaussian blur

#include "test.h"
#include "fl/gfx/blur.h"
#include "fl/gfx/crgb.h"
#include "fl/gfx/crgb16.h"
#include "fl/math/xymap.h"
#include "fl/stl/chrono.h"
// Note: fl/stl/cstdio.h intentionally NOT included — workaround for
// zackees/zccache#619 (Windows PCH path-spelling drift defeats #pragma
// once across the PCH boundary). The PCH provides cstdio.h transitively
// via ostream.h, so the symbols this test uses (fl::print etc.) stay in
// scope.
#include "fl/stl/stdio.h"

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

// ── Benchmark ────────────────────────────────────────────────────────────

static void fill_test_data(CRGB *pixels, int n) {
    for (int i = 0; i < n; ++i) {
        pixels[i] = CRGB(static_cast<uint8_t>(i * 37 + 17),
                         static_cast<uint8_t>(i * 59 + 31),
                         static_cast<uint8_t>(i * 83 + 47));
    }
}

template <typename Func>
#ifdef NDEBUG
__attribute__((noinline)) static double bench(Func fn, int iterations, int warmup = 200) {
#else
__attribute__((noinline)) static double bench(Func fn, int iterations, int warmup = 2) {
#endif
    for (int i = 0; i < warmup; ++i)
        fn();
    fl::u32 t0 = fl::micros();
    for (int i = 0; i < iterations; ++i)
        fn();
    fl::u32 t1 = fl::micros();
    return static_cast<double>(t1 - t0) * 1000.0 /
           static_cast<double>(iterations); // ns per iteration
}

template <int R, int W, int H>
__attribute__((noinline)) static void run_benchmark(const char *label, int iters) {
    constexpr int N = W * H;
    static CRGB px[N]; // static to avoid stack overflow with ASAN

    auto t = bench(
        [&]() {
            fill_test_data(px, N);
            gfx::Canvas<CRGB> c(fl::span<CRGB>(px, N), W, H);
            gfx::blurGaussian<R, R>(c);
        },
        iters);

    char buf[80];
    fl::snprintf(buf, sizeof(buf), "  %s: %d ns/iter", label,
             static_cast<int>(t));
    fl::println(buf);
}

template <int R, int W, int H>
__attribute__((noinline)) static void run_benchmark_blur_only(const char *label, int iters) {
    constexpr int N = W * H;
    static CRGB px[N]; // static to avoid stack overflow with ASAN
    fill_test_data(px, N); // fill once before benchmark

    auto t = bench(
        [&]() {
            gfx::Canvas<CRGB> c(fl::span<CRGB>(px, N), W, H);
            gfx::blurGaussian<R, R>(c);
        },
        iters);

    char buf[80];
    fl::snprintf(buf, sizeof(buf), "  %s: %d ns/iter", label,
             static_cast<int>(t));
    fl::println(buf);
}

FL_TEST_CASE("blur benchmark") {
    // Debug builds use -Og (optimized for debugging) which provides enough
    // optimization for benchmarks to complete within the 20s CI watchdog.
#ifdef NDEBUG
    const int ITERS = 5000;
#else
    const int ITERS = 200;
#endif

    fl::println("\n── Gaussian Blur Benchmark (fill + blur) ──\n");

    run_benchmark<1, 16, 16>("16x16 R1", ITERS);
    run_benchmark<2, 16, 16>("16x16 R2", ITERS);
    run_benchmark<3, 16, 16>("16x16 R3", ITERS);
    run_benchmark<4, 16, 16>("16x16 R4", ITERS);

    fl::println("");

    run_benchmark<1, 64, 64>("64x64 R1", ITERS / 2);
    run_benchmark<2, 64, 64>("64x64 R2", ITERS / 2);
    run_benchmark<3, 64, 64>("64x64 R3", ITERS / 2);
    run_benchmark<4, 64, 64>("64x64 R4", ITERS / 2);

    fl::println("\n── Gaussian Blur Benchmark (blur only) ──\n");

    run_benchmark_blur_only<1, 16, 16>("16x16 R1", ITERS);
    run_benchmark_blur_only<2, 16, 16>("16x16 R2", ITERS);
    run_benchmark_blur_only<4, 16, 16>("16x16 R4", ITERS);

    fl::println("");

    run_benchmark_blur_only<1, 64, 64>("64x64 R1", ITERS / 2);
    run_benchmark_blur_only<2, 64, 64>("64x64 R2", ITERS / 2);
    run_benchmark_blur_only<3, 64, 64>("64x64 R3", ITERS / 2);
    run_benchmark_blur_only<4, 64, 64>("64x64 R4", ITERS / 2);

    fl::println("\n── Larger images (blur only) ──\n");

    run_benchmark_blur_only<1, 128, 128>("128x128 R1", ITERS / 4);
    run_benchmark_blur_only<2, 128, 128>("128x128 R2", ITERS / 4);
    run_benchmark_blur_only<3, 128, 128>("128x128 R3", ITERS / 4);
    run_benchmark_blur_only<4, 128, 128>("128x128 R4", ITERS / 4);

    fl::println("");

    run_benchmark_blur_only<1, 256, 256>("256x256 R1", ITERS / 8);
    run_benchmark_blur_only<2, 256, 256>("256x256 R2", ITERS / 8);
    run_benchmark_blur_only<3, 256, 256>("256x256 R3", ITERS / 8);
    run_benchmark_blur_only<4, 256, 256>("256x256 R4", ITERS / 8);

    fl::println("\n── H-only vs V-only (blur only, 64x64) ──\n");

    // Horizontal only (hR=R, vR=0)
    {
        constexpr int N = 64*64;
        static CRGB px[N];
        fill_test_data(px, N);
        auto t = bench([&]() {
            gfx::Canvas<CRGB> c(fl::span<CRGB>(px, N), 64, 64);
            gfx::blurGaussian<1, 0>(c);
        }, ITERS/2);
        char buf[80];
        fl::snprintf(buf, sizeof(buf), "  H-only R1: %d ns", (int)t);
        fl::println(buf);
    }
    // Vertical only (hR=0, vR=1)
    {
        constexpr int N = 64*64;
        static CRGB px[N];
        fill_test_data(px, N);
        auto t = bench([&]() {
            gfx::Canvas<CRGB> c(fl::span<CRGB>(px, N), 64, 64);
            gfx::blurGaussian<0, 1>(c);
        }, ITERS/2);
        char buf[80];
        fl::snprintf(buf, sizeof(buf), "  V-only R1: %d ns", (int)t);
        fl::println(buf);
    }
    // H+V R1
    {
        constexpr int N = 64*64;
        static CRGB px[N];
        fill_test_data(px, N);
        auto t = bench([&]() {
            gfx::Canvas<CRGB> c(fl::span<CRGB>(px, N), 64, 64);
            gfx::blurGaussian<1, 1>(c);
        }, ITERS/2);
        char buf[80];
        fl::snprintf(buf, sizeof(buf), "  H+V   R1: %d ns", (int)t);
        fl::println(buf);
    }

    // H-only and V-only R=2 (64x64)
    {
        constexpr int N = 64*64;
        static CRGB px[N];
        fill_test_data(px, N);
        auto t = bench([&]() {
            gfx::Canvas<CRGB> c(fl::span<CRGB>(px, N), 64, 64);
            gfx::blurGaussian<2, 0>(c);
        }, ITERS/2);
        char buf[80];
        fl::snprintf(buf, sizeof(buf), "  H-only R2: %d ns", (int)t);
        fl::println(buf);
    }
    {
        constexpr int N = 64*64;
        static CRGB px[N];
        fill_test_data(px, N);
        auto t = bench([&]() {
            gfx::Canvas<CRGB> c(fl::span<CRGB>(px, N), 64, 64);
            gfx::blurGaussian<0, 2>(c);
        }, ITERS/2);
        char buf[80];
        fl::snprintf(buf, sizeof(buf), "  V-only R2: %d ns", (int)t);
        fl::println(buf);
    }
    // H-only and V-only R=2 (256x256)
    {
        constexpr int N = 256*256;
        static CRGB px[N]; // static to avoid stack overflow with ASAN
        fill_test_data(px, N);
        auto t = bench([&]() {
            gfx::Canvas<CRGB> c(fl::span<CRGB>(px, N), 256, 256);
            gfx::blurGaussian<2, 0>(c);
        }, ITERS/8);
        char buf[80];
        fl::snprintf(buf, sizeof(buf), "  H-only R2 256x256: %d ns", (int)t);
        fl::println(buf);
    }
    {
        constexpr int N = 256*256;
        static CRGB px[N]; // static to avoid stack overflow with ASAN
        fill_test_data(px, N);
        auto t = bench([&]() {
            gfx::Canvas<CRGB> c(fl::span<CRGB>(px, N), 256, 256);
            gfx::blurGaussian<0, 2>(c);
        }, ITERS/8);
        char buf[80];
        fl::snprintf(buf, sizeof(buf), "  V-only R2 256x256: %d ns", (int)t);
        fl::println(buf);
    }

    fl::println("");
}

// ── XYMap-aware blur tests ──────────────────────────────────────────────

FL_TEST_CASE("blur2d with serpentine XYMap produces different result") {
    // Verify that blur2d actually uses the XYMap when it's non-trivial.
    // A serpentine layout reverses every other row, so blurring with
    // serpentine vs rectangular should produce different pixel values
    // when the source data is asymmetric.
    const fl::u8 W = 4, H = 4;
    const int N = W * H;

    // Create asymmetric test data: a gradient along columns.
    CRGB rect_pixels[N];
    CRGB serp_pixels[N];
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            fl::u8 val = static_cast<fl::u8>(x * 60 + y * 10);
            rect_pixels[y * W + x] = CRGB(val, 0, 0);
            serp_pixels[y * W + x] = CRGB(val, 0, 0);
        }
    }

    XYMap rectMap = XYMap::constructRectangularGrid(W, H);
    XYMap serpMap = XYMap::constructSerpentine(W, H);

    fl::gfx::blur2d(fl::span<CRGB>(rect_pixels, N), W, H, 128, rectMap);
    fl::gfx::blur2d(fl::span<CRGB>(serp_pixels, N), W, H, 128, serpMap);

    // At least some pixels must differ between rectangular and serpentine.
    bool any_diff = false;
    for (int i = 0; i < N; ++i) {
        if (rect_pixels[i].r != serp_pixels[i].r ||
            rect_pixels[i].g != serp_pixels[i].g ||
            rect_pixels[i].b != serp_pixels[i].b) {
            any_diff = true;
            break;
        }
    }
    FL_CHECK(any_diff);
}

FL_TEST_CASE("blurRows with custom XYMap uses mapping") {
    // Use a custom XYFunction that interleaves logical row members across
    // physical positions to verify that blurRows uses the XYMap. When a
    // logical row's pixels are scattered in physical memory, the blur
    // reads different physical data than the rectangular case.
    const fl::u8 W = 4, H = 2;
    const int N = W * H;

    // Interleaved mapping: logical (x, y) → physical index 2*x + y.
    // Row 0 maps to even physical indices: 0, 2, 4, 6.
    // Row 1 maps to odd physical indices: 1, 3, 5, 7.
    auto interleave = [](fl::u16 x, fl::u16 y, fl::u16 w, fl::u16 h) -> fl::u16 {
        (void)w; (void)h;
        return static_cast<fl::u16>(2 * x + y);
    };

    CRGB rect_pixels[N];
    CRGB inter_pixels[N];
    // Fill physical pixels with unique ascending values.
    for (int i = 0; i < N; ++i) {
        fl::u8 val = static_cast<fl::u8>(i * 30 + 10);
        rect_pixels[i] = CRGB(val, 0, 0);
        inter_pixels[i] = CRGB(val, 0, 0);
    }

    XYMap rectMap = XYMap::constructRectangularGrid(W, H);
    XYMap interMap = XYMap::constructWithUserFunction(W, H, interleave);

    fl::gfx::blurRows(fl::span<CRGB>(rect_pixels, N), W, H, 128, rectMap);
    fl::gfx::blurRows(fl::span<CRGB>(inter_pixels, N), W, H, 128, interMap);

    // With rectangular: row 0 blurs phys[0..3], row 1 blurs phys[4..7].
    // With interleaved: row 0 blurs phys[0,2,4,6], row 1 blurs phys[1,3,5,7].
    // These access different physical pixels so the results must differ.
    bool any_diff = false;
    for (int i = 0; i < N; ++i) {
        if (rect_pixels[i].r != inter_pixels[i].r) {
            any_diff = true;
            break;
        }
    }
    FL_CHECK(any_diff);
}

FL_TEST_CASE("blurColumns with serpentine XYMap uses mapping") {
    const fl::u8 W = 4, H = 4;
    const int N = W * H;

    CRGB rect_pixels[N];
    CRGB serp_pixels[N];
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            fl::u8 val = static_cast<fl::u8>(x * 60 + y * 10);
            rect_pixels[y * W + x] = CRGB(val, 0, 0);
            serp_pixels[y * W + x] = CRGB(val, 0, 0);
        }
    }

    XYMap rectMap = XYMap::constructRectangularGrid(W, H);
    XYMap serpMap = XYMap::constructSerpentine(W, H);

    fl::gfx::blurColumns(fl::span<CRGB>(rect_pixels, N), W, H, 128, rectMap);
    fl::gfx::blurColumns(fl::span<CRGB>(serp_pixels, N), W, H, 128, serpMap);

    bool any_diff = false;
    for (int i = 0; i < N; ++i) {
        if (rect_pixels[i].r != serp_pixels[i].r) {
            any_diff = true;
            break;
        }
    }
    FL_CHECK(any_diff);
}

FL_TEST_CASE("blur2d with rectangular XYMap matches Canvas path") {
    // Verify backward compatibility: rectangular XYMap should produce
    // the same result as the Canvas-based path.
    const fl::u8 W = 8, H = 8;
    const int N = W * H;

    CRGB xymap_pixels[N];
    CRGB canvas_pixels[N];
    for (int i = 0; i < N; ++i) {
        fl::u8 val = static_cast<fl::u8>((i * 37 + 17) & 0xFF);
        xymap_pixels[i] = CRGB(val, val, val);
        canvas_pixels[i] = CRGB(val, val, val);
    }

    XYMap rectMap = XYMap::constructRectangularGrid(W, H);
    fl::gfx::blur2d(fl::span<CRGB>(xymap_pixels, N), W, H, 128, rectMap);

    gfx::Canvas<CRGB> canvas(fl::span<CRGB>(canvas_pixels, N), W, H);
    fl::gfx::blur2d(canvas, alpha8(128));

    for (int i = 0; i < N; ++i) {
        FL_CHECK_EQ(xymap_pixels[i].r, canvas_pixels[i].r);
        FL_CHECK_EQ(xymap_pixels[i].g, canvas_pixels[i].g);
        FL_CHECK_EQ(xymap_pixels[i].b, canvas_pixels[i].b);
    }
}

// ── CanvasMapped tests: verify identical results to Canvas ──────────────

// Helper: fill pixel arrays with deterministic test data.
static void fill_deterministic(CRGB *pixels, int n) {
    for (int i = 0; i < n; ++i) {
        pixels[i] = CRGB(static_cast<uint8_t>((i * 37 + 17) & 0xFF),
                         static_cast<uint8_t>((i * 59 + 31) & 0xFF),
                         static_cast<uint8_t>((i * 83 + 47) & 0xFF));
    }
}

FL_TEST_CASE("CanvasMapped 3x3 - uniform interior unchanged") {
    const int W = 5, H = 5;
    CRGB pixels[W * H];
    for (int i = 0; i < W * H; ++i)
        pixels[i] = CRGB(100, 100, 100);

    XYMap rectMap = XYMap::constructRectangularGrid(W, H);
    gfx::CanvasMapped<CRGB> canvas(fl::span<CRGB>(pixels, W * H), rectMap);
    gfx::blurGaussian<1, 1>(canvas);

    for (int y = 1; y < H - 1; ++y) {
        for (int x = 1; x < W - 1; ++x) {
            FL_CHECK_EQ(pixels[y * W + x].r, 100);
            FL_CHECK_EQ(pixels[y * W + x].g, 100);
            FL_CHECK_EQ(pixels[y * W + x].b, 100);
        }
    }
}

FL_TEST_CASE("CanvasMapped 3x3 - single bright pixel spreads") {
    const int W = 5, H = 5;
    CRGB pixels[W * H];
    for (int i = 0; i < W * H; ++i)
        pixels[i] = CRGB(0, 0, 0);
    pixels[2 * W + 2] = CRGB(255, 0, 0);

    XYMap rectMap = XYMap::constructRectangularGrid(W, H);
    gfx::CanvasMapped<CRGB> canvas(fl::span<CRGB>(pixels, W * H), rectMap);
    gfx::blurGaussian<1, 1>(canvas);

    int cx = 2 * W + 2;
    FL_CHECK(pixels[cx].r < 255);
    FL_CHECK(pixels[cx].r > 0);
    FL_CHECK(pixels[1 * W + 2].r > 0);
    FL_CHECK(pixels[2 * W + 1].r > 0);
    FL_CHECK(pixels[2 * W + 3].r > 0);
    FL_CHECK(pixels[3 * W + 2].r > 0);

    for (int i = 0; i < W * H; ++i) {
        FL_CHECK_EQ(pixels[i].g, 0);
        FL_CHECK_EQ(pixels[i].b, 0);
    }
}

FL_TEST_CASE("CanvasMapped matches Canvas - blurGaussian R1") {
    const int W = 8, H = 8, N = W * H;
    CRGB canvas_px[N], mapped_px[N];
    fill_deterministic(canvas_px, N);
    FL_BUILTIN_MEMCPY(mapped_px, canvas_px, sizeof(canvas_px));

    gfx::Canvas<CRGB> canvas(fl::span<CRGB>(canvas_px, N), W, H);
    gfx::blurGaussian<1, 1>(canvas);

    XYMap rectMap = XYMap::constructRectangularGrid(W, H);
    gfx::CanvasMapped<CRGB> mapped(fl::span<CRGB>(mapped_px, N), rectMap);
    gfx::blurGaussian<1, 1>(mapped);

    for (int i = 0; i < N; ++i) {
        FL_CHECK_EQ(canvas_px[i].r, mapped_px[i].r);
        FL_CHECK_EQ(canvas_px[i].g, mapped_px[i].g);
        FL_CHECK_EQ(canvas_px[i].b, mapped_px[i].b);
    }
}

FL_TEST_CASE("CanvasMapped matches Canvas - blurGaussian R2") {
    const int W = 9, H = 9, N = W * H;
    CRGB canvas_px[N], mapped_px[N];
    fill_deterministic(canvas_px, N);
    FL_BUILTIN_MEMCPY(mapped_px, canvas_px, sizeof(canvas_px));

    gfx::Canvas<CRGB> canvas(fl::span<CRGB>(canvas_px, N), W, H);
    gfx::blurGaussian<2, 2>(canvas);

    XYMap rectMap = XYMap::constructRectangularGrid(W, H);
    gfx::CanvasMapped<CRGB> mapped(fl::span<CRGB>(mapped_px, N), rectMap);
    gfx::blurGaussian<2, 2>(mapped);

    for (int i = 0; i < N; ++i) {
        FL_CHECK_EQ(canvas_px[i].r, mapped_px[i].r);
        FL_CHECK_EQ(canvas_px[i].g, mapped_px[i].g);
        FL_CHECK_EQ(canvas_px[i].b, mapped_px[i].b);
    }
}

FL_TEST_CASE("CanvasMapped matches Canvas - blurGaussian R3") {
    const int W = 12, H = 12, N = W * H;
    CRGB canvas_px[N], mapped_px[N];
    fill_deterministic(canvas_px, N);
    FL_BUILTIN_MEMCPY(mapped_px, canvas_px, sizeof(canvas_px));

    gfx::Canvas<CRGB> canvas(fl::span<CRGB>(canvas_px, N), W, H);
    gfx::blurGaussian<3, 3>(canvas);

    XYMap rectMap = XYMap::constructRectangularGrid(W, H);
    gfx::CanvasMapped<CRGB> mapped(fl::span<CRGB>(mapped_px, N), rectMap);
    gfx::blurGaussian<3, 3>(mapped);

    for (int i = 0; i < N; ++i) {
        FL_CHECK_EQ(canvas_px[i].r, mapped_px[i].r);
        FL_CHECK_EQ(canvas_px[i].g, mapped_px[i].g);
        FL_CHECK_EQ(canvas_px[i].b, mapped_px[i].b);
    }
}

FL_TEST_CASE("CanvasMapped matches Canvas - blurGaussian R4") {
    const int W = 16, H = 16, N = W * H;
    CRGB canvas_px[N], mapped_px[N];
    fill_deterministic(canvas_px, N);
    FL_BUILTIN_MEMCPY(mapped_px, canvas_px, sizeof(canvas_px));

    gfx::Canvas<CRGB> canvas(fl::span<CRGB>(canvas_px, N), W, H);
    gfx::blurGaussian<4, 4>(canvas);

    XYMap rectMap = XYMap::constructRectangularGrid(W, H);
    gfx::CanvasMapped<CRGB> mapped(fl::span<CRGB>(mapped_px, N), rectMap);
    gfx::blurGaussian<4, 4>(mapped);

    for (int i = 0; i < N; ++i) {
        FL_CHECK_EQ(canvas_px[i].r, mapped_px[i].r);
        FL_CHECK_EQ(canvas_px[i].g, mapped_px[i].g);
        FL_CHECK_EQ(canvas_px[i].b, mapped_px[i].b);
    }
}

FL_TEST_CASE("CanvasMapped matches Canvas - horizontal only") {
    const int W = 8, H = 8, N = W * H;
    CRGB canvas_px[N], mapped_px[N];
    fill_deterministic(canvas_px, N);
    FL_BUILTIN_MEMCPY(mapped_px, canvas_px, sizeof(canvas_px));

    gfx::Canvas<CRGB> canvas(fl::span<CRGB>(canvas_px, N), W, H);
    gfx::blurGaussian<2, 0>(canvas);

    XYMap rectMap = XYMap::constructRectangularGrid(W, H);
    gfx::CanvasMapped<CRGB> mapped(fl::span<CRGB>(mapped_px, N), rectMap);
    gfx::blurGaussian<2, 0>(mapped);

    for (int i = 0; i < N; ++i) {
        FL_CHECK_EQ(canvas_px[i].r, mapped_px[i].r);
        FL_CHECK_EQ(canvas_px[i].g, mapped_px[i].g);
        FL_CHECK_EQ(canvas_px[i].b, mapped_px[i].b);
    }
}

FL_TEST_CASE("CanvasMapped matches Canvas - vertical only") {
    const int W = 8, H = 8, N = W * H;
    CRGB canvas_px[N], mapped_px[N];
    fill_deterministic(canvas_px, N);
    FL_BUILTIN_MEMCPY(mapped_px, canvas_px, sizeof(canvas_px));

    gfx::Canvas<CRGB> canvas(fl::span<CRGB>(canvas_px, N), W, H);
    gfx::blurGaussian<0, 2>(canvas);

    XYMap rectMap = XYMap::constructRectangularGrid(W, H);
    gfx::CanvasMapped<CRGB> mapped(fl::span<CRGB>(mapped_px, N), rectMap);
    gfx::blurGaussian<0, 2>(mapped);

    for (int i = 0; i < N; ++i) {
        FL_CHECK_EQ(canvas_px[i].r, mapped_px[i].r);
        FL_CHECK_EQ(canvas_px[i].g, mapped_px[i].g);
        FL_CHECK_EQ(canvas_px[i].b, mapped_px[i].b);
    }
}

FL_TEST_CASE("CanvasMapped matches Canvas - with alpha8 dim") {
    const int W = 8, H = 8, N = W * H;
    CRGB canvas_px[N], mapped_px[N];
    fill_deterministic(canvas_px, N);
    FL_BUILTIN_MEMCPY(mapped_px, canvas_px, sizeof(canvas_px));

    gfx::Canvas<CRGB> canvas(fl::span<CRGB>(canvas_px, N), W, H);
    gfx::blurGaussian<1, 1>(canvas, alpha8(128));

    XYMap rectMap = XYMap::constructRectangularGrid(W, H);
    gfx::CanvasMapped<CRGB> mapped(fl::span<CRGB>(mapped_px, N), rectMap);
    gfx::blurGaussian<1, 1>(mapped, alpha8(128));

    for (int i = 0; i < N; ++i) {
        FL_CHECK_EQ(canvas_px[i].r, mapped_px[i].r);
        FL_CHECK_EQ(canvas_px[i].g, mapped_px[i].g);
        FL_CHECK_EQ(canvas_px[i].b, mapped_px[i].b);
    }
}

FL_TEST_CASE("CanvasMapped matches Canvas - asymmetric radii") {
    const int W = 10, H = 10, N = W * H;
    CRGB canvas_px[N], mapped_px[N];
    fill_deterministic(canvas_px, N);
    FL_BUILTIN_MEMCPY(mapped_px, canvas_px, sizeof(canvas_px));

    gfx::Canvas<CRGB> canvas(fl::span<CRGB>(canvas_px, N), W, H);
    gfx::blurGaussian<1, 2>(canvas);

    XYMap rectMap = XYMap::constructRectangularGrid(W, H);
    gfx::CanvasMapped<CRGB> mapped(fl::span<CRGB>(mapped_px, N), rectMap);
    gfx::blurGaussian<1, 2>(mapped);

    for (int i = 0; i < N; ++i) {
        FL_CHECK_EQ(canvas_px[i].r, mapped_px[i].r);
        FL_CHECK_EQ(canvas_px[i].g, mapped_px[i].g);
        FL_CHECK_EQ(canvas_px[i].b, mapped_px[i].b);
    }
}

// ── CanvasMapped non-optimized path tests (non-rectangular XYMap) ────────

// Identity XYMap via user function — forces the non-optimized gather/scatter path.
static fl::u16 identity_xy(fl::u16 x, fl::u16 y, fl::u16 w, fl::u16 h) {
    (void)h;
    return static_cast<fl::u16>(y * w + x);
}

// Helper: check two pixel values are within ±tolerance per channel.
static bool near_eq(uint8_t a, uint8_t b, int tol = 1) {
    return (a >= b ? a - b : b - a) <= tol;
}

FL_TEST_CASE("CanvasMapped non-rect matches Canvas within 1 LSB - R1") {
    const int W = 8, H = 8, N = W * H;
    CRGB canvas_px[N], mapped_px[N];
    fill_deterministic(canvas_px, N);
    FL_BUILTIN_MEMCPY(mapped_px, canvas_px, sizeof(canvas_px));

    gfx::Canvas<CRGB> canvas(fl::span<CRGB>(canvas_px, N), W, H);
    gfx::blurGaussian<1, 1>(canvas);

    // Use identity user function — NOT rectangular, so non-optimized path runs.
    XYMap idMap = XYMap::constructWithUserFunction(W, H, identity_xy);
    gfx::CanvasMapped<CRGB> mapped(fl::span<CRGB>(mapped_px, N), idMap);
    gfx::blurGaussian<1, 1>(mapped);

    // SIMD avg_round introduces ±1 per pass; H+V can compound to ±1 per component.
    for (int i = 0; i < N; ++i) {
        FL_CHECK(near_eq(canvas_px[i].r, mapped_px[i].r));
        FL_CHECK(near_eq(canvas_px[i].g, mapped_px[i].g));
        FL_CHECK(near_eq(canvas_px[i].b, mapped_px[i].b));
    }
}

FL_TEST_CASE("CanvasMapped non-rect matches Canvas within tolerance - R2") {
    const int W = 9, H = 9, N = W * H;
    CRGB canvas_px[N], mapped_px[N];
    fill_deterministic(canvas_px, N);
    FL_BUILTIN_MEMCPY(mapped_px, canvas_px, sizeof(canvas_px));

    gfx::Canvas<CRGB> canvas(fl::span<CRGB>(canvas_px, N), W, H);
    gfx::blurGaussian<2, 2>(canvas);

    XYMap idMap = XYMap::constructWithUserFunction(W, H, identity_xy);
    gfx::CanvasMapped<CRGB> mapped(fl::span<CRGB>(mapped_px, N), idMap);
    gfx::blurGaussian<2, 2>(mapped);

    // R=2 now uses exact [1,4,6,4,1] SIMD kernel (no cascaded avg_round).
    for (int i = 0; i < N; ++i) {
        FL_CHECK(near_eq(canvas_px[i].r, mapped_px[i].r));
        FL_CHECK(near_eq(canvas_px[i].g, mapped_px[i].g));
        FL_CHECK(near_eq(canvas_px[i].b, mapped_px[i].b));
    }
}

FL_TEST_CASE("CanvasMapped non-rect matches Canvas within 1 LSB - R3") {
    const int W = 12, H = 12, N = W * H;
    CRGB canvas_px[N], mapped_px[N];
    fill_deterministic(canvas_px, N);
    FL_BUILTIN_MEMCPY(mapped_px, canvas_px, sizeof(canvas_px));

    gfx::Canvas<CRGB> canvas(fl::span<CRGB>(canvas_px, N), W, H);
    gfx::blurGaussian<3, 3>(canvas);

    XYMap idMap = XYMap::constructWithUserFunction(W, H, identity_xy);
    gfx::CanvasMapped<CRGB> mapped(fl::span<CRGB>(mapped_px, N), idMap);
    gfx::blurGaussian<3, 3>(mapped);

    // R=3 uses exact SIMD weights (no cascaded avg_round).
    for (int i = 0; i < N; ++i) {
        FL_CHECK(near_eq(canvas_px[i].r, mapped_px[i].r));
        FL_CHECK(near_eq(canvas_px[i].g, mapped_px[i].g));
        FL_CHECK(near_eq(canvas_px[i].b, mapped_px[i].b));
    }
}

FL_TEST_CASE("CanvasMapped non-rect matches Canvas within 1 LSB - R4") {
    const int W = 16, H = 16, N = W * H;
    CRGB canvas_px[N], mapped_px[N];
    fill_deterministic(canvas_px, N);
    FL_BUILTIN_MEMCPY(mapped_px, canvas_px, sizeof(canvas_px));

    gfx::Canvas<CRGB> canvas(fl::span<CRGB>(canvas_px, N), W, H);
    gfx::blurGaussian<4, 4>(canvas);

    XYMap idMap = XYMap::constructWithUserFunction(W, H, identity_xy);
    gfx::CanvasMapped<CRGB> mapped(fl::span<CRGB>(mapped_px, N), idMap);
    gfx::blurGaussian<4, 4>(mapped);

    // R=4 uses exact SIMD weights (no cascaded avg_round).
    for (int i = 0; i < N; ++i) {
        FL_CHECK(near_eq(canvas_px[i].r, mapped_px[i].r));
        FL_CHECK(near_eq(canvas_px[i].g, mapped_px[i].g));
        FL_CHECK(near_eq(canvas_px[i].b, mapped_px[i].b));
    }
}

FL_TEST_FILE(FL_FILEPATH) {

} // FL_TEST_FILE
