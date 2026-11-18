/// @file    PolyDemo.ino
/// @brief   Color drawPolygon showcase: six full-color animated scenes
/// @example PolyDemo.ino
// FL_AGENT_ALLOW_NEW_EXAMPLE
//
// A color, motion-rich demonstration of the antialiased drawPolygon
// ABuffer fill (src/fl/gfx/poly_abuffer.h) on an ATmega2560. The canvas
// is a fl::CanvasRGB view directly over the LED array (no copy); wire a
// 20x20 WS2812 matrix in row-major order to PIN_DATA. Six ~5 s scenes
// cycle with fade-to-black transitions:
//
//   1. Kaleido    - a small triangle orbiting the center; the base
//                   triangle is rotated about the center by 60-degree
//                   table steps into six exact-fold additive copies,
//                   hue per copy
//   2. Gasket     - a level-3 Sierpinski gasket built level by level
//                   (color fill, black-erased holes, per-level hue) with
//                   three small additive triangles orbiting the center
//   3. Blob       - a 12-vertex polygon whose radius is the sum of two
//                   rotating 3x and 5x harmonics, plus a counter-rotating
//                   9-vertex inner blob, additive, complementary hues
//   4. Knot       - two self-intersecting 14-vertex Lissajous polygons
//                   (2:3) whose phase angles drift at different speeds;
//                   the even-odd rule fills the morphing lobes, additive
//   5. Fireworks  - deterministic radial bursts of eight expanding
//                   petals while the background decays (trails),
//                   additive
//   6. Pinwheel   - six rotating sail blades, additive, hue per blade;
//                   the overlaps sum toward white in the center
//
// "Additive" is the canvas's default DrawMode::DRAW_MODE_BLEND: channel
// sums saturate at 255, so overlapping shapes brighten toward white.
//
// -DPW/-DPH override the canvas (scenes are defined in 20x20 space and
// scaled at draw time, e.g. -DPW=16 -DPH=8).
// -DPOLYDEMO_EXACT=1 swaps in the Exact backend for an AA comparison.
// With POLYDEMO_SERIAL=1 (off by default) it prints a startup banner and
// a 20x20 brightness preview at each scene change.
//
// Integer-only: s16x16 Q16.16 coordinates, s16x16::sincos drives all the
// smooth motion (no floats in the hot path), i64 intermediates for
// products.

// @filter
// require:
//   - memory: low
// exclude:
//   - board: nano
//   - board: nano_every
//   - board: uno
//   - board: arduino_uno_q
//   - board: atmega8a
//   - board: attiny1616
//   - board: leonardo
// @end-filter
// Measured on the ATmega2560 firmware: the default build totals 1.7 KB
// SRAM (the 1,200 B LED buffer is the largest static) and 31.4 KB flash;
// -DPOLYDEMO_SERIAL=1 -DPOLYDEMO_EXACT=1 measures 1.9 KB SRAM and 46.3 KB
// flash. So the 1 KB (atmega8a) and 2 KB (nano, uno, ...) boards are
// excluded. The 2.5 KB "leonardo" fits the default build with thin
// margin and is excluded like its drawPolygon siblings; the serial
// build does not fit at all.

#include <Arduino.h>
#include <FastLED.h>
#include <fl/gfx/gfx.h>

// Host (non-AVR) builds of this example have no flash; keep the macro
// meaningful on AVR and inert elsewhere.
#ifndef PROGMEM
#define PROGMEM
#endif

#ifndef PW
#define PW 20
#endif
#ifndef PH
#define PH 20
#endif

// Fill backend: 0 = ABuffer (default, the demoed kernel), 1 = Exact.
#ifndef POLYDEMO_EXACT
#define POLYDEMO_EXACT 0
#endif
#if POLYDEMO_EXACT
#define POLYDEMO_FILL fl::gfx::PolyFill::Exact
#else
#define POLYDEMO_FILL fl::gfx::PolyFill::ABuffer
#endif

// Serial output: startup banner plus a brightness preview at every scene
// change. Off by default - the LED show needs no Serial, and compiling it
// out removes the HardwareSerial code path and its ring buffers.
#ifndef POLYDEMO_SERIAL
#define POLYDEMO_SERIAL 0
#endif

