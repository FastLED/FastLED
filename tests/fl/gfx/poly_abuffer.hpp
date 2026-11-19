
#pragma once

#include "test.h"
#include "fl/gfx/gfx.h"
#include "fl/stl/chrono.h"
#include "fl/math/fixed_point/s16x16.h"
#include "tests/fl/gfx/poly_tinygl_ref.h"

// ============================================================================
// drawPolygon A-buffer backend tests
//
// polyABuffer (fl::gfx::PolyFill::ABuffer) is validated against a
// bit-exact integer oracle that re-derives the kernel's sampling model
// independently (quantized edge steps, half-open sub-rows, right-edge
// sub-column samples). The oracle is intentionally a *different* code
// path: per-sample floor divisions in i64 (the kernel uses an incremental
// step with no division in the scan), so agreement proves the
// decomposition is exact.
//
// The TinyGL reference port (poly_tinygl_ref.h) is exercised for
// structural parity and its documented limitations.
// ============================================================================

static const fl::u16 kPolyABIters = 1000;

FL_TEST_FILE(FL_FILEPATH) {

// ----------------------------------------------------------------------------
// Bit-exact oracle (test-only; mirrors the model documented in
// src/fl/gfx/poly_abuffer.h's file header)
// ----------------------------------------------------------------------------

namespace {

inline long long polyABTestFloorDiv64(long long a, long long b) {
    long long q = a / b;
    if ((a % b) != 0 && ((a < 0) != (b < 0))) --q;
    return q;
}

struct PolyABTestEdge {  // oriented top->bottom, Q16.16
    long long t1, t2;
    long long x1;
    long long B;
};

/// Coverage alpha (0..255, 8 per subpixel) for pixel (cx, cy):
/// sub-row s in [ceil(4*t1), ceil(4*t2)) — i.e. the sub-row's TOP grid
/// line lies in the edge's [t1, t2) span (the segment reaches the
/// sub-row's top), so a vertex shared by two edges (on the grid or
/// mid-sub-row) never makes a sub-row claimed twice. The crossing X is
/// the edge's line at the sub-row's top grid line:
/// X = x1 + floor((s*16384 - t1)*xinc/65536), xinc = floor(65536*B/dy);
/// a sample at sub-column c counts when its right edge (c+1)/8 is
/// strictly right of X.
int polyABTestOracleCov(const long long* px, const long long* py, int n,
                        int cx, int cy) {
    PolyABTestEdge e[16];
    int m = 0;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        long long t1 = py[j], t2 = py[i];
        long long x1 = px[j], x2 = px[i];
        if (t1 > t2) {
            long long t = t1; t1 = t2; t2 = t;
            long long u = x1; x1 = x2; x2 = u;
        }
        if (t1 == t2) continue;  // horizontal: no trapezoid
        e[m].t1 = t1; e[m].t2 = t2; e[m].x1 = x1; e[m].B = x2 - x1;
        ++m;
    }
    int count = 0;
    for (int r = 0; r < 4; ++r) {
        const int s = cy * 4 + r;
        for (int c = 0; c < 8; ++c) {
            int inside = 0;
            for (int k = 0; k < m; ++k) {
                const int st1 = (int)((e[k].t1 + 16383) >> 14);
                const int st2 = (int)((e[k].t2 + 16383) >> 14);
                if (s < st1 || s >= st2) continue;
                const long long xinc =
                    polyABTestFloorDiv64(65536LL * e[k].B, e[k].t2 - e[k].t1);
                const long long X =
                    e[k].x1
                    + polyABTestFloorDiv64(((long long)s * 16384LL - e[k].t1) * xinc,
                                           65536);
                if (8192LL * (c + 1) > (X - ((long long)cx << 16)))
                    inside ^= 1;
            }
            if (inside) ++count;
        }
    }
    return (count >= 32) ? 255 : count * 8;
}

struct PolyABTestScene {
    const char* name;
    long long x[16], y[16];
    int n, w, h;
    int maxdLimit;  // quality-vs-Exact gate: max per-pixel alpha difference
};

static const long long kPolyABQ = 65536LL;

const PolyABTestScene kPolyABScenes[] = {
    { "triangle", { 213011, 966656, 557056 }, { 163840, 737280, 835584 }, 3, 20, 20, 80 },
    { "bowtie",   { 163840, 1146880, 163840, 1146880 }, { 393216, 917504, 917504, 393216 }, 4, 20, 20, 80 },
    // 0.7px-tall band tilted 0.6px over 17.2px. Placed so each sloped edge
    // lies within a single canvas row: the exact backend anchors a
    // <1px-tall edge's crossing at its FIRST row's center and reuses that
    // (stale) anchor on any second row, shifting the integrated line by a
    // full row of slope (see PolyEdge in src/fl/gfx/primitives.h) — keeping
    // the edges single-row makes the exact side exact here, so this scene
    // gates ABuffer against true area. sliver_straddle (below) pins the
    // stale-anchor regime itself.
    { "sliver",   { 78643, 1205862, 1205862, 78643 }, { 678297, 717619, 763494, 724172 }, 4, 20, 20, 80 },
    { "diamond",  { 655360, 1196032, 655360, 114688 }, { 32768, 655360, 1277952, 655360 }, 4, 20, 20, 80 },
    { "offleft",  { -262144, 786432, -131072 }, { 131072, 917504, 1048576 }, 3, 20, 20, 80 },
    { "offcanv",  { -2621440, -1310720, -1966080 }, { 131072, 917504, 1048576 }, 3, 20, 20, 80 },
    { "horiz",    { 163840, 655360, 1146880 }, { 655360, 655360, 655360 }, 3, 20, 20, 80 },
    { "steep",    { 65536, 1179648, 131072 }, { 655360, 658360, 664360 }, 3, 20, 20, 80 },
    // Rotated bowties: every vertex sits mid-sub-row (and two edges share
    // such a vertex), which used to double-claim the vertex's sub-row and
    // fill a spurious even-odd tail past the far edge (regression scenes
    // for the sub-row occupancy / DDA-seed fix).
    { "bowtie_r1", { 183328, 1127392, 146752, 1163968 }, { 359566, 951154, 882574, 428146 }, 4, 20, 20, 80 },
    { "bowtie_r2", { 205115, 1105604, 132142, 1178577 }, { 327357, 983362, 846537, 464182 }, 4, 20, 20, 80 },
    // The sliver band with its sloped edges straddling row boundaries. On
    // the second row of each <1px-tall edge the exact backend integrates
    // the edge's line shifted by a full row of slope (here ~29px, off
    // canvas), so it under-renders the band by up to ~0.5px^2 per pixel;
    // the ABuffer's reference-faithful sub-row-top sampling is closer to
    // the geometric truth there. The gate bounds the ABuffer's divergence
    // from that (defective) exact output, not from truth — hence the wider
    // limit.
    { "sliver_straddle", { 78643, 1205862, 1205862, 78643 }, { 635699, 675021, 720896, 681574 }, 4, 20, 20, 120 },
};

void polyABTestRenderABuffer(CRGB* buf, const PolyABTestScene& s,
                             fl::DrawMode mode) {
    for (int i = 0; i < s.w * s.h; ++i) buf[i] = CRGB(0, 0, 0);
    fl::CanvasRGB canvas(fl::span<CRGB>(buf, (fl::size)s.w * s.h), s.w, s.h);
    fl::PolyPoint<fl::s16x16> pts[16];
    for (int i = 0; i < s.n; ++i)
        pts[i] = { fl::s16x16::from_raw((fl::i32)s.x[i]),
                   fl::s16x16::from_raw((fl::i32)s.y[i]) };
    canvas.drawPolygon<fl::s16x16, 16, 64, fl::gfx::PolyFill::ABuffer>(
        CRGB(200, 120, 60), fl::span<const fl::PolyPoint<fl::s16x16>>(pts, s.n),
        mode);
}

void polyABTestRenderExact(CRGB* buf, const PolyABTestScene& s,
                           fl::DrawMode mode) {
    for (int i = 0; i < s.w * s.h; ++i) buf[i] = CRGB(0, 0, 0);
    fl::CanvasRGB canvas(fl::span<CRGB>(buf, (fl::size)s.w * s.h), s.w, s.h);
    fl::PolyPoint<fl::s16x16> pts[16];
    for (int i = 0; i < s.n; ++i)
        pts[i] = { fl::s16x16::from_raw((fl::i32)s.x[i]),
                   fl::s16x16::from_raw((fl::i32)s.y[i]) };
    canvas.drawPolygon<fl::s16x16, 16, 64, fl::gfx::PolyFill::Exact>(
        CRGB(200, 120, 60), fl::span<const fl::PolyPoint<fl::s16x16>>(pts, s.n),
        mode);
}

void polyABTestRenderRef(CRGB* buf, const PolyABTestScene& s, fl::DrawMode mode) {
    for (int i = 0; i < s.w * s.h; ++i) buf[i] = CRGB(0, 0, 0);
    // The ref port takes the base gfx::Canvas (CanvasRGB hides it via
    // protected inheritance).
    fl::gfx::Canvas<CRGB> canvas(fl::span<CRGB>(buf, (fl::size)s.w * s.h), s.w, s.h);
    fl::PolyPoint<fl::s16x16> pts[16];
    for (int i = 0; i < s.n; ++i)
        pts[i] = { fl::s16x16::from_raw((fl::i32)s.x[i]),
                   fl::s16x16::from_raw((fl::i32)s.y[i]) };
    if (mode == fl::DrawMode::DRAW_MODE_OVERWRITE)
        fl::gfx::detail::drawPolygonTinyGLRef<CRGB, fl::s16x16, true, 16>(
            canvas, CRGB(200, 120, 60),
            fl::span<const fl::PolyPoint<fl::s16x16>>(pts, s.n));
    else
        fl::gfx::detail::drawPolygonTinyGLRef<CRGB, fl::s16x16, false, 16>(
            canvas, CRGB(200, 120, 60),
            fl::span<const fl::PolyPoint<fl::s16x16>>(pts, s.n));
}

}  // namespace

