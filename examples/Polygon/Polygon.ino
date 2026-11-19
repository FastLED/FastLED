/// @file    Polygon.ino
/// @brief   Antialiased filled polygons with fl::CanvasRGB::drawPolygon
/// @example Polygon.ino
//
// Draws filled, antialiased polygons straight into the LED array through a
// fl::CanvasRGB view (no copy). Wire a WIDTH x HEIGHT WS2812 matrix in
// row-major order to PIN_DATA. Four 5 s scenes, each showing one part of
// the drawPolygon API:
//
//   1. Pinwheel - s16x16 (Q16.16) vertices, ABuffer fill, additive blend:
//                 six overlapping blades sum toward white in the middle
//   2. Star     - float vertices, Exact fill (the default) and the even-odd
//                 rule: a self-intersecting pentagram leaves its center
//                 pentagon empty
//   3. Layers   - DRAW_MODE_OVERWRITE: three rotating squares drawn back
//                 to front, each hiding the ones below
//   4. Knot     - two 14-vertex self-intersecting Lissajous curves, more
//                 vertices than the small-stack default MAXN of 8
//
// The API in brief:
//
//   canvas.drawPolygon(color, pts);  // Exact fill, additive blend
//   canvas.drawPolygon(color, pts, fl::DrawMode::DRAW_MODE_OVERWRITE);
//   canvas.drawPolygon<Coord, MAXN, CHUNK, fl::gfx::PolyFill::ABuffer>(
//       color, pts, mode);
//
// pts is an array (or fl::span) of fl::PolyPoint<Coord>; Coord may be
// float, int or a fixed-point type such as fl::s16x16. Both fill backends
// use the even-odd rule and the same edge conventions:
//
//   Exact   - exact per-pixel area coverage (256 levels). The default.
//   ABuffer - 8x4 subpixel sampling (32 levels): faster on dense fills and
//             a shallower stack. It ignores CHUNK.
//
// MAXN is a compile-time vertex cap that sizes the kernel's stack frame;
// vertices past MAXN are ignored. The default (FL_GFX_POLYGON_DEFAULT_MAXN)
// is 8 on small-stack targets (AVR, ESP8266, ...) and 32 elsewhere, so a
// polygon with more vertices must ask for a bigger MAXN, and a known small
// polygon can ask for a smaller one to save stack. CHUNK (Exact only) is a
// power-of-two column-chunk width; the default is fine.

// @filter
// require:
//   - memory: low
// exclude:
//   - board: uno
//   - board: nano
//   - board: nano_every
//   - board: leonardo
//   - board: attiny1616
// @end-filter
// The four scenes instantiate four polygon kernels (Exact for float and
// s16x16, ABuffer at two MAXN tiers) plus soft-float math: ~50 KB of AVR
// flash, more than the 16-48 KB parts above have. Mega 2560 fits (50.9 KB
// flash, 940 B static RAM plus a ~1.2 KB worst-case polygon stack frame).

#include <Arduino.h>
#include <FastLED.h>
#include <fl/gfx/gfx.h>
#include <fl/math/math.h>  // fl::sinf / fl::cosf for the float scene

#ifndef WIDTH
#define WIDTH 16
#endif
#ifndef HEIGHT
#define HEIGHT 16
#endif
#define NUM_LEDS (WIDTH * HEIGHT)

#ifndef PIN_DATA
#define PIN_DATA FL_PIN_CLOCKLESS_1
#endif

CRGB leds[NUM_LEDS];

// The canvas is a view over the LED array itself - no copy anywhere.
fl::CanvasRGB canvas(fl::span<CRGB>(leds, NUM_LEDS), WIDTH, HEIGHT);

typedef fl::s16x16 Fix;  // Q16.16 fixed point: raw value = real * 65536
typedef fl::PolyPoint<Fix> FixPt;

// ---- Q16.16 helpers ----------------------------------------------------------

