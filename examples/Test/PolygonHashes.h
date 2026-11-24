#pragma once

// AVR runtime regression check for 16-bit-int bugs in drawPolygon (#5324).
//
// Host builds use a 32-bit int, so they cannot see arithmetic that overflows
// or narrows only where int is 16 bits (#5301, #5303, #5292). Test.ino renders
// these scenes on the AVR emulator (avr8js, `bash test --run uno Test`) and
// compares each FNV-1a canvas hash to the golden table below. The host unit
// test in tests/unit/fl/gfx/draw_polygon.hpp renders the same scenes and pins the
// same table, so a match on AVR means "AVR renders identically to the host".
//
// The scenes target the paths that broke on AVR: edges near the +/-8191 px
// clamp (ABuffer ytop - 4*py, Exact column extents), near-horizontal huge
// edges, self-intersection, sub-pixel slivers and blend math. Vertices are
// exact Q16.16 (s16x16) so no float conversion differs between targets, and
// the kernel tier is pinned (MAXN 8, CHUNK 16) to the AVR default. A 16x8
// canvas keeps the Exact kernel's call chain inside an Uno's 2 KB SRAM.
//
// To change a scene: edit it, run `bash test gfx` (the AVR scene case
// prints the new hashes), update the table, then `bash test --run uno Test`.

#include "FastLED.h"
#include "fl/gfx/gfx.h"
#include "fl/math/fixed_point/s16x16.h"