// ----------------------------------------------------------------------------
// Correctness
// ----------------------------------------------------------------------------

FL_TEST_CASE("drawPolygon ABuffer: bit-exact vs integer oracle") {
    for (int s = 0; s < (int)(sizeof(kPolyABScenes) / sizeof(kPolyABScenes[0])); ++s) {
        const PolyABTestScene& sc = kPolyABScenes[s];
        FL_SUBCASE(sc.name) {
            CRGB buffer[28 * 28];
            polyABTestRenderABuffer(buffer, sc, fl::DrawMode::DRAW_MODE_OVERWRITE);
            int mismatches = 0, firstMax = 0;
            int firstGot = 0, firstWant = 0;
            for (int y = 0; y < sc.h; ++y)
                for (int x = 0; x < sc.w; ++x) {
                    const int cov = polyABTestOracleCov(sc.x, sc.y, sc.n, x, y);
                    CRGB wantc(200, 120, 60);
                    if (cov < 255) wantc.nscale8((fl::u8)cov);
                    if (wantc.r != buffer[y * sc.w + x].r) {
                        if (mismatches == 0) { firstGot = buffer[y*sc.w+x].r; firstWant = wantc.r; }
                        ++mismatches;
                    }
                }
            fl::printf("  %s: %d mismatched pixels\n", sc.name, mismatches);
            FL_CHECK_EQ(mismatches, 0);
            (void)firstMax; (void)firstGot; (void)firstWant;
        }
    }
}