static const fl::i32 kOne = 65536L;
static const fl::i32 kTwoPi = 411775L;  // 2*pi radians
static const fl::i32 kPi = 205887L;     // pi
static const fl::i32 kCX = WIDTH * kOne / 2;  // canvas center
static const fl::i32 kCY = HEIGHT * kOne / 2;
static const fl::i32 kR = (WIDTH < HEIGHT ? WIDTH : HEIGHT) * kOne / 2;

static fl::i32 mulQ16(fl::i32 a, fl::i32 b) {
    return (fl::i32)(((fl::i64)a * b) >> 16);
}

// Angle in [0, 2*pi) for time t (ms) and a period in ms.
static fl::i32 angleAt(unsigned long t, unsigned long periodMs) {
    return (fl::i32)(((fl::i64)(t % periodMs) * kTwoPi) / (fl::i64)periodMs);
}

// Wrap an angle into [0, 2*pi): the s16x16 trig wants a non-negative input.
static Fix wrap(fl::i32 a) {
    a %= kTwoPi;
    if (a < 0) a += kTwoPi;
    return Fix::from_raw(a);
}

// Point at (cx, cy) + r * (cos a, sin a); every value is Q16.16.
static FixPt polar(fl::i32 cx, fl::i32 cy, fl::i32 r, fl::i32 a) {
    Fix s, c;
    Fix::sincos(wrap(a), s, c);
    FixPt p = {Fix::from_raw(cx + mulQ16(c.raw(), r)),
               Fix::from_raw(cy + mulQ16(s.raw(), r))};
    return p;
}

// ---- 1. Pinwheel: s16x16, ABuffer, additive ----------------------------------
// DRAW_MODE_BLEND (the default) adds the antialiased color to what is
// already there, saturating at 255, so overlapping blades brighten toward
// white. A blade is a triangle, so MAXN = 3 is the smallest stack frame.
static void scenePinwheel(unsigned long t) {
    const fl::i32 spin = angleAt(t, 7000);
    const fl::i32 len = mulQ16(kR, 62259L);    // blade tip at 0.95 R
    const fl::i32 back = mulQ16(len, 36045L);  // trailing corner at 0.55 len
    const fl::u8 hue = (fl::u8)(t / 40);
    for (int k = 0; k < 6; ++k) {
        const fl::i32 a = spin + k * (kTwoPi / 6);
        const FixPt blade[3] = {
            {Fix::from_raw(kCX), Fix::from_raw(kCY)},
            polar(kCX, kCY, len, a),
            polar(kCX, kCY, back, a + 58982L),  // swept 0.9 rad back
        };
        canvas.drawPolygon<Fix, 3, FL_GFX_POLYGON_DEFAULT_CHUNK,
                           fl::gfx::PolyFill::ABuffer>(
            CHSV((fl::u8)(hue + 42 * k), 200, 240), blade,
            fl::DrawMode::DRAW_MODE_BLEND);
    }
}

// ---- 2. Star: float, Exact (default), even-odd -------------------------------
// Joining every second corner of a pentagon gives a pentagram whose edges
// cross. Under the even-odd rule a point is inside when a ray from it
// crosses an odd number of edges; the center pentagon crosses an even
// number, so it stays empty. A small counter-rotating star sits in it.
static void drawStar(float cx, float cy, float r, float spin, const CRGB& color) {
    fl::PolyPoint<float> star[5];
    for (int i = 0; i < 5; ++i) {
        const float a = spin + (float)i * (2.0f * 6.2831853f / 5.0f);
        star[i].x = cx + r * fl::cosf(a);
        star[i].y = cy + r * fl::sinf(a);
    }
    canvas.drawPolygon(color, star);  // Exact fill, default MAXN, additive
}