#define NUM_LEDS (PW * PH)
#ifndef PIN_DATA
#define PIN_DATA 3
#endif

CRGB leds[NUM_LEDS];

static const int W = PW;
static const int H = PH;

// drawPolygon tiers: at most 16 vertices (the knot uses 14), 1-px chunk.
static const int TIER_MAXN = 16;
static const int TIER_CHUNK = 1;

// The canvas is a view over the LED array itself - no copy anywhere.
fl::CanvasRGB canvas(fl::span<CRGB>(leds, NUM_LEDS), W, H);

typedef fl::PolyPoint<fl::s16x16> PP;

// ---- 20x20-space staging -----------------------------------------------------
// All scene math happens in 20x20 Q16.16 space (center at 10,10); mapPt
// projects a point onto the actual (W, H) canvas about its center - an
// identity map at the default 20x20, e.g. radius 8 px becomes 4 px on a
// 16x8 canvas. i64 products: |dx| < 15 px * 65536 and |scale| < 2^20,
// so the terms stay < 2^38.
static const fl::i32 kBaseC = 10 * 65536L;                 // 20x20-space center
static const fl::i32 kCX = (fl::i32)((fl::i64)W * 65536L / 2);
static const fl::i32 kCY = (fl::i32)((fl::i64)H * 65536L / 2);
static const fl::i32 kSX = (fl::i32)((fl::i64)W * 65536L / 20);
static const fl::i32 kSY = (fl::i32)((fl::i64)H * 65536L / 20);

static PP sPoly[TIER_MAXN];  // vertex staging (RAM, one polygon at a time)
static PP sTri[3];           // small 3-vertex staging

static void mapPt(PP& p) {
    p.x = fl::s16x16::from_raw(
        kCX + (fl::i32)(((fl::i64)(p.x.raw() - kBaseC) * kSX) >> 16));
    p.y = fl::s16x16::from_raw(
        kCY + (fl::i32)(((fl::i64)(p.y.raw() - kBaseC) * kSY) >> 16));
}

static void mapPts(PP* p, int n) {
    for (int i = 0; i < n; ++i) mapPt(p[i]);
}

// ---- fixed-point angles ------------------------------------------------------
// 2*pi in Q16.16 radians.
static const fl::i32 kTwoPi = 411775L;
static const fl::i32 kHalfPi = 104720L;    // pi/2
static const fl::i32 kThirdTwoPi = 137445L;  // 2*pi/3 (120 deg)
static const fl::i32 kHalf = 209440L;      // pi
static const fl::i32 kOct = 51468L;        // 2*pi/8 (45 deg)
static const fl::i32 kSix = 68630L;        // 2*pi/6 (60 deg)
static const fl::i32 k022 = 14417L;        // 0.22 rad (firework petal half-angle)
static const fl::i32 k09 = 58982L;         // 0.9 rad (pinwheel blade sweep)
static const fl::i32 k07 = 45875L;         // 0.7 rad

// Trig tables in flash (PROGMEM). kSin60/kCos60 step a point around the
// center in 60-degree increments (kaleido copies); kSin30/kCos30 and
// kSin40/kCos40 place the blob's 12 and 9 vertices without per-vertex
// sincos. Staged to plain RAM once by stageTrig() below.
static const fl::i32 kSin60[6] PROGMEM = { 0, 56755, 56755, 0, -56755, -56755 };
static const fl::i32 kCos60[6] PROGMEM = { 65536, 32768, -32768, -65536, -32768, 32768 };
static const fl::i32 kSin30[12] PROGMEM = {
    0, 32768, 56755, 65536, 56755, 32768, 0, -32768, -56755, -65536, -56755, -32768 };
static const fl::i32 kCos30[12] PROGMEM = {
    65536, 56755, 32768, 0, -32768, -56755, -65536, -56755, -32768, 0, 32768, 56755 };
static const fl::i32 kSin40[9] PROGMEM = {
    0, 42125, 64541, 56755, 22414, -22414, -56755, -64541, -42125 };
static const fl::i32 kCos40[9] PROGMEM = {
    65536, 50150, 11379, -32768, -61584, -61584, -32768, 11379, 50150 };