FL_TEST_CASE("drawPolygon ABuffer: even-odd and degenerate inputs") {
    FL_SUBCASE("fewer than 3 vertices paints nothing") {
        PolyABTestScene sc = kPolyABScenes[0];
        sc.n = 2;
        CRGB buffer[28 * 28];
        polyABTestRenderABuffer(buffer, sc, fl::DrawMode::DRAW_MODE_OVERWRITE);
        for (int i = 0; i < sc.w * sc.h; ++i)
            FL_CHECK_EQ((int)buffer[i].r + (int)buffer[i].g + (int)buffer[i].b, 0);
    }
    FL_SUBCASE("all-horizontal polygon paints nothing") {
        CRGB buffer[28 * 28];
        const PolyABTestScene& sc = kPolyABScenes[6];  // horiz
        polyABTestRenderABuffer(buffer, sc, fl::DrawMode::DRAW_MODE_OVERWRITE);
        for (int i = 0; i < sc.w * sc.h; ++i)
            FL_CHECK_EQ((int)buffer[i].r + (int)buffer[i].g + (int)buffer[i].b, 0);
    }
    FL_SUBCASE("fully off-canvas polygon paints nothing") {
        CRGB buffer[28 * 28];
        const PolyABTestScene& sc = kPolyABScenes[5];  // offcanv
        polyABTestRenderABuffer(buffer, sc, fl::DrawMode::DRAW_MODE_OVERWRITE);
        for (int i = 0; i < sc.w * sc.h; ++i)
            FL_CHECK_EQ((int)buffer[i].r + (int)buffer[i].g + (int)buffer[i].b, 0);
    }
    FL_SUBCASE("bowtie: petals filled, side notches empty (even-odd)") {
        CRGB buffer[28 * 28];
        const PolyABTestScene& sc = kPolyABScenes[1];  // bowtie
        polyABTestRenderABuffer(buffer, sc, fl::DrawMode::DRAW_MODE_OVERWRITE);
        // This bowtie is a vertical hourglass: the top petal (apex (2.5,6)
        // to (17.5,6), meeting at (10,10)) and bottom petal are filled; the
        // left/right notches beside the crossing are empty (even-odd).
        FL_CHECK_GE((int)buffer[7 * 20 + 10].r, 200);   // top petal
        FL_CHECK_GE((int)buffer[12 * 20 + 10].r, 200);  // bottom petal
        FL_CHECK_LT((int)buffer[10 * 20 + 6].r, 40);    // left notch
        FL_CHECK_LT((int)buffer[10 * 20 + 13].r, 40);   // right notch
    }
    FL_SUBCASE("fully-covered pixel is solid draw color") {
        CRGB buffer[28 * 28];
        // Big diamond with a solid interior; (10, 10) is fully covered.
        const PolyABTestScene& sc = kPolyABScenes[3];  // diamond
        polyABTestRenderABuffer(buffer, sc, fl::DrawMode::DRAW_MODE_OVERWRITE);
        FL_CHECK_EQ((int)buffer[10 * 20 + 10].r, 200);
    }
}

