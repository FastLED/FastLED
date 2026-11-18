/// @file    PolyKernels.ino
/// @brief   Side-by-side benchmark of the drawPolygon fill kernels
/// @example PolyKernels.ino
// FL_AGENT_ALLOW_NEW_EXAMPLE
//
// Same animated scenes as the PolyShow example (bowtie, subpixel, clamp,
// sliver, degenerate, diamond), rendered with a selectable fill backend:
//
//   -DPOLY_KERNEL=0  Exact   (per-pixel exact area integration, the
//                         default backend)
//   -DPOLY_KERNEL=1  ABuffer (8x4 A-buffer antialiasing, default here)
//   -DPOLY_KERNEL=2  TinyGL-ref (faithful port of aykevl/tinygl's polygon
//                         fill; test/benchmark reference, see
//                         tests/fl/gfx/poly_tinygl_ref.h)
//
// The canvas is a fl::CanvasRGB view directly over the LED array (no copy).
// The default canvas is 20x20 (400 WS2812 in row-major order on PIN_DATA);
// the scenes are defined in 20x20 space and scaled to any PW x PH canvas at
// staging time, e.g. a 16x8 matrix on an ATmega328P:
//
//   -DPW=16 -DPH=8
//
// With POLYKERNEL_SERIAL=1 (off by default) it prints a startup banner.
//
// Integer-only: s16x16 Q16.16 coordinates, no floats.
//
// Kernel 2 (TinyGL-ref) is a benchmark reference, not a product backend:
// its static edge order mis-renders self-intersecting polygons once the
// edges' x-order flips (the bowtie scene shows the far petal mis-filled),
// and its i32 slope math wraps on very steep sub-pixel edges. Use it to
// compare against, not to ship.
//
// poly_tinygl_ref.h (next to this sketch) is a byte-identical copy of
// tests/fl/gfx/poly_tinygl_ref.h: the AVR (fbuild) build only stages the
// library's src/ tree plus the sketch, so it cannot reach tests/. A meson
// test (tests/meson.build) fails the build if the two copies drift.
//
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
// The 20x20 build uses ~1.4 KB of statics (dominated by the 1,200 B LED
// buffer) plus up to ~740 B of stack (Exact kernel; the default ABuffer
// kernel uses ~408 B), i.e. ~1.8 KB worst case for the default build and
// ~2.1 KB for -DPOLY_KERNEL=0 (measured on the ATmega2560 firmware), so
// the 1 KB (atmega8a) and 2 KB (nano, uno, ...) boards are excluded: the
// 2 KB boards have only ~256 B of margin on the default build and no
// margin at all for Exact. The 2.5 KB "leonardo" fits the default build
// with thin margin (and the POLYKERNEL_SERIAL=1 build does not fit at
// all). Smaller canvases (e.g. -DPW=16 -DPH=8, 384 B LED buffer, ~560 B
// of statics plus up to ~230 B of stack; measured on the ATmega328P
// firmware) fit the 2 KB boards; use that define set there.

#include <Arduino.h>
#include <FastLED.h>
#include <fl/gfx/gfx.h>
#if defined(POLY_KERNEL) && POLY_KERNEL == 2
#include "poly_tinygl_ref.h"  // copy of tests/fl/gfx/poly_tinygl_ref.h
#endif

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

// Fill backend: 0 = Exact, 1 = ABuffer (default), 2 = TinyGL-ref.
#ifndef POLY_KERNEL
#define POLY_KERNEL 1
#endif
#if POLY_KERNEL != 0 && POLY_KERNEL != 1 && POLY_KERNEL != 2
#error "POLY_KERNEL must be 0 (Exact), 1 (ABuffer) or 2 (TinyGL-ref)"
#endif

#define NUM_LEDS (PW * PH)
#ifndef PIN_DATA
#define PIN_DATA 3
#endif

// Serial output: a startup banner only. Off by default — the LED show
// itself needs no Serial, and compiling it out removes the HardwareSerial
// code path and its ring buffers. Build with -DPOLYKERNEL_SERIAL=1.
#ifndef POLYKERNEL_SERIAL
#define POLYKERNEL_SERIAL 0
#endif

CRGB leds[NUM_LEDS];

static const int W = PW;
static const int H = PH;

// drawPolygon tiers: at most 4 vertices (every scene below has <= 4).
// CHUNK applies to the Exact backend; ABuffer ignores it.
static const int TIER_MAXN = 4;
static const int TIER_CHUNK = 1;