// Plain-RAM copies of the flash tables, staged once on first use: the
// show runs forever, so the per-frame pgm_read_dword loops are not worth
// it. (On non-AVR hosts the source arrays are already in RAM and the
// "staging" is a plain one-time copy.)
static fl::i32 sSin60[6], sCos60[6];
static fl::i32 sSin30[12], sCos30[12];
static fl::i32 sSin40[9], sCos40[9];

static void stageTrig() {
    static bool done = false;
    if (done) return;
#if defined(__AVR__)
    for (int i = 0; i < 6; ++i) {
        sSin60[i] = pgm_read_dword(&kSin60[i]);
        sCos60[i] = pgm_read_dword(&kCos60[i]);
    }
    for (int i = 0; i < 12; ++i) {
        sSin30[i] = pgm_read_dword(&kSin30[i]);
        sCos30[i] = pgm_read_dword(&kCos30[i]);
    }
    for (int i = 0; i < 9; ++i) {
        sSin40[i] = pgm_read_dword(&kSin40[i]);
        sCos40[i] = pgm_read_dword(&kCos40[i]);
    }
#else
    for (int i = 0; i < 6; ++i) { sSin60[i] = kSin60[i]; sCos60[i] = kCos60[i]; }
    for (int i = 0; i < 12; ++i) { sSin30[i] = kSin30[i]; sCos30[i] = kCos30[i]; }
    for (int i = 0; i < 9; ++i) { sSin40[i] = kSin40[i]; sCos40[i] = kCos40[i]; }
#endif
    done = true;
}

// Angle (Q16.16 radians, [0, 2*pi)) for time t with a period in ms.
static fl::i32 angAt(unsigned long t, unsigned long periodMs) {
    return (fl::i32)(((fl::i64)t * kTwoPi) / periodMs) % kTwoPi;
}

// Wrap an angle into [0, 2*pi) - s16x16::sincos needs a non-negative input.
static fl::i32 normA(fl::i32 a) {
    a %= kTwoPi;
    if (a < 0) a += kTwoPi;
    return a;
}

// Point at (cx, cy) + r * (cos a, sin a), in 20x20 space (unmapped).
// i64 products: |s/c| <= 65536 and r < 20 px * 65536, so terms < 2^43.
static PP polarPt20(fl::i32 cx, fl::i32 cy, fl::i32 r, fl::i32 a) {
    fl::s16x16 s, c;
    fl::s16x16::sincos(fl::s16x16::from_raw(a), s, c);
    PP p;
    p.x = fl::s16x16::from_raw(
        cx + (fl::i32)(((fl::i64)c.raw() * r) >> 16));
    p.y = fl::s16x16::from_raw(
        cy + (fl::i32)(((fl::i64)s.raw() * r) >> 16));
    return p;
}

static void polyDraw(const CRGB& color, const PP* pts, int n, fl::DrawMode mode) {
    canvas.drawPolygon<fl::s16x16, TIER_MAXN, TIER_CHUNK, POLYDEMO_FILL>(
        color, fl::span<const PP>(pts, n), mode);
}

// ---- scenes ------------------------------------------------------------------

static const unsigned long kSceneMs = 5000;
static const int kNumScenes = 6;

