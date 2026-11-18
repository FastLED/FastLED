/// @file    PolyShow.ino
/// @brief   Animated antialiased drawPolygon showcase on a 20x20 canvas
/// @example PolyShow.ino
// FL_AGENT_ALLOW_NEW_EXAMPLE
//
// The canvas is a fl::CanvasRGB view directly over the LED array (no copy);
// wire a 20x20 matrix (400 WS2812 in row-major order) to PIN_DATA. Cycles
// six animated scenes, each exercising a different drawPolygon edge case:
//   1. Bowtie     - self-intersecting quad, even-odd fill, rotating
//   2. Subpixel   - fractional-pixel vertices, antialiased edge ramps, rotating
//   3. Clamp      - large triangle sliding fully off-canvas left to right
//   4. Sliver     - near-horizontal edges (dy < 1 px) spanning many x columns,
//                   spinning
//   5. Degenerate - spinning 2-vertex segment + horizontal-only polygon
//                   (must draw nothing over a dim base)
//   6. Diamond    - vertices exactly on row centers (y = k + 0.5 px),
//                   breathing scale
// With POLYSHOW_SERIAL=1 (off by default) it also prints a 20x20
// brightness preview (2 hex digits per pixel) over Serial at each scene
// change.
//
// Integer-only: s16x16 Q16.16 coordinates, no floats. The drawPolygon tier
// (MAXN=4, CHUNK=1) keeps the working set small, but the 400-LED x 3-byte
// array plus statics and stack needs ~2.1 KB SRAM, so sub-4 KB boards are
// excluded (@filter).
// Budget measured on the built Arduino Mega 2560 firmware with the serial
// output off (avr-gcc 7.3 -Os; runtime depths from a cycle-accurate AVR
// trace of the ELF covering all six scenes):
//
//   flash:        23,480 B / 248 KB (9.2%)
//   static RAM:   1,384 B (.data 36 + .bss 1,348, ends 0x800768)
//   max stack:    740 B (the drawPolygon<,4,1> -> 64-bit libgcc chain is
//                 728 B; the extra 12 B is a timer0 ISR landing inside a
//                 64-bit division)
//   worst-case RAM: 2,124 B of 8 KB (26%)
//   frame rate:   ~5 FPS (bowtie, densest fill) to ~75 FPS (the
//                 degenerate no-op scene)
// POLYSHOW_SERIAL=1 adds 1,734 B of flash and 421 B of static RAM (the
// HardwareSerial code and its ring buffers), pushing the worst case to
// 2,558 B.

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
// The 1,200 B LED/canvas buffer plus ~1.4 KB of statics and ~740 B of
// stack need ~2.1 KB SRAM worst case, so the 1 KB (atmega8a) and 2 KB
// (nano, uno, ...) boards are excluded. The 2.5 KB ATmega32u4 "leonardo"
// would fit the default build with ~430 B to spare, but is excluded too:
// the margin is thin, and the POLYSHOW_SERIAL=1 build (2,558 B) does not
// fit it at all.

#include <Arduino.h>
#include <FastLED.h>
#include <fl/gfx/gfx.h>

// Host (non-AVR) builds of this example have no flash; keep the macro
// meaningful on AVR and inert elsewhere.
#ifndef PROGMEM
#define PROGMEM
#endif

#define NUM_LEDS (20 * 20)
#ifndef PIN_DATA
#define PIN_DATA 3
#endif

// Serial output: the startup banner plus the 20x20 brightness preview at
// every scene change. Off by default — the LED show itself needs no
// Serial, and compiling it out removes the HardwareSerial code path and
// its ring buffers. Build with -DPOLYSHOW_SERIAL=1 to re-enable.
#ifndef POLYSHOW_SERIAL
#define POLYSHOW_SERIAL 0
#endif

CRGB leds[NUM_LEDS];

static const int W = 20;
static const int H = 20;

// drawPolygon tier: at most 4 vertices (every scene below has <= 4) and a
// 1-px column chunk.
static const int TIER_MAXN = 4;
static const int TIER_CHUNK = 1;

// The canvas is a view over the LED array itself — no copy anywhere.
fl::CanvasRGB canvas(fl::span<CRGB>(leds, NUM_LEDS), W, H);

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

// Relative to the centroid; the centroid itself slides (see scene 2).
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

// ---- staging (flash -> RAM) -------------------------------------------------

static PP sStage[TIER_MAXN];
static PP* stage(const PP* flash, int n) {
#if defined(__AVR__)
    // s16x16 is a single i32, so each point is two flash dwords.
    for (int i = 0; i < n; ++i) {
        sStage[i].x = fl::s16x16::from_raw(pgm_read_dword(&flash[i].x));
        sStage[i].y = fl::s16x16::from_raw(pgm_read_dword(&flash[i].y));
    }
#else
    for (int i = 0; i < n; ++i) sStage[i] = flash[i];
#endif
    return sStage;
}

static void drawStaged(const CRGB& color, const PP* pts, int n) {
    canvas.drawPolygon<fl::s16x16, TIER_MAXN, TIER_CHUNK>(
        color, fl::span<const PP>(pts, n), fl::DrawMode::DRAW_MODE_OVERWRITE);
}

// ---- animation (integer-only, Q16.16) ---------------------------------------
// The canvas center in Q16.16.

static const fl::i32 kCX = 10 * 65536L;
static const fl::i32 kCY = 10 * 65536L;

// Rotate points in place about the canvas center by a fixed angle given in
// Q16.16. i64 products: |dx| < 256 px * 65536 and cos/sin < 65536, so the
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
// at roughly 5-22 FPS depending on the scene; see the budget note above).
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

#if POLYSHOW_SERIAL
static const char* sceneName(int scene) {
    switch (scene) {
        case 0: return "bowtie (rotating, even-odd)";
        case 1: return "subpixel (rotating, fractional vertices)";
        case 2: return "clamp (sliding off-canvas triangle)";
        case 3: return "sliver (spinning, dy < 1 px edges)";
        case 4: return "degenerate (spinning segment: no-op)";
        default: return "diamond (breathing scale, row-center)";
    }
}

static void printHex2(int v) {
    static const char kHex[] = "0123456789ABCDEF";
    char c[3];
    c[0] = kHex[v >> 4];
    c[1] = kHex[v & 15];
    c[2] = 0;
    Serial.print(c);
}

static void printPreview(int scene) {
    Serial.print(F("scene: "));
    Serial.println(sceneName(scene));
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x)
            printHex2(leds[y * W + x].r);
        Serial.println();
    }
    Serial.println(F("---"));
}
#endif

static int lastScene = -1;

void setup() {
#if POLYSHOW_SERIAL
    Serial.begin(115200);
#endif
    FastLED.addLeds<NEOPIXEL, PIN_DATA>(leds, NUM_LEDS);
    FastLED.setBrightness(255);
#if POLYSHOW_SERIAL
    delay(500);  // let a serial monitor attach before the show starts
    Serial.println(F("PolyShow: animated drawPolygon edge cases, 20x20"));
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
#if POLYSHOW_SERIAL
    if (newScene) printPreview(scene);
#endif
    FastLED.show();
}