// The canvas is a view over the LED array itself — no copy anywhere.
// Kernels 0 and 1 use the user-facing fl::CanvasRGB; kernel 2 (the ref
// port) takes the base gfx::Canvas, so give it its own view over the same
// pixels (a span is 2 words; no data moves).
fl::CanvasRGB canvas(fl::span<CRGB>(leds, NUM_LEDS), W, H);
#if POLY_KERNEL == 2
fl::gfx::Canvas<CRGB> refCanvas(fl::span<CRGB>(leds, NUM_LEDS), W, H);
#endif

typedef fl::PolyPoint<fl::s16x16> PP;

// All coordinates are s16x16 (Q16.16) — the from_raw values are exact:
// pixel p + fraction f  ->  (p + f) * 65536 rounded.
// The tables sit in flash (PROGMEM); on AVR they are staged to sStage
// before use so the span handed to drawPolygon stays a plain RAM pointer
// (and the animation transforms have a mutable copy).
static const PP kBowtie[4] PROGMEM = {
    // (2.5,6) (17.5,14) (2.5,14) (17.5,6): edges AB and CD cross mid-canvas.
    { fl::s16x16::from_raw(163840L),  fl::s16x16::from_raw(393216L)  },
    { fl::s16x16::from_raw(1146880L), fl::s16x16::from_raw(917504L)  },
    { fl::s16x16::from_raw(163840L),  fl::s16x16::from_raw(917504L)  },
    { fl::s16x16::from_raw(1146880L), fl::s16x16::from_raw(393216L)  },
};

static const PP kSubpixel[3] PROGMEM = {
    // (3.3, 16.7) (9.8, 3.2) (16.4, 12.9) — no vertex on a pixel center.
    { fl::s16x16::from_raw(216269L),  fl::s16x16::from_raw(1094451L) },
    { fl::s16x16::from_raw(642253L),  fl::s16x16::from_raw(209715L)  },
    { fl::s16x16::from_raw(1074790L), fl::s16x16::from_raw(845414L)  },
};

// Relative to the 20x20-space center; the centroid itself slides.
static const PP kClamp[3] PROGMEM = {
    // (-75,-75) (75,0) (0,75) — far off-canvas at rest; the clipped core
    // with antialiased edges sweeps across the whole canvas.
    { fl::s16x16::from_raw(-4915200L), fl::s16x16::from_raw(-4915200L) },
    { fl::s16x16::from_raw(4915200L),  fl::s16x16::from_raw(0L)        },
    { fl::s16x16::from_raw(0L),        fl::s16x16::from_raw(4915200L)  },
};

static const PP kSliver[4] PROGMEM = {
    // (1.2, 9.7) (18.4, 10.3) (18.4, 11.0) (1.2, 10.4) — the top and
    // bottom edges each change y by < 1 px while spanning 17 px of x.
    { fl::s16x16::from_raw(78643L),   fl::s16x16::from_raw(635699L) },
    { fl::s16x16::from_raw(1205862L), fl::s16x16::from_raw(675021L) },
    { fl::s16x16::from_raw(1205862L), fl::s16x16::from_raw(720896L) },
    { fl::s16x16::from_raw(78643L),   fl::s16x16::from_raw(681574L) },
};

static const PP kDegenerate[2] PROGMEM = {
    // 2 vertices: a line segment, must fill nothing.
    { fl::s16x16::from_raw(163840L),  fl::s16x16::from_raw(163840L)  },
    { fl::s16x16::from_raw(1146880L), fl::s16x16::from_raw(819200L)  },
};

static const PP kDegenerateH[3] PROGMEM = {
    // (2.5,10) (10,10) (17.5,10): horizontal-only, must fill nothing.
    { fl::s16x16::from_raw(163840L),  fl::s16x16::from_raw(655360L) },
    { fl::s16x16::from_raw(655360L),  fl::s16x16::from_raw(655360L) },
    { fl::s16x16::from_raw(1146880L), fl::s16x16::from_raw(655360L) },
};

static const PP kDiamond[4] PROGMEM = {
    // (10, 0.5) (18.25, 10) (10, 19.5) (1.75, 10): every vertex sits
    // exactly on a row center (y = k + 0.5 px) — the row-boundary
    // convention case.
    { fl::s16x16::from_raw(655360L),  fl::s16x16::from_raw(32768L)   },
    { fl::s16x16::from_raw(1196032L), fl::s16x16::from_raw(655360L)  },
    { fl::s16x16::from_raw(655360L),  fl::s16x16::from_raw(1277952L) },
    { fl::s16x16::from_raw(114688L),  fl::s16x16::from_raw(655360L)  },
};