// 1. Kaleido: a small triangle orbits the center in one 60-degree sector
// (radial wobble, 3x self-spin); rotating the base triangle about the
// canvas center by 60-degree table steps gives six exact-fold additive
// copies, hue per copy. Rotation i64 terms: |dx| < 10 px * 65536, < 2^36.
static void sceneKaleido(unsigned long t) {
    memset(leds, 0, sizeof leds);
    stageTrig();
    const fl::i32 a = angAt(t, 9000L);        // orbit angle
    const fl::i32 w = angAt(t, 2400L);        // radial wobble
    const fl::i32 ws = fl::s16x16::sin(fl::s16x16::from_raw(w)).raw();
    const fl::i32 rOrb = 294912L + (fl::i32)(((fl::i64)98304L * ws) >> 16);  // 4.5 + 1.5 px
    const fl::i32 spin = (fl::i32)(((fl::i64)3 * a) % kTwoPi);
    const PP cen = polarPt20(kBaseC, kBaseC, rOrb, a);
    for (int j = 0; j < 3; ++j)
        sPoly[j] = polarPt20(cen.x.raw(), cen.y.raw(), 144179L,
                             normA(spin + j * kThirdTwoPi));  // 2.2 px
    const fl::u8 hueBase = (fl::u8)(t / 40L);  // slow drift, full wheel ~10 s
    for (int k = 0; k < 6; ++k) {
        const fl::i32 s = sSin60[k], c = sCos60[k];
        for (int j = 0; j < 3; ++j) {
            const fl::i32 dx = sPoly[j].x.raw() - kBaseC;
            const fl::i32 dy = sPoly[j].y.raw() - kBaseC;
            sTri[j].x = fl::s16x16::from_raw(
                kBaseC + (fl::i32)((((fl::i64)c * dx) - ((fl::i64)s * dy)) >> 16));
            sTri[j].y = fl::s16x16::from_raw(
                kBaseC + (fl::i32)((((fl::i64)s * dx) + ((fl::i64)c * dy)) >> 16));
        }
        mapPts(sTri, 3);
        polyDraw(CHSV((fl::u8)((hueBase + 40u * (fl::u8)k) & 0xFFu), 210, 240),
                 sTri, 3, fl::DrawMode::DRAW_MODE_BLEND);
    }
}

// Level by level: fill the up-triangle with color, erase the center
// down-triangle with black (OVERWRITE), then recurse into the three
// corner sub-triangles. depth 2 -> 26 small drawPolygon calls whose
// fills and erasures compose into the level-3 gasket.
static void gasketRec(int depth, fl::i32 cx, fl::i32 cy, fl::i32 r, fl::i32 rot,
                      fl::u8 hueBase) {
    for (int j = 0; j < 3; ++j)
        sPoly[j] = polarPt20(cx, cy, r, normA(rot - kHalfPi + j * kThirdTwoPi));
    mapPts(sPoly, 3);
    const fl::u8 hue = (fl::u8)((hueBase + (fl::u8)(2 - depth) * 40u) & 0xFFu);
    polyDraw(CHSV(hue, 220, 255), sPoly, 3, fl::DrawMode::DRAW_MODE_OVERWRITE);

    for (int j = 0; j < 3; ++j)
        sPoly[j] = polarPt20(cx, cy, r >> 1, normA(rot + kHalfPi + j * kThirdTwoPi));
    mapPts(sPoly, 3);
    polyDraw(CRGB::Black, sPoly, 3, fl::DrawMode::DRAW_MODE_OVERWRITE);

    if (depth > 0) {
        for (int k = 0; k < 3; ++k) {
            PP corner = polarPt20(cx, cy, r, normA(rot - kHalfPi + k * kThirdTwoPi));
            gasketRec(depth - 1, (cx + corner.x.raw()) >> 1,
                      (cy + corner.y.raw()) >> 1, r >> 1, rot, hueBase);
        }
    }
}

// 2. Gasket: a slowly rotating level-3 Sierpinski gasket, per-level hue,
// with three small additive triangles orbiting the center in the
// complementary hue.
static void sceneGasket(unsigned long t) {
    memset(leds, 0, sizeof leds);
    const fl::i32 rot = angAt(t, 24000L);
    const fl::u8 hueBase = (fl::u8)(t / 40L);
    gasketRec(2, kBaseC, kBaseC, 524288L, rot, hueBase);  // 8 px circumradius
    for (int k = 0; k < 3; ++k) {
        const fl::i32 phi = normA(kHalfPi - angAt(t, 12000L) + k * kThirdTwoPi);
        PP cen = polarPt20(kBaseC, kBaseC, 163840L, phi);  // 2.5 px orbit
        for (int j = 0; j < 3; ++j)
            sTri[j] = polarPt20(cen.x.raw(), cen.y.raw(), 117965L,  // 1.8 px
                                normA(rot + j * kThirdTwoPi));
        mapPts(sTri, 3);
        polyDraw(CHSV((fl::u8)(hueBase + 128u), 200, 255), sTri, 3,
                 fl::DrawMode::DRAW_MODE_BLEND);
    }
}

