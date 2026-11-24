#pragma once

// AVR runtime regression check for 16-bit-int arithmetic in fill_noise16
// (#5330 / PR #5341).
//
// Host builds use a 32-bit int, so they cannot see arithmetic that overflows
// or narrows only where int is 16 bits. Two checks run on the AVR emulator
// (avr8js, `bash test --run uno Test`):
//
// 1. Widened accumulator. Before #5341, fill_raw_noise16into8 added
//    `pData[i] << 8` as an int; on AVR that overflowed for an intermediate
//    byte >= 128 and corrupted the 0..65535 clamp, so AVR output differed from
//    the host. kHostCases keep every inoise16 coordinate below 0x8000 (no int
//    narrowing anywhere) and use octaves >= 2, so their output must equal the
//    host's. The FNV-1a hash of each fill is compared with the golden table,
//    which tests/unit/noise.cpp pins on the host.
//
// 2. Coordinate narrowing. The buffered fill_raw_noise16into8 keeps its
//    coordinate in an int, so on AVR a coordinate >= 0x8000 wraps and
//    sign-extends into inoise16's u32 parameter. fill_noise16 must narrow the
//    same way. kWrapCases push coordinates past 0x8000; they cannot match the
//    host (where int does not wrap), so they are compared with the buffered
//    reference on the same target instead.
//
// To change a case: edit it, run `bash test noise` (the host case prints the
// new hashes), update the table, then `bash test --run uno Test`.

#include "FastLED.h"
#include "noise.h"

namespace noise_hashes {

enum : int { kLeds = 16, kHostCaseCount = 3, kWrapCaseCount = 2 };

struct Case {
    fl::u8 octaves;
    fl::u16 x;
    fl::i16 scale;
    fl::u8 hue_octaves;
    fl::u16 hue_x;
    fl::i16 hue_scale;
    fl::u16 time;
    fl::u8 hue_shift;
};

// Max coordinate (x + 15 * scale) << (octaves - 1) stays below 0x8000.
const Case kHostCases[kHostCaseCount] FL_PROGMEM = {
    {3, 100, 40, 0, 7, 33, 1234, 10},
    {3, 2000, -30, 0, 500, 90, 4321, 0},
    {6, 10, 3, 0, 9000, -40, 65000, 200},
};

// Coordinates reach or pass 0x8000 (wraps through a 16-bit int on AVR).
const Case kWrapCases[kWrapCaseCount] FL_PROGMEM = {
    {2, 0x9000, 300, 1, 0, 50, 777, 0},
    {4, 0x4000, 1000, 2, 0x8000, 1000, 0xFFFF, 17},
};

// Golden hashes of fill_noise16 for kHostCases. Host output, pinned by
// tests/unit/noise.cpp.
const fl::u32 kGolden[kHostCaseCount] FL_PROGMEM = {
    0xCBBD05F1UL,  // 0
    0x1F322165UL,  // 1
    0x269837C9UL,  // 2
};

inline fl::u32 goldenHash(int c) {
    return FL_PGM_READ_DWORD_NEAR(&kGolden[c]);
}

inline Case readCase(const Case* src) {
    Case c;
    fl::u8* dst = reinterpret_cast<fl::u8*>(&c);
    const fl::u8* p = reinterpret_cast<const fl::u8*>(src);
    for (unsigned b = 0; b < sizeof(Case); ++b) {
        dst[b] = FL_PGM_READ_BYTE_NEAR(p + b);
    }
    return c;
}

inline void fill(const Case& c, CRGB* leds) {
    fill_noise16(leds, kLeds, c.octaves, c.x, c.scale, c.hue_octaves, c.hue_x,
                 c.hue_scale, c.time, c.hue_shift);
}

// FNV-1a of fill_noise16 output for kHostCases[c]. `leds` holds kLeds.
inline fl::u32 hostCaseHash(int c, CRGB* leds) {
    fill(readCase(&kHostCases[c]), leds);
    fl::u32 h = 2166136261UL;
    for (int i = 0; i < kLeds; ++i) {
        const fl::u8 bytes[3] = {leds[i].r, leds[i].g, leds[i].b};
        for (int b = 0; b < 3; ++b) {
            h ^= bytes[b];
            h *= 16777619UL;
        }
    }
    return h;
}

// Number of LEDs where fill_noise16 differs from the buffered
// fill_raw_noise16into8 / fill_raw_noise8 formulation for kWrapCases[c].
inline int wrapCaseMismatches(int c, CRGB* leds) {
    const Case k = readCase(&kWrapCases[c]);
    fill(k, leds);
    fl::u8 v[kLeds] = {0};
    fl::u8 hue[kLeds] = {0};
    fill_raw_noise16into8(v, kLeds, k.octaves, k.x, k.scale, k.time);
    fill_raw_noise8(hue, kLeds, k.hue_octaves, k.hue_x, k.hue_scale, k.time);
    int bad = 0;
    for (int i = 0; i < kLeds; ++i) {
        if (leds[i] != CRGB(CHSV(hue[i] + k.hue_shift, 255, v[i]))) {
            ++bad;
        }
    }
    return bad;
}

}  // namespace noise_hashes
