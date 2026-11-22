
#pragma once

#include "test.h"
#include "fl/gfx/gfx.h"
#include "fl/stl/chrono.h"
#include "fl/math/fixed_point/s16x16.h"
#include "fl/stl/bitset.h"
#include "fl/stl/scratch.h"
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
//
// The scene regression tests at the end of the file cross-check all three
// kernels on the scenes of the former PolyKernels/PolyShow/PolyDemo
// sketches and the Polygon example, and pin their output.
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

FL_TEST_CASE("drawPolygon ABuffer: AVR byte-popcount table is exact") {
    // polyABPop8 reads this table on AVR only (the host uses the builtin),
    // so check every entry here. A hand-typed copy had bytes 0x58..0x5F and
    // 0x98..0x9F one low, which dropped a subpixel on AVR (Polygon's knot
    // scene rendered differently than on the host).
    const fl::u8* table = fl::gfx::detail::polyABPop8Table();
    for (int v = 0; v < 256; ++v)
        FL_CHECK_EQ((int)FL_PGM_READ_BYTE_NEAR(&table[v]), (int)fl::popcount((fl::u8)v));
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
        // Fastest of 10 batches, scaled to the full count: one preemption on a
        // loaded host spoils only its batch instead of the whole total.
        const fl::u16 kBatches = 10;
        fl::u32 best = 0xFFFFFFFFu;
        for (fl::u16 b = 0; b < kBatches; ++b) {
            auto start = fl::chrono::steady_clock::now();
            for (fl::u16 i = 0; i < kPolyABIters / kBatches; ++i) {
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
            const fl::u32 batch_us = (fl::u32)fl::chrono::duration_cast<fl::chrono::microseconds>(end - start).count();
            best = batch_us < best ? batch_us : best;
        }
        const fl::u32 us = best * kBatches;
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

// ============================================================================
// Scene regression tests
//
// These replace the PolyKernels, PolyShow and PolyDemo example sketches
// (examples/Polygon/Polygon.ino is the single remaining drawPolygon
// example):
//
//  - "kernel scenes" are the animated PolyShow/PolyKernels scenes (bowtie,
//    subpixel, clamp, sliver, degenerate, diamond) sampled at fixed
//    animation times, with the sketches' exact integer transforms and tier
//    (MAXN = 4, CHUNK = 1). Each frame is rendered by Exact, ABuffer and
//    the TinyGL reference and cross-checked (ABuffer bit-exact vs the
//    oracle, Exact vs ABuffer coverage, TinyGL-ref vs ABuffer energy where
//    the reference is valid), then hash-pinned per kernel.
//  - "showcase scenes" are the multi-polygon scenes of the Polygon example
//    (pinwheel, layers, knot) plus PolyDemo's blob, rendered with both
//    product backends at a few times on 16x16 and 16x8 canvases,
//    cross-checked and hash-pinned. The scene functions are copies of the
//    sketch's (a sketch cannot be compiled into a unit test): when a scene
//    in Polygon.ino changes, change its copy here and re-pin.
//
// All geometry is integer (s16x16), so the pinned hashes are platform
// independent. A changed hash means the rendered pixels changed: if that is
// intended, update the table from the "got" values the failing test prints.
// ============================================================================

namespace {

typedef fl::s16x16 PolyScFix;
typedef fl::PolyPoint<PolyScFix> PolyScPt;

enum PolyScKernel { kPolyScExact = 0, kPolyScABuffer = 1, kPolyScRef = 2 };

// FNV-1a over the frame's bytes.
fl::u32 polyScHash(const CRGB* buf, int n, fl::u32 h = 2166136261u) {
    for (int i = 0; i < n; ++i) {
        const fl::u8 b[3] = {buf[i].r, buf[i].g, buf[i].b};
        for (int k = 0; k < 3; ++k) h = (h ^ b[k]) * 16777619u;
    }
    return h;
}

template <int MAXN, int CHUNK>
void polyScDraw(fl::gfx::Canvas<CRGB>& c, int kernel, const CRGB& color,
                const PolyScPt* pts, int n, fl::DrawMode mode) {
    const fl::span<const PolyScPt> sp(pts, (fl::size)n);
    if (kernel == kPolyScExact)
        c.drawPolygon<PolyScFix, MAXN, CHUNK, fl::gfx::PolyFill::Exact>(color, sp, mode);
    else if (kernel == kPolyScABuffer)
        c.drawPolygon<PolyScFix, MAXN, CHUNK, fl::gfx::PolyFill::ABuffer>(color, sp, mode);
    else if (mode == fl::DrawMode::DRAW_MODE_OVERWRITE)
        fl::gfx::detail::drawPolygonTinyGLRef<CRGB, PolyScFix, true, MAXN>(c, color, sp);
    else
        fl::gfx::detail::drawPolygonTinyGLRef<CRGB, PolyScFix, false, MAXN>(c, color, sp);
}

// ---- kernel scenes (PolyShow / PolyKernels, 20x20, one polygon) -------------

const int kPolyScW = 20;
const fl::i32 kPolyScC = 10 * 65536L;  // canvas center (both axes)

struct PolyScKernelScene {
    const char* name;
    fl::i32 x[4], y[4];
    int n;
    CRGB color;
};

// Vertex tables copied verbatim from the sketches (Q16.16 raw).
const PolyScKernelScene kPolyScKernelScenes[] = {
    {"bowtie", {163840, 1146880, 163840, 1146880}, {393216, 917504, 917504, 393216}, 4, CRGB(255, 0, 0)},
    {"subpixel", {216269, 642253, 1074790, 0}, {1094451, 209715, 845414, 0}, 3, CRGB(0, 128, 0)},
    {"clamp", {-4915200, 4915200, 0, 0}, {-4915200, 0, 4915200, 0}, 3, CRGB(0, 0, 255)},
    {"sliver", {78643, 1205862, 1205862, 78643}, {635699, 675021, 720896, 681574}, 4, CRGB(0, 255, 255)},
    {"degenerate", {163840, 1146880, 0, 0}, {163840, 819200, 0, 0}, 2, CRGB(255, 255, 255)},
    {"diamond", {655360, 1196032, 655360, 114688}, {32768, 655360, 1277952, 655360}, 4, CRGB(255, 0, 255)},
};
const int kPolyScNumKernelScenes = (int)(sizeof(kPolyScKernelScenes) / sizeof(kPolyScKernelScenes[0]));
const int kPolyScBowtie = 0;      // indices into kPolyScKernelScenes
const int kPolyScClamp = 2;
const int kPolyScDegenerate = 4;
const int kPolyScDiamond = 5;
const fl::u32 kPolyScTimes[] = {0, 1000, 2000, 3000, 3980};  // ms into the scene
const int kPolyScNumTimes = (int)(sizeof(kPolyScTimes) / sizeof(kPolyScTimes[0]));

void polyScRotate(PolyScPt* p, int n, fl::i32 cosD, fl::i32 sinD) {
    for (int i = 0; i < n; ++i) {
        const fl::i32 dx = p[i].x.raw() - kPolyScC;
        const fl::i32 dy = p[i].y.raw() - kPolyScC;
        const fl::i64 rx = (fl::i64)dx * cosD - (fl::i64)dy * sinD;
        const fl::i64 ry = (fl::i64)dx * sinD + (fl::i64)dy * cosD;
        p[i].x = PolyScFix::from_raw(kPolyScC + (fl::i32)(rx >> 16));
        p[i].y = PolyScFix::from_raw(kPolyScC + (fl::i32)(ry >> 16));
    }
}

/// Vertices of kernel scene `s` at time t (ms), animated exactly as the
/// sketches did: spin in fixed 20 ms steps, slide (clamp), breathe
/// (diamond). Returns the vertex count.
int polyScKernelFrame(int s, fl::u32 t, PolyScPt* p) {
    const PolyScKernelScene& sc = kPolyScKernelScenes[s];
    for (int i = 0; i < sc.n; ++i)
        p[i] = {PolyScFix::from_raw(sc.x[i]), PolyScFix::from_raw(sc.y[i])};
    // Per-20-ms rotation step (cos, sin in Q16.16) of the spinning scenes.
    static const fl::i32 kSteps[5][2] = {
        {65376, 4572}, {65496, 2287}, {0, 0}, {64898, 9121}, {65287, 5712}};
    if (s == kPolyScClamp) {  // clamp: centroid slides x = -80 .. +100 px over 4 s
        const fl::i32 cx = (-80 + 180 * (fl::i32)t / 4000) * 65536;
        for (int i = 0; i < sc.n; ++i) {
            p[i].x = PolyScFix::from_raw(p[i].x.raw() + cx);
            p[i].y = PolyScFix::from_raw(p[i].y.raw() + kPolyScC);
        }
    } else if (s == kPolyScDiamond) {  // diamond: scale 0.45 .. 1.0, period 2 s
        const fl::i32 w = (fl::i32)(t % 2000);
        const fl::i32 f = w < 1000 ? w : 2000 - w;
        const fl::i32 scale16 = 29491 + 36045 * f / 1000;
        for (int i = 0; i < sc.n; ++i) {
            const fl::i32 dx = p[i].x.raw() - kPolyScC;
            const fl::i32 dy = p[i].y.raw() - kPolyScC;
            p[i].x = PolyScFix::from_raw(kPolyScC + (fl::i32)(((fl::i64)dx * scale16) >> 16));
            p[i].y = PolyScFix::from_raw(kPolyScC + (fl::i32)(((fl::i64)dy * scale16) >> 16));
        }
    } else {
        for (fl::u32 acc = 0; acc < t; acc += 20)
            polyScRotate(p, sc.n, kSteps[s][0], kSteps[s][1]);
    }
    return sc.n;
}

/// True when the frame has a sloped edge less than 1 px tall. Both
/// reference comparisons below are ill-conditioned there: the Exact backend
/// reuses a stale crossing anchor on the second row such an edge straddles
/// (see sliver_straddle above and PolyEdge in src/fl/gfx/primitives.h), and
/// the TinyGL reference quantizes the edge to whole .2 sub-scanlines.
bool polyScHasThinEdge(const PolyScPt* p, int n) {
    for (int i = 0, j = n - 1; i < n; j = i++) {
        fl::i32 dy = p[i].y.raw() - p[j].y.raw();
        if (dy < 0) dy = -dy;
        if (dy > 0 && dy < 65536) return true;
    }
    return false;
}

/// Largest per-channel difference, normalized to a full-scale (255)
/// channel of `color` so differently colored scenes share one limit.
int polyScMaxAlphaDiff(const CRGB* a, const CRGB* b, int n, const CRGB& color) {
    const int full = color.r > color.g ? (color.r > color.b ? color.r : color.b)
                                       : (color.g > color.b ? color.g : color.b);
    int maxd = 0;
    for (int i = 0; i < n; ++i)
        for (int ch = 0; ch < 3; ++ch) {
            const int d = (int)a[i].raw[ch] - (int)b[i].raw[ch];
            const int ad = d < 0 ? -d : d;
            if (ad > maxd) maxd = ad;
        }
    return full ? maxd * 255 / full : 0;
}

const CRGB kPolyScDegenerateBase(24, 12, 4);

/// Render kernel scene `s` at time t with `kernel` into buf (20x20).
template <int CHUNK = 1>
void polyScRenderKernelScene(CRGB* buf, int s, fl::u32 t, int kernel) {
    const bool degenerate = (s == kPolyScDegenerate);
    for (int i = 0; i < kPolyScW * kPolyScW; ++i)
        buf[i] = degenerate ? kPolyScDegenerateBase : CRGB(0, 0, 0);
    fl::gfx::Canvas<CRGB> c(fl::span<CRGB>(buf, kPolyScW * kPolyScW), kPolyScW, kPolyScW);
    PolyScPt p[4];
    const int n = polyScKernelFrame(s, t, p);
    const fl::DrawMode ow = fl::DrawMode::DRAW_MODE_OVERWRITE;
    polyScDraw<4, CHUNK>(c, kernel, kPolyScKernelScenes[s].color, p, n, ow);
    if (degenerate) {  // plus a horizontal-only polygon: also a no-op
        const PolyScPt h[3] = {
            {PolyScFix::from_raw(163840), PolyScFix::from_raw(655360)},
            {PolyScFix::from_raw(655360), PolyScFix::from_raw(655360)},
            {PolyScFix::from_raw(1146880), PolyScFix::from_raw(655360)}};
        polyScDraw<4, CHUNK>(c, kernel, kPolyScKernelScenes[s].color, h, 3, ow);
    }
}

// ---- showcase scenes (examples/Polygon + PolyDemo's blob) -------------------

const fl::i32 kPolyScTwoPi = 411775L;
const fl::i32 kPolyScPi = 205887L;

fl::i32 polyScMul(fl::i32 a, fl::i32 b) { return (fl::i32)(((fl::i64)a * b) >> 16); }

fl::i32 polyScAngle(fl::u32 t, fl::u32 periodMs) {
    return (fl::i32)(((fl::i64)(t % periodMs) * kPolyScTwoPi) / (fl::i64)periodMs);
}

PolyScFix polyScWrap(fl::i32 a) {
    a %= kPolyScTwoPi;
    if (a < 0) a += kPolyScTwoPi;
    return PolyScFix::from_raw(a);
}

PolyScPt polyScPolar(fl::i32 cx, fl::i32 cy, fl::i32 r, fl::i32 a) {
    PolyScFix s, c;
    PolyScFix::sincos(polyScWrap(a), s, c);
    PolyScPt p = {PolyScFix::from_raw(cx + polyScMul(c.raw(), r)),
                  PolyScFix::from_raw(cy + polyScMul(s.raw(), r))};
    return p;
}

struct PolyScShow {
    fl::gfx::Canvas<CRGB>* canvas;
    int kernel;
    fl::i32 cx, cy, r;  // canvas center and inscribed radius, Q16.16
    template <int MAXN>
    void draw(const CRGB& color, const PolyScPt* p, int n, fl::DrawMode mode) {
        polyScDraw<MAXN, 64>(*canvas, kernel, color, p, n, mode);
    }
};

// Same geometry as scenePinwheel() in examples/Polygon/Polygon.ino.
void polyScPinwheel(PolyScShow& s, fl::u32 t) {
    const fl::i32 spin = polyScAngle(t, 7000);
    const fl::i32 len = polyScMul(s.r, 62259L);
    const fl::i32 back = polyScMul(len, 36045L);
    const fl::u8 hue = (fl::u8)(t / 40);
    for (int k = 0; k < 6; ++k) {
        const fl::i32 a = spin + k * (kPolyScTwoPi / 6);
        const PolyScPt blade[3] = {{PolyScFix::from_raw(s.cx), PolyScFix::from_raw(s.cy)},
                                   polyScPolar(s.cx, s.cy, len, a),
                                   polyScPolar(s.cx, s.cy, back, a + 58982L)};
        s.draw<3>(CHSV((fl::u8)(hue + 42 * k), 200, 240), blade, 3,
                  fl::DrawMode::DRAW_MODE_BLEND);
    }
}

// Same geometry as sceneLayers() in examples/Polygon/Polygon.ino.
void polyScLayers(PolyScShow& s, fl::u32 t) {
    static const CRGB kColors[3] = {CRGB(255, 0, 80), CRGB(255, 200, 0), CRGB(0, 160, 255)};
    for (int layer = 0; layer < 3; ++layer) {
        const fl::i32 r = s.r - layer * (s.r / 4);
        fl::i32 a = polyScAngle(t, 9000u - 2000u * (fl::u32)layer);
        if (layer & 1) a = -a;
        PolyScPt square[4];
        for (int j = 0; j < 4; ++j)
            square[j] = polyScPolar(s.cx, s.cy, r, a + j * (kPolyScTwoPi / 4));
        s.draw<4>(kColors[layer], square, 4, fl::DrawMode::DRAW_MODE_OVERWRITE);
    }
}

// Same geometry as sceneKnot() in examples/Polygon/Polygon.ino; MAXN is a
// template parameter so the MAXN test can render it under-capped.
const int kPolyScKnotN = 14;

template <int MAXN>
void polyScKnotMaxN(PolyScShow& s, fl::u32 t) {
    const fl::i32 ph1 = polyScAngle(t, 11000);
    const fl::i32 ph2 = polyScAngle(t, 8700);
    const fl::i32 rx = polyScMul(s.cx, 60293L);
    const fl::i32 ry = polyScMul(s.cy, 60293L);
    for (int pass = 0; pass < 2; ++pass) {
        PolyScPt knot[kPolyScKnotN];
        for (int i = 0; i < kPolyScKnotN; ++i) {
            const fl::i32 th = (fl::i32)((fl::i64)i * kPolyScTwoPi / kPolyScKnotN);
            const fl::i32 sx = PolyScFix::sin(polyScWrap(2 * th + ph1 + pass * kPolyScPi)).raw();
            const fl::i32 sy = PolyScFix::sin(polyScWrap(3 * th + ph2)).raw();
            knot[i].x = PolyScFix::from_raw(s.cx + polyScMul(rx, sx));
            knot[i].y = PolyScFix::from_raw(s.cy + polyScMul(ry, sy));
        }
        s.draw<MAXN>(CHSV((fl::u8)(160 + 90 * pass), 190, 255), knot, kPolyScKnotN,
                     fl::DrawMode::DRAW_MODE_BLEND);
    }
}

void polyScKnot(PolyScShow& s, fl::u32 t) { polyScKnotMaxN<kPolyScKnotN>(s, t); }

// PolyDemo's blob: a 12-vertex concave polygon (radius = sum of two rotating
// 3x/5x harmonics) and a counter-rotating 9-vertex inner blob, additive.
void polyScBlobPts(PolyScPt* p, int n, fl::i32 cx, fl::i32 cy, fl::i32 r0, fl::i32 a1,
                   fl::i32 phi1, fl::i32 a2, fl::i32 phi2) {
    for (int i = 0; i < n; ++i) {
        const fl::i32 th = (fl::i32)((fl::i64)i * kPolyScTwoPi / n);
        const fl::i32 s1 = PolyScFix::sin(polyScWrap(th * 3 + phi1)).raw();
        const fl::i32 s2 = PolyScFix::sin(polyScWrap(th * 5 + phi2)).raw();
        p[i] = polyScPolar(cx, cy, r0 + polyScMul(a1, s1) + polyScMul(a2, s2), th);
    }
}

void polyScBlob(PolyScShow& s, fl::u32 t) {
    const fl::i32 p1 = polyScAngle(t, 3200), p2 = polyScAngle(t, 2100) + 45875L;
    const fl::u8 hue = (fl::u8)(t / 30);
    PolyScPt p[12];
    // r0 / a1 / a2 as fractions of the inscribed radius (PolyDemo used
    // 6.5 / 2 / 1.1 px and 3.5 / 1.3 / 0.7 px of a 10 px radius).
    polyScBlobPts(p, 12, s.cx, s.cy, polyScMul(s.r, 42598L), polyScMul(s.r, 13107L), p1,
                  polyScMul(s.r, 7209L), p2);
    s.draw<12>(CHSV(hue, 215, 255), p, 12, fl::DrawMode::DRAW_MODE_BLEND);
    polyScBlobPts(p, 9, s.cx, s.cy, polyScMul(s.r, 22938L), polyScMul(s.r, 8520L),
                  kPolyScPi - p1, polyScMul(s.r, 4588L), p2 + kPolyScPi);
    s.draw<12>(CHSV((fl::u8)(hue + 140), 200, 240), p, 9, fl::DrawMode::DRAW_MODE_BLEND);
}

typedef void (*PolyScShowFn)(PolyScShow&, fl::u32);
struct PolyScShowScene {
    const char* name;
    PolyScShowFn fn;
    int energyTolPct;  // Exact-vs-ABuffer total energy gate
};
// Small shapes are mostly edge pixels, where the two coverage models
// differ most (10%). OVERWRITE layers are looser: each edge pixel keeps only
// the top shape's partial coverage, so the models' edge differences do not
// average out against the layer below.
const PolyScShowScene kPolyScShowScenes[] = {
    {"pinwheel", polyScPinwheel, 10},
    {"layers", polyScLayers, 30},
    {"knot", polyScKnot, 10},
    {"blob", polyScBlob, 10},
};
const int kPolyScNumShowScenes = (int)(sizeof(kPolyScShowScenes) / sizeof(kPolyScShowScenes[0]));
const fl::u32 kPolyScShowTimes[] = {0, 1234, 2500, 4321};
const int kPolyScNumShowTimes = (int)(sizeof(kPolyScShowTimes) / sizeof(kPolyScShowTimes[0]));
// Polygon.ino's default 16x16 canvas, plus a non-square 16x8 one (its
// -DWIDTH/-DHEIGHT overrides; the knot scales x and y separately).
const int kPolyScShowSizes[2][2] = {{16, 16}, {16, 8}};

void polyScRenderShow(CRGB* buf, int w, int h, PolyScShowFn fn, fl::u32 t, int kernel) {
    for (int i = 0; i < w * h; ++i) buf[i] = CRGB(0, 0, 0);
    fl::gfx::Canvas<CRGB> c(fl::span<CRGB>(buf, (fl::size)(w * h)), w, h);
    PolyScShow s = {&c, kernel, w * 65536L / 2, h * 65536L / 2, (w < h ? w : h) * 65536L / 2};
    fn(s, t);
}

long polyScEnergy(const CRGB* buf, int n) {
    long e = 0;
    for (int i = 0; i < n; ++i) e += buf[i].r + buf[i].g + buf[i].b;
    return e;
}

}  // namespace

// ---- kernel cross-checks (formerly PolyKernels' POLY_KERNEL=0/1/2) ----------

FL_TEST_CASE("drawPolygon scenes: ABuffer is bit-exact vs the oracle on animated frames") {
    for (int s = 0; s < kPolyScNumKernelScenes; ++s) {
        if (s == kPolyScDegenerate) continue;  // covered by the no-op test
        for (int ti = 0; ti < kPolyScNumTimes; ++ti) {
            CRGB buf[20 * 20];
            polyScRenderKernelScene(buf, s, kPolyScTimes[ti], kPolyScABuffer);
            PolyScPt p[4];
            const int n = polyScKernelFrame(s, kPolyScTimes[ti], p);
            long long px[16], py[16];
            for (int i = 0; i < n; ++i) { px[i] = p[i].x.raw(); py[i] = p[i].y.raw(); }
            int mismatches = 0;
            for (int y = 0; y < kPolyScW; ++y)
                for (int x = 0; x < kPolyScW; ++x) {
                    CRGB want = kPolyScKernelScenes[s].color;
                    const int cov = polyABTestOracleCov(px, py, n, x, y);
                    if (cov < 255) want.nscale8((fl::u8)cov);
                    if (!(want == buf[y * kPolyScW + x])) ++mismatches;
                }
            if (mismatches)
                fl::printf("  %s t=%u: %d pixels differ from the oracle\n",
                           kPolyScKernelScenes[s].name, (unsigned)kPolyScTimes[ti], mismatches);
            FL_CHECK_EQ(mismatches, 0);
        }
    }
}

FL_TEST_CASE("drawPolygon scenes: Exact and ABuffer agree on animated frames") {
    // Same polygon, two coverage models (exact area vs 8x4 samples): the
    // worst pixel within ~a third of full scale and the total energy within
    // 3%. Frames with a sub-pixel-tall sloped edge only get a loose energy
    // gate (see polyScHasThinEdge).
    for (int s = 0; s < kPolyScNumKernelScenes; ++s) {
        if (s == kPolyScDegenerate) continue;
        for (int ti = 0; ti < kPolyScNumTimes; ++ti) {
            CRGB e[20 * 20], a[20 * 20];
            polyScRenderKernelScene(e, s, kPolyScTimes[ti], kPolyScExact);
            polyScRenderKernelScene(a, s, kPolyScTimes[ti], kPolyScABuffer);
            PolyScPt p[4];
            const bool thin = polyScHasThinEdge(p, polyScKernelFrame(s, kPolyScTimes[ti], p));
            const int maxd = polyScMaxAlphaDiff(e, a, 400, kPolyScKernelScenes[s].color);
            const long ee = polyScEnergy(e, 400), ea = polyScEnergy(a, 400);
            const long de = ee > ea ? ee - ea : ea - ee;
            fl::printf("  %s t=%u%s: max alpha diff %d, energy exact %ld abuffer %ld\n",
                       kPolyScKernelScenes[s].name, (unsigned)kPolyScTimes[ti],
                       thin ? " (thin edge)" : "", maxd, ee, ea);
            if (thin) {
                // Within 35% of the larger energy.
                FL_CHECK_LE(de * 100, (ee > ea ? ee : ea) * 35);
            } else {
                FL_CHECK_LE(maxd, 80);
                FL_CHECK_LE(de * 100, ee * 3 + 255);  // within 3%
            }
        }
    }
}

FL_TEST_CASE("drawPolygon scenes: TinyGL reference agrees where its assumptions hold") {
    // The reference is valid for simple (non-self-intersecting) polygons
    // without sub-pixel-tall edges. The bowtie (static edge order) and the
    // thin-edge frames are its documented failure cases: rendered (and
    // hash-pinned below), not compared.
    for (int s = 0; s < kPolyScNumKernelScenes; ++s) {
        if (s == kPolyScDegenerate) continue;
        for (int ti = 0; ti < kPolyScNumTimes; ++ti) {
            CRGB r[20 * 20], a[20 * 20];
            polyScRenderKernelScene(r, s, kPolyScTimes[ti], kPolyScRef);
            polyScRenderKernelScene(a, s, kPolyScTimes[ti], kPolyScABuffer);
            PolyScPt p[4];
            const bool thin = polyScHasThinEdge(p, polyScKernelFrame(s, kPolyScTimes[ti], p));
            const long er = polyScEnergy(r, 400), ea = polyScEnergy(a, 400);
            fl::printf("  %s t=%u%s: energy ref %ld abuffer %ld\n", kPolyScKernelScenes[s].name,
                       (unsigned)kPolyScTimes[ti], thin ? " (thin edge)" : "", er, ea);
            if (s == kPolyScBowtie || thin) continue;
            const long d = er > ea ? er - ea : ea - er;
            FL_CHECK_LE(d * 10, ea + 255);  // within 10%
        }
    }
}

FL_TEST_CASE("drawPolygon scenes: TinyGL reference orders shared-top edges across equal-x edges") {
    // Arrowhead (5,0) (8,8) (5,3) (2,8): the two edges leaving (5,3) share
    // xstart with the two leaving (5,0). The pair from (5,0) must still be
    // ordered left-then-right, or row 1 starts at the right edge and skips
    // the covered pixel (4,1).
    const PolyScPt arrow[4] = {
        {PolyScFix::from_raw(5 * 65536), PolyScFix::from_raw(0)},
        {PolyScFix::from_raw(8 * 65536), PolyScFix::from_raw(8 * 65536)},
        {PolyScFix::from_raw(5 * 65536), PolyScFix::from_raw(3 * 65536)},
        {PolyScFix::from_raw(2 * 65536), PolyScFix::from_raw(8 * 65536)}};
    CRGB r[10 * 10], a[10 * 10];
    for (int i = 0; i < 100; ++i) r[i] = a[i] = CRGB(0, 0, 0);
    fl::gfx::Canvas<CRGB> cr(fl::span<CRGB>(r, 100), 10, 10);
    fl::gfx::Canvas<CRGB> ca(fl::span<CRGB>(a, 100), 10, 10);
    const fl::DrawMode ow = fl::DrawMode::DRAW_MODE_OVERWRITE;
    polyScDraw<4, 1>(cr, kPolyScRef, CRGB(255, 255, 255), arrow, 4, ow);
    polyScDraw<4, 1>(ca, kPolyScABuffer, CRGB(255, 255, 255), arrow, 4, ow);
    FL_CHECK_GT((int)a[1 * 10 + 4].r, 0);
    FL_CHECK_GT((int)r[1 * 10 + 4].r, 0);
    // Rows 0..2 (above the notch) have only the two edges from (5,0)
    // active, so the reference must agree with ABuffer there. Below row 3
    // the four edges' x-order differs from their static order, the
    // reference's documented limitation for concave outlines.
    const long er = polyScEnergy(r, 30), ea = polyScEnergy(a, 30);
    const long d = er > ea ? er - ea : ea - er;
    fl::printf("  arrowhead rows 0..2: energy ref %ld abuffer %ld\n", er, ea);
    FL_CHECK_LE(d * 10, ea);  // within 10%
}

FL_TEST_CASE("drawPolygon scenes: degenerate polygons leave the canvas untouched") {
    for (int k = 0; k < 3; ++k)
        for (int ti = 0; ti < kPolyScNumTimes; ++ti) {
            CRGB buf[20 * 20];
            polyScRenderKernelScene(buf, kPolyScDegenerate, kPolyScTimes[ti], k);
            int changed = 0;
            for (int i = 0; i < kPolyScW * kPolyScW; ++i)
                if (!(buf[i] == kPolyScDegenerateBase)) ++changed;
            FL_CHECK_EQ(changed, 0);
        }
}

FL_TEST_CASE("drawPolygon scenes: Exact output does not depend on CHUNK") {
    for (int s = 0; s < kPolyScNumKernelScenes; ++s)
        for (int ti = 0; ti < kPolyScNumTimes; ++ti) {
            CRGB a[20 * 20], b[20 * 20];
            polyScRenderKernelScene<1>(a, s, kPolyScTimes[ti], kPolyScExact);
            polyScRenderKernelScene<64>(b, s, kPolyScTimes[ti], kPolyScExact);
            FL_CHECK_EQ(polyScHash(a, 400), polyScHash(b, 400));
        }
}

FL_TEST_CASE("drawPolygon scenes: kernel scene hashes are pinned") {
    // One hash per (scene, kernel) over all sample times.
    static const fl::u32 kWant[kPolyScNumKernelScenes][3] = {
        // Exact, ABuffer, TinyGL-ref
        {0x551419c6u, 0x997a15bbu, 0x240953d6u},  // bowtie
        {0x22b823b1u, 0xb3e4e3f5u, 0x76946385u},  // subpixel
        {0xfe2e97cdu, 0x55c89912u, 0x55c89912u},  // clamp
        {0x1bfa3bf9u, 0xb01f1885u, 0xaf87cc55u},  // sliver
        {0x2f20a585u, 0x2f20a585u, 0x2f20a585u},  // degenerate
        {0x31dbaa45u, 0x3a1656ddu, 0x5eddeb45u},  // diamond
    };
    for (int s = 0; s < kPolyScNumKernelScenes; ++s)
        for (int k = 0; k < 3; ++k) {
            fl::u32 h = 2166136261u;
            for (int ti = 0; ti < kPolyScNumTimes; ++ti) {
                CRGB buf[20 * 20];
                polyScRenderKernelScene(buf, s, kPolyScTimes[ti], k);
                h = polyScHash(buf, 400, h);
            }
            if (h != kWant[s][k])
                fl::printf("  %s kernel %d: got 0x%x\n", kPolyScKernelScenes[s].name, k,
                           (unsigned)h);
            FL_CHECK_EQ(h, kWant[s][k]);
        }
}

// ---- showcase scenes (formerly the PolyShow / PolyDemo sketches) ------------

FL_TEST_CASE("drawPolygon scenes: showcase frames agree across Exact and ABuffer") {
    for (int sz = 0; sz < 2; ++sz) {
        const int w = kPolyScShowSizes[sz][0], h = kPolyScShowSizes[sz][1];
        for (int s = 0; s < kPolyScNumShowScenes; ++s)
            for (int ti = 0; ti < kPolyScNumShowTimes; ++ti) {
                CRGB e[16 * 16], a[16 * 16];
                const fl::u32 t = kPolyScShowTimes[ti];
                polyScRenderShow(e, w, h, kPolyScShowScenes[s].fn, t, kPolyScExact);
                polyScRenderShow(a, w, h, kPolyScShowScenes[s].fn, t, kPolyScABuffer);
                const long ee = polyScEnergy(e, w * h), ea = polyScEnergy(a, w * h);
                const long d = ee > ea ? ee - ea : ea - ee;
                fl::printf("  %s %dx%d t=%u: energy exact %ld abuffer %ld\n",
                           kPolyScShowScenes[s].name, w, h, (unsigned)t, ee, ea);
                FL_CHECK_GT(ee, 0);  // every showcase frame draws something
                FL_CHECK_LE(d * 100, ee * kPolyScShowScenes[s].energyTolPct);
            }
    }
}

FL_TEST_CASE("drawPolygon scenes: MAXN caps the vertex count") {
    // The 14-vertex knot: any MAXN >= 14 renders identically (MAXN only
    // sizes the working set); the small-target default of 8 truncates it.
    for (int k = 0; k < 2; ++k) {
        CRGB exact14[16 * 16], big[16 * 16], small[16 * 16];
        polyScRenderShow(exact14, 16, 16, polyScKnotMaxN<14>, 2500, k);
        polyScRenderShow(big, 16, 16, polyScKnotMaxN<32>, 2500, k);
        polyScRenderShow(small, 16, 16, polyScKnotMaxN<8>, 2500, k);
        FL_CHECK_EQ(polyScHash(exact14, 256), polyScHash(big, 256));
        FL_CHECK_NE(polyScHash(exact14, 256), polyScHash(small, 256));
    }
}

FL_TEST_CASE("drawPolygon scenes: showcase hashes are pinned") {
    // One hash per (canvas size, scene, kernel) over all sample times.
    static const fl::u32 kWant[2][kPolyScNumShowScenes][2] = {
        {
            // 16x16: Exact, ABuffer
            {0xcb0bfb0fu, 0xc5766872u},  // pinwheel
            {0x779e1d81u, 0x8484f188u},  // layers
            {0x9d771341u, 0x9e0961c8u},  // knot
            {0xe907650bu, 0x16e68874u},  // blob
        },
        {
            // 16x8 (non-square)
            {0x1f8fab18u, 0x4b020489u},  // pinwheel
            {0x515bd9acu, 0x68184506u},  // layers
            {0xe155d34eu, 0x2c867decu},  // knot
            {0x16f909a3u, 0x3981582au},  // blob
        },
    };
    for (int sz = 0; sz < 2; ++sz) {
        const int w = kPolyScShowSizes[sz][0], h = kPolyScShowSizes[sz][1];
        for (int s = 0; s < kPolyScNumShowScenes; ++s)
            for (int k = 0; k < 2; ++k) {
                fl::u32 hash = 2166136261u;
                for (int ti = 0; ti < kPolyScNumShowTimes; ++ti) {
                    CRGB buf[16 * 16];
                    polyScRenderShow(buf, w, h, kPolyScShowScenes[s].fn, kPolyScShowTimes[ti], k);
                    hash = polyScHash(buf, w * h, hash);
                }
                if (hash != kWant[sz][s][k])
                    fl::printf("  %s %dx%d kernel %d: got 0x%x\n", kPolyScShowScenes[s].name, w,
                               h, k, (unsigned)hash);
                FL_CHECK_EQ(hash, kWant[sz][s][k]);
            }
    }
}

FL_TEST_CASE("drawPolygon scenes: float pentagram leaves its center empty (even-odd)") {
    // The Polygon example's star scene: float vertices, default Exact fill.
    // Float trig is not bit-identical across libms, so check structure only.
    CRGB buf[16 * 16];
    for (int i = 0; i < 256; ++i) buf[i] = CRGB(0, 0, 0);
    fl::CanvasRGB canvas(fl::span<CRGB>(buf, 256), 16, 16);
    fl::PolyPoint<float> star[5];
    for (int i = 0; i < 5; ++i) {
        const float a = -1.5707963f + (float)i * (2.0f * 6.2831853f / 5.0f);  // apex up
        star[i].x = 8.0f + 7.75f * fl::cosf(a);
        star[i].y = 8.0f + 7.75f * fl::sinf(a);
    }
    canvas.drawPolygon(CRGB(255, 140, 0), star);
    FL_CHECK_EQ((int)buf[8 * 16 + 8].r, 0);    // center pentagon: even crossings
    FL_CHECK_EQ((int)buf[4 * 16 + 8].r, 255);  // top arm, fully covered
    FL_CHECK_EQ((int)buf[15 * 16 + 1].r, 0);   // outside, between the lower arms
}

// ---- fl::scratch side-stack backend (#5306) ---------------------------------
// Host builds have FL_SCRATCH_STACK_BYTES == 0, so drawPolygon's working set
// is a stack local. These cases run the production kernels with the
// side-stack backend instead: detail::arena_scratch (the class fl::scratch
// is when FL_SCRATCH_STACK_BYTES > 0) over a guarded test arena, passed
// through the kernels' Scratch template parameter.

namespace {

struct PolyScGuardedArena;
PolyScGuardedArena* gPolyScArena = nullptr;  // the arena installed for a test

// Arena policy for detail::arena_scratch: leases from gPolyScArena.
struct PolyScArena {
    static void* acquire(fl::size bytes, fl::size align, fl::size* saved);
    static void release(fl::size saved, const void* block, fl::size bytes);
};

template <typename T>
using PolyScArenaScratch = fl::detail::arena_scratch<T, PolyScArena>;

// 8 KB arena with guard bytes on both sides, installed for the test's scope.
struct PolyScGuardedArena {
    enum : fl::size { kGuard = 16, kCap = 8192 };
    FL_ALIGNAS(16) fl::u8 mBytes[kGuard + kCap + kGuard];
    fl::detail::side_stack mStack;
    fl::size mCap;
    int mNonLifo;  // releases that were not LIFO (must stay 0)
    explicit PolyScGuardedArena(fl::size cap = kCap)
        : mStack(mBytes + kGuard, cap), mCap(cap), mNonLifo(0) {
        for (fl::size i = 0; i < sizeof(mBytes); ++i) mBytes[i] = 0xA5;
        gPolyScArena = this;
    }
    ~PolyScGuardedArena() { gPolyScArena = nullptr; }
    // The leading guard and everything past the arena's capacity (the
    // trailing guard, plus the unused tail of mBytes for a smaller arena).
    bool guardsIntact() const {
        for (fl::size i = 0; i < kGuard; ++i)
            if (mBytes[i] != 0xA5) return false;
        for (fl::size i = kGuard + mCap; i < sizeof(mBytes); ++i)
            if (mBytes[i] != 0xA5) return false;
        return true;
    }
};

void* PolyScArena::acquire(fl::size bytes, fl::size align, fl::size* saved) {
    return gPolyScArena->mStack.push(bytes, align, saved);
}
void PolyScArena::release(fl::size saved, const void* block, fl::size bytes) {
    if (!gPolyScArena->mStack.pop(saved, block, bytes)) ++gPolyScArena->mNonLifo;
}

/// polyScDraw (Exact / ABuffer) through the side-stack backend.
template <int MAXN, int CHUNK>
void polyScDrawArena(fl::gfx::Canvas<CRGB>& c, int kernel, const CRGB& color,
                     const PolyScPt* pts, int n, fl::DrawMode mode) {
    namespace d = fl::gfx::detail;
    const fl::span<const PolyScPt> sp(pts, (fl::size)n);
    const bool ow = (mode == fl::DrawMode::DRAW_MODE_OVERWRITE);
    if (kernel == kPolyScExact) {
        if (ow) d::drawPolygonCore<CRGB, PolyScFix, true, MAXN, CHUNK, PolyScArenaScratch>(c, color, sp);
        else d::drawPolygonCore<CRGB, PolyScFix, false, MAXN, CHUNK, PolyScArenaScratch>(c, color, sp);
    } else {
        if (ow) d::drawPolygonABufferCore<CRGB, PolyScFix, true, MAXN, PolyScArenaScratch>(c, color, sp);
        else d::drawPolygonABufferCore<CRGB, PolyScFix, false, MAXN, PolyScArenaScratch>(c, color, sp);
    }
}

/// A 24-point star (self-intersecting under even-odd), centred on a 20x20
/// canvas, rotated by step: exercises the 32/64 tier with many edges.
int polyScStar(PolyScPt* p, int step) {
    const int n = 24;
    for (int i = 0; i < n; ++i) {
        const fl::i32 r = (i & 1) ? 3 * 65536L : 9 * 65536L + 32768L;
        const fl::i32 a = (fl::i32)((i * 7 + step) % n) * (kPolyScTwoPi / n);
        p[i] = polyScPolar(kPolyScC, kPolyScC, r, a);
    }
    return n;
}

/// Render one polygon with the stack backend (polyScDraw) and the arena
/// backend into identical canvases (base pixel `base`, so blend mode is
/// exercised too) and require byte-identical output. Returns the number of
/// pixels the draw changed.
template <int MAXN, int CHUNK>
int polyScCheckArenaMatches(PolyScGuardedArena& arena, const PolyScPt* p, int n,
                             int kernel, fl::DrawMode mode, const CRGB& color,
                             const CRGB& base) {
    CRGB a[20 * 20], b[20 * 20];
    for (int i = 0; i < 400; ++i) a[i] = b[i] = base;
    fl::gfx::Canvas<CRGB> ca(fl::span<CRGB>(a, 400), 20, 20);
    fl::gfx::Canvas<CRGB> cb(fl::span<CRGB>(b, 400), 20, 20);
    polyScDraw<MAXN, CHUNK>(ca, kernel, color, p, n, mode);
    polyScDrawArena<MAXN, CHUNK>(cb, kernel, color, p, n, mode);
    FL_CHECK_EQ(polyScHash(a, 400), polyScHash(b, 400));
    FL_CHECK_EQ(arena.mStack.used(), fl::size(0));  // the lease was released
    int changed = 0;
    for (int i = 0; i < 400; ++i)
        if (!(b[i] == base)) ++changed;
    return changed;
}

}  // namespace

// Without an arena (the default host build) the default lease is the plain
// stack local.
#if FL_SCRATCH_STACK_BYTES == 0
FL_STATIC_ASSERT((fl::is_same<fl::gfx::detail::PolyScratchLease<fl::gfx::detail::PolyScratch<32, 64> >,
                              fl::detail::inline_scratch<fl::gfx::detail::PolyScratch<32, 64> > >::value),
                 "host drawPolygon working set must stay a stack local");
#endif

FL_TEST_CASE("drawPolygon scenes: side-stack scratch renders identically to the stack") {
    PolyScGuardedArena arena;
    const CRGB base(3, 40, 7);
    const fl::DrawMode modes[2] = {fl::DrawMode::DRAW_MODE_OVERWRITE,
                                   fl::DrawMode::DRAW_MODE_BLEND};
    for (int k = 0; k < 2; ++k)
        for (int m = 0; m < 2; ++m) {
            // Kernel scenes (<= 4 vertices) at the 8/16 and 32/64 tiers.
            for (int s = 0; s < kPolyScNumKernelScenes; ++s)
                for (int ti = 0; ti < kPolyScNumTimes; ++ti) {
                    PolyScPt p[4];
                    const int n = polyScKernelFrame(s, kPolyScTimes[ti], p);
                    const CRGB& color = kPolyScKernelScenes[s].color;
                    polyScCheckArenaMatches<8, 16>(arena, p, n, k, modes[m], color, base);
                    polyScCheckArenaMatches<32, 64>(arena, p, n, k, modes[m], color, base);
                }
            // A 24-point star: within the 32/64 tier, truncated at 8/16.
            for (int step = 0; step < 6; ++step) {
                PolyScPt p[24];
                const int n = polyScStar(p, step);
                FL_CHECK_GT((polyScCheckArenaMatches<8, 16>(arena, p, n, k, modes[m],
                                                             CRGB(200, 90, 255), base)), 0);
                FL_CHECK_GT((polyScCheckArenaMatches<32, 64>(arena, p, n, k, modes[m],
                                                              CRGB(200, 90, 255), base)), 0);
            }
        }
    // The working sets really came from the arena (the 32/64 Exact set is
    // the largest), every lease was released LIFO, and nothing was written
    // outside the arena.
    FL_CHECK_GE(arena.mStack.high_water(),
                (sizeof(fl::gfx::detail::PolyScratch<32, 64>)));
    FL_CHECK_EQ(arena.mNonLifo, 0);
    FL_CHECK(arena.guardsIntact());
}

FL_TEST_CASE("drawPolygon scenes: exhausted side-stack scratch makes the draw a no-op") {
    PolyScPt p[24];
    const int n = polyScStar(p, 1);
    const CRGB base(1, 2, 3);
    for (int k = 0; k < 2; ++k) {
        // An arena too small for the working set: nothing is drawn and the
        // arena is untouched.
        {
            PolyScGuardedArena arena(64);
            CRGB buf[20 * 20];
            for (int i = 0; i < 400; ++i) buf[i] = base;
            fl::gfx::Canvas<CRGB> c(fl::span<CRGB>(buf, 400), 20, 20);
            polyScDrawArena<32, 64>(c, k, CRGB(255, 255, 255), p, n,
                                    fl::DrawMode::DRAW_MODE_OVERWRITE);
            int changed = 0;
            for (int i = 0; i < 400; ++i)
                if (!(buf[i] == base)) ++changed;
            FL_CHECK_EQ(changed, 0);
            FL_CHECK_EQ(arena.mStack.used(), fl::size(0));
            FL_CHECK_EQ(arena.mStack.high_water(), fl::size(0));
            FL_CHECK(arena.guardsIntact());
        }
        // A large-enough arena already held by an outer lease: the draw is
        // skipped, the outer block keeps its contents, and once the outer
        // lease is released the same draw renders normally.
        {
            PolyScGuardedArena arena;
            CRGB buf[20 * 20];
            for (int i = 0; i < 400; ++i) buf[i] = base;
            fl::gfx::Canvas<CRGB> c(fl::span<CRGB>(buf, 400), 20, 20);
            const fl::size outerBytes = PolyScGuardedArena::kCap - 512;
            fl::size saved = 0;
            fl::u8* outer = static_cast<fl::u8*>(arena.mStack.push(outerBytes, 1, &saved));
            FL_REQUIRE(outer != nullptr);
            for (fl::size i = 0; i < outerBytes; ++i) outer[i] = (fl::u8)(i * 31u);
            polyScDrawArena<32, 64>(c, k, CRGB(255, 255, 255), p, n,
                                    fl::DrawMode::DRAW_MODE_OVERWRITE);
            int changed = 0;
            for (int i = 0; i < 400; ++i)
                if (!(buf[i] == base)) ++changed;
            FL_CHECK_EQ(changed, 0);
            FL_CHECK_EQ(arena.mStack.used(), outerBytes);
            bool outerIntact = true;
            for (fl::size i = 0; i < outerBytes; ++i)
                if (outer[i] != (fl::u8)(i * 31u)) outerIntact = false;
            FL_CHECK(outerIntact);
            FL_CHECK(arena.mStack.pop(saved, outer, outerBytes));

            FL_CHECK_GT((polyScCheckArenaMatches<32, 64>(arena, p, n, k,
                                                          fl::DrawMode::DRAW_MODE_OVERWRITE,
                                                          CRGB(255, 255, 255), base)), 0);
            FL_CHECK(arena.guardsIntact());
        }
    }
}

}  // FL_TEST_FILE