// Blob polygon: n vertices, radius a sum of two rotating harmonics.
// i64 terms: 131072 * 65536 < 2^37.
static void blobPts(int n, const fl::i32* sinT, const fl::i32* cosT,
                    fl::i32 cx, fl::i32 cy, fl::i32 r0,
                    fl::i32 a1, fl::i32 phi1, fl::i32 a2, fl::i32 phi2) {
    for (int i = 0; i < n; ++i) {
        const fl::i32 th = normA((fl::i32)(((fl::i64)i * kTwoPi) / n));
        const fl::i32 s1 = fl::s16x16::sin(fl::s16x16::from_raw(normA(th * 3 + phi1))).raw();
        const fl::i32 s2 = fl::s16x16::sin(fl::s16x16::from_raw(normA(th * 5 + phi2))).raw();
        const fl::i32 r = r0 + (fl::i32)(((fl::i64)a1 * s1) >> 16)
                                 + (fl::i32)(((fl::i64)a2 * s2) >> 16);
        sPoly[i].x = fl::s16x16::from_raw(
            cx + (fl::i32)(((fl::i64)cosT[i] * r) >> 16));
        sPoly[i].y = fl::s16x16::from_raw(
            cy + (fl::i32)(((fl::i64)sinT[i] * r) >> 16));
    }
}

// 3. Blob: a 12-vertex morphing polygon (3x and 5x harmonics) with a
// counter-rotating 9-vertex inner blob, additive, complementary hues,
// the whole pair wandering on a slow Lissajous.
static void sceneBlob(unsigned long t) {
    memset(leds, 0, sizeof leds);
    stageTrig();
    const fl::i32 w1 = angAt(t, 8000L), w2 = angAt(t, 5300L);
    const fl::i32 ox = (fl::i32)(((fl::i64)78643L * fl::s16x16::sin(fl::s16x16::from_raw(w1)).raw()) >> 16);  // 1.2 px
    const fl::i32 oy = (fl::i32)(((fl::i64)78643L * fl::s16x16::cos(fl::s16x16::from_raw(w2)).raw()) >> 16);
    const fl::i32 cx = kBaseC + ox, cy = kBaseC + oy;
    const fl::i32 p1 = angAt(t, 3200L), p2 = normA(angAt(t, 2100L) + k07);
    const fl::u8 hue = (fl::u8)(t / 30L);

    blobPts(12, sSin30, sCos30, cx, cy, 425984L, 131072L, p1, 72090L, p2);
    // 6.5 px + 2 px * sin(3 th + p1) + 1.1 px * sin(5 th + p2)
    mapPts(sPoly, 12);
    polyDraw(CHSV(hue, 215, 255), sPoly, 12, fl::DrawMode::DRAW_MODE_BLEND);

    blobPts(9, sSin40, sCos40, cx, cy, 229376L, 85197L, normA(kHalf - p1),
            45875L, normA(p2 + kHalf));
    // 3.5 px + 1.3 px * sin(3 th - p1) + 0.7 px * sin(5 th + p2 + pi)
    mapPts(sPoly, 9);
    polyDraw(CHSV((fl::u8)(hue + 140u), 200, 240), sPoly, 9,
             fl::DrawMode::DRAW_MODE_BLEND);
}

// 4. Knot: a closed 14-vertex Lissajous polygon (2:3). The curve
// self-intersects; the even-odd fill paints the lobes. Two knots with
// offset phases drift against each other, additive.
static void knotPts(fl::i32 cx, fl::i32 cy, fl::i32 R, fl::i32 ph1, fl::i32 ph2) {
    for (int i = 0; i < 14; ++i) {
        const fl::i32 th = normA((fl::i32)(((fl::i64)i * kTwoPi) / 14));
        const fl::i32 th2 = normA(2 * th);
        const fl::i32 th3 = normA(3 * th);
        const fl::i32 sx = fl::s16x16::sin(fl::s16x16::from_raw(normA(th2 + ph1))).raw();
        const fl::i32 sy = fl::s16x16::sin(fl::s16x16::from_raw(normA(th3 + ph2))).raw();
        sPoly[i].x = fl::s16x16::from_raw(cx + (fl::i32)(((fl::i64)R * sx) >> 16));
        sPoly[i].y = fl::s16x16::from_raw(cy + (fl::i32)(((fl::i64)R * sy) >> 16));
    }
}