FL_TEST_CASE("drawPolygon ABuffer: blend is additive") {
    FL_SUBCASE("blend from zero equals overwrite") {
        CRGB a[28 * 28], b[28 * 28];
        const PolyABTestScene& sc = kPolyABScenes[0];  // triangle
        polyABTestRenderABuffer(a, sc, fl::DrawMode::DRAW_MODE_OVERWRITE);
        polyABTestRenderABuffer(b, sc, fl::DrawMode::DRAW_MODE_BLEND);
        for (int i = 0; i < sc.w * sc.h; ++i) {
            FL_CHECK_EQ((int)b[i].r, (int)a[i].r);
            FL_CHECK_EQ((int)b[i].g, (int)a[i].g);
            FL_CHECK_EQ((int)b[i].b, (int)a[i].b);
        }
    }
    FL_SUBCASE("two blend draws accumulate") {
        // Draw the triangle, then the same triangle shifted: every pixel
        // gets (scaled coverage of draw 1) + (scaled coverage of draw 2),
        // saturated at 255.
        PolyABTestScene s1 = kPolyABScenes[0];            // triangle
        PolyABTestScene s2 = kPolyABScenes[0];            // shifted right by 3
        for (int i = 0; i < s2.n; ++i) s2.x[i] += 3 * kPolyABQ;
        CRGB a[28 * 28], b[28 * 28];
        polyABTestRenderABuffer(a, s1, fl::DrawMode::DRAW_MODE_BLEND);
        {
            fl::CanvasRGB canvas(a, s1.w, s1.h);
            fl::PolyPoint<fl::s16x16> pts[16];
            for (int i = 0; i < s2.n; ++i)
                pts[i] = { fl::s16x16::from_raw((fl::i32)s2.x[i]),
                           fl::s16x16::from_raw((fl::i32)s2.y[i]) };
            canvas.drawPolygon<fl::s16x16, 16, 64, fl::gfx::PolyFill::ABuffer>(
                CRGB(200, 120, 60), fl::span<const fl::PolyPoint<fl::s16x16>>(pts, s2.n),
                fl::DrawMode::DRAW_MODE_BLEND);
        }
        // Expected: oracle(s1) + oracle(s2), each through nscale8(200, cov),
        // saturated at 255 (CRGB::operator+= is qadd8, saturating).
        for (int y = 0; y < s1.h; ++y)
            for (int x = 0; x < s1.w; ++x) {
                CRGB c1(200, 120, 60), c2(200, 120, 60);
                const int cov1 = polyABTestOracleCov(s1.x, s1.y, s1.n, x, y);
                const int cov2 = polyABTestOracleCov(s2.x, s2.y, s2.n, x, y);
                if (cov1 < 255) c1.nscale8((fl::u8)cov1);
                if (cov2 < 255) c2.nscale8((fl::u8)cov2);
                const int want = c1.r + c2.r;
                FL_CHECK_EQ((int)a[y * s1.w + x].r, (want > 255) ? 255 : (int)want);
            }
        (void)b;
    }
}