namespace polygon_hashes {

enum : int { kWidth = 16, kHeight = 8, kMaxVerts = 8, kSceneCount = 8 };

// Q16.16 literal helpers (pixel units).
#define POLY_HASH_PX(v) static_cast<fl::i32>((v) * 65536L)
#define POLY_HASH_FAR 536805376L  // 8191 px, the kernels' coordinate clamp

struct Scene {
    fl::i32 x[kMaxVerts];
    fl::i32 y[kMaxVerts];
    fl::u32 n;
};

// Kept in flash: the table is larger than an Uno can spare in SRAM.
const Scene kScenes[kSceneCount] FL_PROGMEM = {
    // 0: fractional triangle
    {{POLY_HASH_PX(1.25), POLY_HASH_PX(14.75), POLY_HASH_PX(4.5)},
     {POLY_HASH_PX(0.5), POLY_HASH_PX(2.25), POLY_HASH_PX(7.75)}, 3},
    // 1: pentagon with irregular sub-pixel vertices
    {{POLY_HASH_PX(8.0), POLY_HASH_PX(14.625), POLY_HASH_PX(12.125),
      POLY_HASH_PX(3.875), POLY_HASH_PX(1.375)},
     {POLY_HASH_PX(0.3125), POLY_HASH_PX(3.0625), POLY_HASH_PX(7.625),
      POLY_HASH_PX(7.5), POLY_HASH_PX(3.125)}, 5},
    // 2: self-intersecting bowtie (even-odd span pairing)
    {{POLY_HASH_PX(1.0), POLY_HASH_PX(15.0), POLY_HASH_PX(15.0),
      POLY_HASH_PX(1.0)},
     {POLY_HASH_PX(1.0), POLY_HASH_PX(7.0), POLY_HASH_PX(1.0),
      POLY_HASH_PX(7.0)}, 4},
    // 3: near-horizontal edge spanning +/-8191 px (Exact column extents
    //    narrowed to a 16-bit int, #5301)
    {{-POLY_HASH_FAR, POLY_HASH_FAR, POLY_HASH_PX(8.0)},
     {POLY_HASH_PX(2.0), POLY_HASH_PX(2.0) + 132, POLY_HASH_PX(7.5)}, 3},
    // 4: edges starting near the -8191 px clamp above the canvas (ABuffer
    //    ytop - 4*py overflowed a 16-bit int, #5301)
    {{POLY_HASH_PX(3.0), POLY_HASH_PX(13.5), POLY_HASH_PX(1.0)},
     {-POLY_HASH_FAR, POLY_HASH_PX(6.5), POLY_HASH_PX(7.0)}, 3},
    // 5: concave 8-point star (fills MAXN)
    {{POLY_HASH_PX(8.0), POLY_HASH_PX(9.5), POLY_HASH_PX(15.0),
      POLY_HASH_PX(10.0), POLY_HASH_PX(12.5), POLY_HASH_PX(8.0),
      POLY_HASH_PX(3.5), POLY_HASH_PX(6.0)},
     {POLY_HASH_PX(0.25), POLY_HASH_PX(2.75), POLY_HASH_PX(3.0),
      POLY_HASH_PX(4.5), POLY_HASH_PX(7.75), POLY_HASH_PX(5.5),
      POLY_HASH_PX(7.75), POLY_HASH_PX(4.5)}, 8},
    // 6: clamp-sized quad covering the whole canvas
    {{-POLY_HASH_FAR, POLY_HASH_FAR, POLY_HASH_FAR, -POLY_HASH_FAR},
     {-POLY_HASH_FAR, -POLY_HASH_FAR, POLY_HASH_FAR, POLY_HASH_FAR}, 4},
    // 7: sub-pixel sliver crossing the full width
    {{POLY_HASH_PX(0.125), POLY_HASH_PX(15.875), POLY_HASH_PX(15.875)},
     {POLY_HASH_PX(3.875), POLY_HASH_PX(4.0625), POLY_HASH_PX(4.125)}, 3},
};

#undef POLY_HASH_PX
#undef POLY_HASH_FAR

// Golden hashes: {Exact, ABuffer} per scene. Host output, pinned by
// tests/unit/fl/gfx/draw_polygon.hpp.
const fl::u32 kGolden[kSceneCount][2] FL_PROGMEM = {
    {0xA8F35217UL, 0x20F530D5UL},  // 0
    {0x9ADBBC51UL, 0xC5631142UL},  // 1
    {0xE4055809UL, 0xCF52A925UL},  // 2
    {0x4369434FUL, 0x2E945579UL},  // 3
    {0xD5167763UL, 0x7E3AAB3FUL},  // 4
    {0xD344BF25UL, 0x2AB93628UL},  // 5
    {0x3D73C605UL, 0x3D73C605UL},  // 6
    {0xB2B131CAUL, 0x5C4D9FC2UL},  // 7
};

inline fl::u32 goldenHash(int scene, bool abuffer) {
    return FL_PGM_READ_DWORD_NEAR(&kGolden[scene][abuffer ? 1 : 0]);
}

// Renders scene `scene` with the selected kernel over a gradient background
// (BLEND mode, so the blend math is covered too) and returns the FNV-1a hash
// of the canvas bytes. `buf` holds kWidth * kHeight pixels.
inline fl::u32 renderHash(int scene, bool abuffer, CRGB* buf) {
    for (int y = 0; y < kHeight; ++y)
        for (int x = 0; x < kWidth; ++x)
            buf[y * kWidth + x] = CRGB(static_cast<fl::u8>(x * 16),
                                       static_cast<fl::u8>(y * 32), 40);

    fl::PolyPoint<fl::s16x16> pts[kMaxVerts];
    const Scene* s = &kScenes[scene];
    const fl::u32 n = FL_PGM_READ_DWORD_NEAR(&s->n);
    for (fl::u32 i = 0; i < n; ++i) {
        pts[i].x = fl::s16x16::from_raw(
            static_cast<fl::i32>(FL_PGM_READ_DWORD_NEAR(&s->x[i])));
        pts[i].y = fl::s16x16::from_raw(
            static_cast<fl::i32>(FL_PGM_READ_DWORD_NEAR(&s->y[i])));
    }

    fl::CanvasRGB canvas(fl::span<CRGB>(buf, kWidth * kHeight), kWidth, kHeight);
    const fl::span<const fl::PolyPoint<fl::s16x16>> span(pts, n);
    const CRGB color(255, 128, 7);
    if (abuffer) {
        canvas.drawPolygon<fl::s16x16, kMaxVerts, 16, fl::gfx::PolyFill::ABuffer>(
            color, span, fl::DrawMode::DRAW_MODE_BLEND);
    } else {
        canvas.drawPolygon<fl::s16x16, kMaxVerts, 16, fl::gfx::PolyFill::Exact>(
            color, span, fl::DrawMode::DRAW_MODE_BLEND);
    }

    fl::u32 h = 2166136261UL;  // FNV-1a
    for (int i = 0; i < kWidth * kHeight; ++i) {
        const fl::u8 bytes[3] = {buf[i].r, buf[i].g, buf[i].b};
        for (int b = 0; b < 3; ++b) {
            h ^= bytes[b];
            h *= 16777619UL;
        }
    }
    return h;
}

}  // namespace polygon_hashes