static void sceneKnot(unsigned long t) {
    memset(leds, 0, sizeof leds);
    const fl::i32 ph1 = angAt(t, 11000L);
    const fl::i32 ph2 = normA(angAt(t, 8700L) + k07);
    const fl::i32 hueS = fl::s16x16::sin(fl::s16x16::from_raw(angAt(t, 20000L))).raw();
    const fl::u8 hue = (fl::u8)(170 + (fl::i32)(hueS * 40L / 65536L));
    for (int pass = 0; pass < 2; ++pass) {
        // The second knot is the first mirrored (x phase + pi).
        knotPts(kBaseC, kBaseC, 524288L, normA(ph1 + pass * kHalf), ph2);  // 8 px
        mapPts(sPoly, 14);
        polyDraw(CHSV((fl::u8)(hue + pass * 90u), 190, 255), sPoly, 14,
                 fl::DrawMode::DRAW_MODE_BLEND);
    }
}

// 5. Fireworks: the frame decays every tick; every 1.2 s a deterministic
// burst of eight thin triangles expands from a pseudo-random point.
static void sceneFireworks(unsigned long t, bool fresh) {
    if (fresh) {
        memset(leds, 0, sizeof leds);
    } else {
        for (int i = 0; i < NUM_LEDS; ++i)
            leds[i] = nblend(leds[i], CRGB::Black, 16);
    }
    const unsigned long b = t / 1200L;
    const unsigned long tb = t % 1200L;
    if (tb >= 700L) return;
    fl::u32 h = (fl::u32)b * 2654435761u + 97u;
    const fl::i32 cx = kBaseC + ((fl::i32)((h >> 8) % 11u) - 5) * 65536L;
    const fl::i32 cy = kBaseC + ((fl::i32)((h >> 16) % 11u) - 5) * 65536L;
    const fl::u8 hueB = (fl::u8)((h >> 24) & 0xFFu);
    const fl::i32 spin = angAt(t, 4000L);
    const fl::i32 L = 98304L + (fl::i32)(((fl::i64)458752L * tb) / 700L);  // 1.5 + 7 px * p
    sTri[0].x = fl::s16x16::from_raw(cx);
    sTri[0].y = fl::s16x16::from_raw(cy);
    if (tb < 90L) {
        // Opening flash: a bright 4-vertex diamond at the burst point.
        const fl::i32 fr = (fl::i32)((90L - tb) * 4587L / 90L);  // up to 0.7 px
        PP d[4] = {
            { fl::s16x16::from_raw(cx), fl::s16x16::from_raw(cy - fr) },
            { fl::s16x16::from_raw(cx + fr), fl::s16x16::from_raw(cy) },
            { fl::s16x16::from_raw(cx), fl::s16x16::from_raw(cy + fr) },
            { fl::s16x16::from_raw(cx - fr), fl::s16x16::from_raw(cy) },
        };
        mapPts(d, 4);
        polyDraw(CRGB(255, 240, 200), d, 4, fl::DrawMode::DRAW_MODE_BLEND);
    }
    for (int k = 0; k < 8; ++k) {
        const fl::i32 a = (spin + k * kOct) % kTwoPi;
        sTri[1] = polarPt20(cx, cy, L, normA(a - k022));
        sTri[2] = polarPt20(cx, cy, L, normA(a + k022));
        mapPts(sTri, 3);
        polyDraw(CHSV((fl::u8)((hueB + 10u * (fl::u8)k) & 0xFFu), 200, 255),
                 sTri, 3, fl::DrawMode::DRAW_MODE_BLEND);
    }
}