FL_TEST_CASE("drawPolygon ABuffer: quality vs Exact backend") {
    // 8x4 sampling vs exact area integration: per-pixel alpha may differ by
    // up to a handful of subpixels on thin/oblique edges. Gate: no pixel
    // further apart than ~10 subpixels of coverage (per-scene limit; see
    // sliver_straddle for the one scene where the exact side is the
    // outlier), mean well under one.
    int worstMax = 0;
    double worstMean = 0.0;
    for (int s = 0; s < (int)(sizeof(kPolyABScenes) / sizeof(kPolyABScenes[0])); ++s) {
        const PolyABTestScene& sc = kPolyABScenes[s];
        FL_SUBCASE(sc.name) {
            CRGB a[28 * 28], e[28 * 28];
            polyABTestRenderABuffer(a, sc, fl::DrawMode::DRAW_MODE_OVERWRITE);
            polyABTestRenderExact(e, sc, fl::DrawMode::DRAW_MODE_OVERWRITE);
            int pixd = 0, maxd = 0;
            long sum = 0;
            for (int i = 0; i < sc.w * sc.h; ++i) {
                const int d = a[i].r > e[i].r ? (int)a[i].r - (int)e[i].r
                                              : (int)e[i].r - (int)a[i].r;
                if (d > maxd) maxd = d;
                if (d) ++pixd;
                sum += d;
            }
            const double mean = (double)sum / (sc.w * sc.h);
            fl::printf("  %s: %d differing pixels, max %d, mean %.3f\n",
                       sc.name, pixd, maxd, mean);
            FL_CHECK_LE(maxd, sc.maxdLimit);
            FL_CHECK(mean < 5.0);
            if (maxd > worstMax) worstMax = maxd;
            if (mean > worstMean) worstMean = mean;
        }
    }
    (void)worstMax; (void)worstMean;
}