static void sceneStar(unsigned long t) {
    const float cx = WIDTH * 0.5f, cy = HEIGHT * 0.5f;
    const float r = (WIDTH < HEIGHT ? WIDTH : HEIGHT) * 0.5f - 0.25f;
    const float spin = (float)(t % 9000) * (6.2831853f / 9000.0f);
    drawStar(cx, cy, r, spin, CRGB(255, 140, 0));
    drawStar(cx, cy, r * 0.28f, -2.0f * spin, CRGB(0, 120, 255));
}

// ---- 3. Layers: DRAW_MODE_OVERWRITE -------------------------------------------
// OVERWRITE replaces each touched pixel with color x coverage instead of
// adding to it, so shapes drawn later hide the ones below (draw back to
// front). Edge pixels keep only the top shape's partial coverage, which is
// the thin dark seam around each square; BLEND would mix the colors instead.
static void sceneLayers(unsigned long t) {
    static const CRGB kColors[3] = {CRGB(255, 0, 80), CRGB(255, 200, 0), CRGB(0, 160, 255)};
    for (int layer = 0; layer < 3; ++layer) {
        const fl::i32 r = kR - layer * (kR / 4);  // corner radius R, 3/4 R, 1/2 R
        fl::i32 a = angleAt(t, 9000UL - 2000UL * layer);
        if (layer & 1) a = -a;  // the middle square turns the other way
        FixPt square[4];
        for (int j = 0; j < 4; ++j) square[j] = polar(kCX, kCY, r, a + j * (kTwoPi / 4));
        canvas.drawPolygon(kColors[layer], square, fl::DrawMode::DRAW_MODE_OVERWRITE);
    }
}

// ---- 4. Knot: more vertices than the default MAXN ----------------------------
// A closed 2:3 Lissajous curve sampled at 14 points crosses itself several
// times; even-odd fills alternate lobes. 14 vertices exceed the 8-vertex
// default on small-stack targets, so MAXN is given explicitly (sized exactly, to
// keep the stack frame small). The ABuffer backend has the shallower stack.
static const int kKnotN = 14;

static void sceneKnot(unsigned long t) {
    const fl::i32 ph1 = angleAt(t, 11000);
    const fl::i32 ph2 = angleAt(t, 8700);
    const fl::i32 rx = mulQ16(kCX, 60293L);  // 0.92 of the half-width
    const fl::i32 ry = mulQ16(kCY, 60293L);
    for (int pass = 0; pass < 2; ++pass) {  // second knot: x phase + pi
        FixPt knot[kKnotN];
        for (int i = 0; i < kKnotN; ++i) {
            const fl::i32 th = (fl::i32)((fl::i64)i * kTwoPi / kKnotN);
            const fl::i32 sx = Fix::sin(wrap(2 * th + ph1 + pass * kPi)).raw();
            const fl::i32 sy = Fix::sin(wrap(3 * th + ph2)).raw();
            knot[i].x = Fix::from_raw(kCX + mulQ16(rx, sx));
            knot[i].y = Fix::from_raw(kCY + mulQ16(ry, sy));
        }
        canvas.drawPolygon<Fix, kKnotN, FL_GFX_POLYGON_DEFAULT_CHUNK,
                           fl::gfx::PolyFill::ABuffer>(
            CHSV((fl::u8)(160 + 90 * pass), 190, 255), knot,
            fl::DrawMode::DRAW_MODE_BLEND);
    }
}

// ---- frame driver --------------------------------------------------------------

typedef void (*SceneFn)(unsigned long t);
static const SceneFn kScenes[] = {scenePinwheel, sceneStar, sceneLayers, sceneKnot};
static const int kNumScenes = (int)(sizeof(kScenes) / sizeof(kScenes[0]));
static const unsigned long kSceneMs = 5000;

void setup() {
    FastLED.addLeds<NEOPIXEL, PIN_DATA>(leds, NUM_LEDS);
    FastLED.setBrightness(64);
}

void loop() {
    const unsigned long t = millis();
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    kScenes[(t / kSceneMs) % kNumScenes](t);
    FastLED.show();
}