// 6. Pinwheel: six sail blades (center, tip, swept trailing edge)
// rotating, additive; the center overlaps sum toward white.
static void scenePinwheel(unsigned long t) {
    memset(leds, 0, sizeof leds);
    const fl::i32 spin = angAt(t, 7000L);
    const fl::i32 w = angAt(t, 1600L);
    const fl::i32 ws = fl::s16x16::sin(fl::s16x16::from_raw(w)).raw();
    const fl::i32 L = 491520L + (fl::i32)(((fl::i64)98304L * ws) >> 16);  // 7.5 + 1.5 px
    const fl::i32 L3 = (fl::i32)(((fl::i64)L * 36045L) >> 16);  // 0.55 * L
    const fl::u8 hueBase = (fl::u8)(t / 40L);
    for (int k = 0; k < 6; ++k) {
        // Sail blade: center, tip at L, trailing edge at 0.55 L swept
        // 0.9 rad back (a 60-degree gap leaves the blades distinct).
        const fl::i32 a = (spin + k * kSix) % kTwoPi;
        sTri[0].x = fl::s16x16::from_raw(kBaseC);
        sTri[0].y = fl::s16x16::from_raw(kBaseC);
        sTri[1] = polarPt20(kBaseC, kBaseC, L, a);
        sTri[2] = polarPt20(kBaseC, kBaseC, L3, normA(a + k09));
        mapPts(sTri, 3);
        polyDraw(CHSV((fl::u8)((hueBase + 42u * (fl::u8)k) & 0xFFu), 200, 240),
                 sTri, 3, fl::DrawMode::DRAW_MODE_BLEND);
    }
}

// ---- frame driver ------------------------------------------------------------

static int sLastScene = -1;

static void fadeFrame(unsigned long tLocal) {
    // Fade to black over 350 ms at each scene boundary (both ends).
    static const unsigned long kEdge = 350;
    if (tLocal >= kEdge && tLocal <= kSceneMs - kEdge) return;
    const unsigned long f255 = (tLocal < kEdge)
        ? (255UL * tLocal / kEdge)
        : (255UL * (kSceneMs - tLocal) / kEdge);
    const fl::u8 toBlack = (fl::u8)(255UL - f255);
    for (int i = 0; i < NUM_LEDS; ++i)
        leds[i] = nblend(leds[i], CRGB::Black, toBlack);
}

// One frame of the show at time t (ms). loop() adds FastLED.show(); the
// host build (stub platform) drives this directly with its own clock.
void polyDemoFrame(unsigned long t) {
    const int scene = (int)((t / kSceneMs) % (unsigned long)kNumScenes);
    const unsigned long tLocal = t % kSceneMs;
    const bool fresh = (scene != sLastScene);
    sLastScene = scene;
    switch (scene) {
        case 0: sceneKaleido(tLocal); break;
        case 1: sceneGasket(tLocal); break;
        case 2: sceneBlob(tLocal); break;
        case 3: sceneKnot(tLocal); break;
        case 4: sceneFireworks(tLocal, fresh); break;
        default: scenePinwheel(tLocal); break;
    }
    fadeFrame(tLocal);
}

void loop() {
    polyDemoFrame(millis());
    FastLED.show();
}

#if POLYDEMO_SERIAL
static const char* sceneName(int scene) {
    switch (scene) {
        case 0: return "kaleido (6-fold, additive)";
        case 1: return "gasket (Sierpinski, black-erase)";
        case 2: return "blob (dual harmonic morph)";
        case 3: return "knot (self-intersecting, even-odd)";
        case 4: return "fireworks (bursts, decaying trails)";
        default: return "pinwheel (6 blades, additive)";
    }
}

static void printPreview(int scene) {
    Serial.print(F("scene: "));
    Serial.println(sceneName(scene));
    static const char kHex[] = "0123456789ABCDEF";
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const CRGB& c = leds[y * W + x];
            const fl::u8 m = c.r > c.g ? (c.r > c.b ? c.r : c.b)
                                       : (c.g > c.b ? c.g : c.b);
            char c2[3] = { kHex[m >> 4], kHex[m & 15], 0 };
            Serial.print(c2);
        }
        Serial.println();
    }
    Serial.println(F("---"));
}
#endif

void setup() {
#if POLYDEMO_SERIAL
    Serial.begin(115200);
    delay(500);  // let a serial monitor attach before the show starts
    Serial.print(F("PolyDemo: drawPolygon ABuffer showcase "));
    Serial.print(PW);
    Serial.print('x');
    Serial.println(PH);
#endif
    FastLED.addLeds<NEOPIXEL, PIN_DATA>(leds, NUM_LEDS);
    FastLED.setBrightness(255);
}