FL_TEST_CASE("drawPolygon ABuffer: TinyGL-ref reference parity") {
    // The ref port keeps the reference algorithm's structure; it must
    // render well inside its assumptions and expose its documented
    // limitations outside them.
    FL_SUBCASE("convex triangle: interior solid, exterior empty") {
        PolyABTestScene sc;
        sc.name = "bigtri";
        sc.n = 3; sc.w = 20; sc.h = 20;
        sc.x[0] = 4 * kPolyABQ;  sc.y[0] = 3 * kPolyABQ;
        sc.x[1] = 16 * kPolyABQ; sc.y[1] = 5 * kPolyABQ;
        sc.x[2] = 8 * kPolyABQ;  sc.y[2] = 16 * kPolyABQ;
        CRGB a[28 * 28], r[28 * 28];
        polyABTestRenderABuffer(a, sc, fl::DrawMode::DRAW_MODE_OVERWRITE);
        polyABTestRenderRef(r, sc, fl::DrawMode::DRAW_MODE_OVERWRITE);
        // Deep interior solid for both (color (200,120,60) -> r == 200);
        // deep exterior empty for both.
        FL_CHECK_EQ((int)a[10 * 20 + 8].r, 200);
        FL_CHECK_EQ((int)r[10 * 20 + 8].r, 200);
        FL_CHECK_EQ((int)a[18 * 20 + 17].r, 0);
        FL_CHECK_EQ((int)r[18 * 20 + 17].r, 0);
        // Energies agree within 10% (edge sampling differs: floor vs
        // truncating division, 32-level vs per-sample quantization).
        int ea = 0, er = 0;
        for (int i = 0; i < 20 * 20; ++i) { ea += (int)a[i].r; er += (int)r[i].r; }
        const int d = ea > er ? ea - er : er - ea;
        fl::printf("  bigtri energy: abuffer %d, ref %d (diff %d)\n", ea, er, d);
        FL_CHECK(d < ea / 10);
    }
    FL_SUBCASE("bowtie: ref's static edge order mis-renders the far petal") {
        CRGB a[28 * 28], r[28 * 28];
        const PolyABTestScene& sc = kPolyABScenes[1];  // bowtie
        polyABTestRenderABuffer(a, sc, fl::DrawMode::DRAW_MODE_OVERWRITE);
        polyABTestRenderRef(r, sc, fl::DrawMode::DRAW_MODE_OVERWRITE);
        int diff = 0;
        for (int i = 0; i < 20 * 20; ++i)
            if ((int)a[i].r != (int)r[i].r) ++diff;
        fl::printf("  bowtie ref-vs-abuffer: %d differing pixels\n", diff);
        // The reference's monotonic edge walk loses the far petal once the
        // edges' x-order flips at the crossing — the backends must differ.
        FL_CHECK_GT(diff, 0);
    }
    FL_SUBCASE("steep sub-pixel edge: ref stays finite (wraps, doesn't crash)") {
        CRGB a[28 * 28], r[28 * 28];
        const PolyABTestScene& sc = kPolyABScenes[7];  // steep
        polyABTestRenderABuffer(a, sc, fl::DrawMode::DRAW_MODE_OVERWRITE);
        polyABTestRenderRef(r, sc, fl::DrawMode::DRAW_MODE_OVERWRITE);
        int diff = 0, maxd = 0;
        for (int i = 0; i < 20 * 20; ++i) {
            const int d = a[i].r > r[i].r ? (int)a[i].r - (int)r[i].r
                                          : (int)r[i].r - (int)a[i].r;
            if (d) ++diff;
            if (d > maxd) maxd = d;
        }
        fl::printf("  steep ref-vs-abuffer: %d differing pixels, max %d\n", diff, maxd);
        FL_CHECK_LE(maxd, 255);
    }
}