// ---- staging (flash -> RAM, scaled to the canvas) ---------------------------
// The scene tables are expressed in 20x20 space. stage() maps them onto the
// actual (W, H) canvas about the canvas center — an identity map at the
// default 20x20, e.g. (2.5, 6) becomes (2, 3.2) on a 16x8 canvas.
// i64 products: |dx| < 100 px * 65536 and scale < 65536, terms < 2^45.
static const fl::i32 kBaseC = 10 * 65536L;                 // 20x20-space center
static const fl::i32 kCX = (fl::i32)((fl::i64)W * 65536L / 2);
static const fl::i32 kCY = (fl::i32)((fl::i64)H * 65536L / 2);
static const fl::i32 kSX = (fl::i32)((fl::i64)W * 65536L / 20);
static const fl::i32 kSY = (fl::i32)((fl::i64)H * 65536L / 20);

static PP sStage[TIER_MAXN];
static PP* stage(const PP* flash, int n) {
    for (int i = 0; i < n; ++i) {
        fl::i32 rx, ry;
#if defined(__AVR__)
        // s16x16 is a single i32, so each point is two flash dwords.
        rx = pgm_read_dword(&flash[i].x);
        ry = pgm_read_dword(&flash[i].y);
#else
        rx = flash[i].x.raw();
        ry = flash[i].y.raw();
#endif
        sStage[i].x = fl::s16x16::from_raw(
            kCX + (fl::i32)(((fl::i64)(rx - kBaseC) * kSX) >> 16));
        sStage[i].y = fl::s16x16::from_raw(
            kCY + (fl::i32)(((fl::i64)(ry - kBaseC) * kSY) >> 16));
    }
    return sStage;
}

static void drawStaged(const CRGB& color, const PP* pts, int n) {
#if POLY_KERNEL == 0
    canvas.drawPolygon<fl::s16x16, TIER_MAXN, TIER_CHUNK,
                       fl::gfx::PolyFill::Exact>(
        color, fl::span<const PP>(pts, n), fl::DrawMode::DRAW_MODE_OVERWRITE);
#elif POLY_KERNEL == 1
    canvas.drawPolygon<fl::s16x16, TIER_MAXN, TIER_CHUNK,
                       fl::gfx::PolyFill::ABuffer>(
        color, fl::span<const PP>(pts, n), fl::DrawMode::DRAW_MODE_OVERWRITE);
#else
    fl::gfx::detail::drawPolygonTinyGLRef<CRGB, fl::s16x16, true, TIER_MAXN>(
        refCanvas, color, fl::span<const PP>(pts, n));
#endif
}

// ---- animation (integer-only, Q16.16) ---------------------------------------

// Rotate points in place about the canvas center by a fixed angle given in
// Q16.16. i64 products: |dx| < 128 px * 65536 and cos/sin < 65536, so the
// terms stay < 2^44.
static void rotPts(PP* p, int n, fl::i32 cosD, fl::i32 sinD) {
    for (int i = 0; i < n; ++i) {
        const fl::i32 dx = p[i].x.raw() - kCX;
        const fl::i32 dy = p[i].y.raw() - kCY;
        const fl::i64 rx = (fl::i64)dx * cosD - (fl::i64)dy * sinD;
        const fl::i64 ry = (fl::i64)dx * sinD + (fl::i64)dy * cosD;
        p[i].x = fl::s16x16::from_raw(kCX + (fl::i32)(rx >> 16));
        p[i].y = fl::s16x16::from_raw(kCY + (fl::i32)(ry >> 16));
    }
}

// Scale points in place about the canvas center by scale16 (Q16.16).
static void scalePts(PP* p, int n, fl::i32 scale16) {
    for (int i = 0; i < n; ++i) {
        const fl::i32 dx = p[i].x.raw() - kCX;
        const fl::i32 dy = p[i].y.raw() - kCY;
        p[i].x = fl::s16x16::from_raw(kCX + (fl::i32)(((fl::i64)dx * scale16) >> 16));
        p[i].y = fl::s16x16::from_raw(kCY + (fl::i32)(((fl::i64)dy * scale16) >> 16));
    }
}