FL_TEST_CASE("drawPolygon ABuffer: 32-bit coverage popcounts count the high word") {
    // Per-pixel coverage masks with high-word bits. On AVR a 16-bit
    // `unsigned int` popcount dropped them (22 -> 16, 24 -> 16); host ints
    // are 32-bit, so this pins the values rather than reproducing that.
    const fl::u32 masks[] = {0x011fffffu, 0x033fffffu, 0x80f8ffffu,
                             0xffff0000u, 0xffffffffu, 0u};
    const int expected[] = {22, 24, 22, 16, 32, 0};
    for (int i = 0; i < 6; ++i) {
        FL_CHECK_EQ((int)fl::gfx::detail::polyTGPopcount32(masks[i]), expected[i]);
        FL_CHECK_EQ((int)fl::gfx::detail::polyABPopcount32(masks[i]), expected[i]);
    }
}

// ----------------------------------------------------------------------------
// Performance (host; the AVR cycle counts come from the cycleprof runs)
// ----------------------------------------------------------------------------

FL_TEST_CASE("Graphics performance - drawPolygon kernels") {
    // Render each scene once per iteration with each backend.
    const PolyABTestScene* perfScenes[4] = {
        &kPolyABScenes[1],  // bowtie
        &kPolyABScenes[2],  // sliver
        &kPolyABScenes[3],  // diamond
        &kPolyABScenes[0],  // triangle
    };
    CRGB buffer[28 * 28];

    auto benchKernel = [&](const char* name, int kernel) {
        auto start = fl::chrono::steady_clock::now();
        for (fl::u16 i = 0; i < kPolyABIters; ++i) {
            for (int s = 0; s < 4; ++s) {
                const PolyABTestScene& sc = *perfScenes[s];
                for (int j = 0; j < sc.w * sc.h; ++j) buffer[j] = CRGB(0, 0, 0);
                fl::gfx::Canvas<CRGB> canvas(fl::span<CRGB>(buffer, (fl::size)sc.w * sc.h),
                                              sc.w, sc.h);
                fl::PolyPoint<fl::s16x16> pts[16];
                for (int j = 0; j < sc.n; ++j)
                    pts[j] = { fl::s16x16::from_raw((fl::i32)sc.x[j]),
                               fl::s16x16::from_raw((fl::i32)sc.y[j]) };
                if (kernel == 0)
                    canvas.drawPolygon<fl::s16x16, 16, 64, fl::gfx::PolyFill::Exact>(
                        CRGB(200, 120, 60),
                        fl::span<const fl::PolyPoint<fl::s16x16>>(pts, sc.n),
                        fl::DrawMode::DRAW_MODE_OVERWRITE);
                else if (kernel == 1)
                    canvas.drawPolygon<fl::s16x16, 16, 64, fl::gfx::PolyFill::ABuffer>(
                        CRGB(200, 120, 60),
                        fl::span<const fl::PolyPoint<fl::s16x16>>(pts, sc.n),
                        fl::DrawMode::DRAW_MODE_OVERWRITE);
                else
                    fl::gfx::detail::drawPolygonTinyGLRef<CRGB, fl::s16x16, true, 16>(
                        canvas, CRGB(200, 120, 60),
                        fl::span<const fl::PolyPoint<fl::s16x16>>(pts, sc.n));
            }
        }
        auto end = fl::chrono::steady_clock::now();
        fl::u32 us = (fl::u32)fl::chrono::duration_cast<fl::chrono::microseconds>(end - start).count();
        const fl::u32 calls = (fl::u32)kPolyABIters * 4;
        fl::printf("  drawPolygon %s: %u us total, %u ns/call\n", name, (unsigned)us,
                   (unsigned)((us * 1000) / calls));
        return us;
    };

    FL_SUBCASE("Exact") {
        fl::u32 us = benchKernel("Exact", 0);
        FL_CHECK(us < 2000000);  // generous host gate; see printed ns/call
    }
    FL_SUBCASE("ABuffer") {
        fl::u32 us = benchKernel("ABuffer", 1);
        FL_CHECK(us < 2000000);
    }
    FL_SUBCASE("TinyGL-ref") {
        fl::u32 us = benchKernel("TinyGL-ref", 2);
        FL_CHECK(us < 2000000);
    }
}

}  // FL_TEST_FILE