// Translate points in place by (dx, dy) in Q16.16.
static void transPts(PP* p, int n, fl::i32 dx, fl::i32 dy) {
    for (int i = 0; i < n; ++i) {
        p[i].x = fl::s16x16::from_raw(p[i].x.raw() + dx);
        p[i].y = fl::s16x16::from_raw(p[i].y.raw() + dy);
    }
}

// Advance the spin in fixed 20 ms sub-steps so the angular speed is
// independent of the render frame rate (on a 2560 the full canvas frames
// at roughly 5-22 FPS depending on the scene and kernel).
static unsigned long sRotAcc = 0;
static void spin(PP* p, int n, unsigned long t, fl::i32 cosD, fl::i32 sinD) {
    while (sRotAcc < t) {
        rotPts(p, n, cosD, sinD);
        sRotAcc += 20;
    }
}

// ---- scenes ------------------------------------------------------------------

static const unsigned long kSceneMs = 4000;

static void renderScene(int scene, unsigned long t) {
    if (scene == 4) {
        // Dim warm base: the degenerate polygons must leave it unchanged.
        for (int i = 0; i < NUM_LEDS; ++i) leds[i] = CRGB(24, 12, 4);
    } else {
        // Black = zeroed bytes (CRGB is a plain 3-byte struct).
        memset(leds, 0, sizeof leds);
    }
    switch (scene) {
        case 0: {  // bowtie: 4 deg per 20 ms about the center
            PP* p = stage(kBowtie, 4);
            spin(p, 4, t, 65376L, 4572L);
            drawStaged(CRGB::Red, p, 4);
            break;
        }
        case 1: {  // subpixel: 2 deg per 20 ms
            PP* p = stage(kSubpixel, 3);
            spin(p, 3, t, 65496L, 2287L);
            drawStaged(CRGB::Green, p, 3);
            break;
        }
        case 2: {  // clamp: centroid slides x = -80 .. +100 px across the scene
            PP* p = stage(kClamp, 3);
            const fl::i32 tms = (fl::i32)(t % kSceneMs);
            const fl::i32 cx = ((-80 + 180 * tms / (fl::i32)kSceneMs) << 16);
            transPts(p, 3, cx, kCY);
            drawStaged(CRGB::Blue, p, 3);
            break;
        }
        case 3: {  // sliver: 8 deg per 20 ms
            PP* p = stage(kSliver, 4);
            spin(p, 4, t, 64898L, 9121L);
            drawStaged(CRGB::Cyan, p, 4);
            break;
        }
        case 4: {  // degenerate: spinning 2-pt segment + static horizontal
            PP* p = stage(kDegenerate, 2);
            spin(p, 2, t, 65287L, 5712L);
            drawStaged(CRGB::White, p, 2);
            drawStaged(CRGB::White, stage(kDegenerateH, 3), 3);
            break;
        }
        default: {  // diamond: breathing scale 0.45 .. 1.0, period 2 s
            PP* p = stage(kDiamond, 4);
            const unsigned long w = t % 2000;
            const fl::i32 f = (w < 1000) ? (fl::i32)w : (fl::i32)(2000 - w);
            const fl::i32 scale16 = 29491L + 36045L * f / 1000L;
            scalePts(p, 4, scale16);
            drawStaged(CRGB::Magenta, p, 4);
            break;
        }
    }
}

#if POLYKERNEL_SERIAL
static const char* kernelName() {
    if (POLY_KERNEL == 0) return "Exact";
    if (POLY_KERNEL == 1) return "ABuffer";
    return "TinyGL-ref";
}
#endif

static int lastScene = -1;

void setup() {
#if POLYKERNEL_SERIAL
    Serial.begin(115200);
#endif
    FastLED.addLeds<NEOPIXEL, PIN_DATA>(leds, NUM_LEDS);
    FastLED.setBrightness(255);
#if POLYKERNEL_SERIAL
    delay(500);  // let a serial monitor attach before the show starts
    Serial.print(F("PolyKernels: kernel="));
    Serial.print(kernelName());
    Serial.print(F(" canvas="));
    Serial.print(PW);
    Serial.print('x');
    Serial.println(PH);
#endif
}

void loop() {
    const unsigned long t = millis();
    const int scene = (int)((t / kSceneMs) % 6);
    const bool newScene = (scene != lastScene);
    if (newScene) {
        lastScene = scene;
        sRotAcc = 0;
    }
    renderScene(scene, t % kSceneMs);
    FastLED.show();
}
