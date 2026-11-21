// fill_noise8 / fill_noise16 must match the buffered reference formulation
// (two scratch planes filled by fill_raw_noise8 / fill_raw_noise16into8,
// then combined per pixel). FastLED#5330 removed the scratch planes; these
// tests pin the output so the bufferless path matches it on every target.
// The same change widened the fill_raw_noise16into8 accumulator, so AVR
// output for octaves >= 2 differs from pre-#5341 AVR builds (the old value
// came from 16-bit int overflow); examples/Test/NoiseHashes.h checks the
// AVR side on the avr8js Uno.

#include "noise.h"
#include "crgb.h"
#include "hsv2rgb.h"
#include "fl/stl/stdint.h"
#include "fl/stl/cstring.h"
#include "fl/stl/vector.h"
#include "test.h"
#include "examples/Test/NoiseHashes.h"

FL_TEST_FILE(FL_FILEPATH) {

namespace {

// Pre-#5330 fill_noise8/fill_noise16 body, kept verbatim in behaviour.
void reference_fill_noise(CRGB *leds, int num_leds, bool sixteen,
                          fl::u8 octaves, fl::u16 x, int scale,
                          fl::u8 hue_octaves, fl::u16 hue_x, int hue_scale,
                          fl::u16 time, fl::u8 hue_shift) {
    if (num_leds <= 0) return;
    for (int j = 0; j < num_leds; j += 255) {
        const int remaining = num_leds - j;
        const int per = remaining > 255 ? 255 : remaining;
        fl::u8 V[255];
        fl::u8 H[255];
        fl::memset(V, 0, sizeof(V));
        fl::memset(H, 0, sizeof(H));
        if (sixteen) {
            fill_raw_noise16into8(V, per, octaves, x, scale, time);
        } else {
            fill_raw_noise8(V, per, octaves, x, scale, time);
        }
        fill_raw_noise8(H, per, hue_octaves, hue_x, hue_scale, time);
        for (int i = 0; i < per; ++i) {
            leds[i + j] = CHSV(H[i] + hue_shift, 255, V[i]);
        }
    }
}

struct Params {
    int num_leds;
    fl::u8 octaves;
    fl::u16 x;
    int scale;
    fl::u8 hue_octaves;
    fl::u16 hue_x;
    int hue_scale;
    fl::u16 time;
    fl::u8 hue_shift;
};

const Params kCases[] = {
    {1, 1, 0, 0, 1, 0, 0, 0, 0},
    {16, 2, 1234, 57, 1, 4321, 33, 777, 0},
    {60, 4, 0xFFF0, 300, 3, 0x8000, 1000, 0xFFFF, 17},
    {255, 3, 42, -90, 2, 9000, -40, 12345, 200},
    {300, 5, 0xABCD, 4000, 4, 0x1357, 2500, 31337, 99},
    {511, 8, 7, 65000, 9, 65535, 70000, 2, 128},
    {20, 0, 100, 100, 0, 100, 100, 100, 5},
};

} // namespace

FL_TEST_CASE("fill_noise8 matches buffered reference") {
    for (const Params &p : kCases) {
        fl::vector<CRGB> got(p.num_leds, CRGB(1, 2, 3));
        fl::vector<CRGB> want(p.num_leds, CRGB(1, 2, 3));
        fill_noise8(got.data(), p.num_leds, p.octaves, p.x, p.scale,
                    p.hue_octaves, p.hue_x, p.hue_scale, p.time);
        reference_fill_noise(want.data(), p.num_leds, false, p.octaves, p.x,
                             p.scale, p.hue_octaves, p.hue_x, p.hue_scale,
                             p.time, 0);
        for (int i = 0; i < p.num_leds; ++i) {
            FL_CHECK_EQ(got[i], want[i]);
        }
    }
}

FL_TEST_CASE("fill_noise16 matches buffered reference") {
    for (const Params &p : kCases) {
        fl::vector<CRGB> got(p.num_leds, CRGB(1, 2, 3));
        fl::vector<CRGB> want(p.num_leds, CRGB(1, 2, 3));
        fill_noise16(got.data(), p.num_leds, p.octaves, p.x, p.scale,
                     p.hue_octaves, p.hue_x, p.hue_scale, p.time, p.hue_shift);
        reference_fill_noise(want.data(), p.num_leds, true, p.octaves, p.x,
                             p.scale, p.hue_octaves, p.hue_x, p.hue_scale,
                             p.time, p.hue_shift);
        for (int i = 0; i < p.num_leds; ++i) {
            FL_CHECK_EQ(got[i], want[i]);
        }
    }
}

FL_TEST_CASE("fill_noise16 AVR hash cases match the host golden table") {
    // Host half of the AVR 16-bit-int regression check. examples/Test runs
    // the same cases on the avr8js Uno (`bash test --run uno Test`) and
    // compares them to the same golden table, so both must match it.
    CRGB leds[noise_hashes::kLeds];
    for (int c = 0; c < noise_hashes::kHostCaseCount; ++c) {
        const fl::u32 h = noise_hashes::hostCaseHash(c, leds);
        fl::printf("  noise case %d: 0x%08lXUL\n", c,
                   static_cast<unsigned long>(h));
        FL_CHECK_EQ(h, noise_hashes::goldenHash(c));
    }
    // The wrap cases only narrow on 16-bit int; on the host they still must
    // match the buffered reference.
    for (int c = 0; c < noise_hashes::kWrapCaseCount; ++c) {
        FL_CHECK_EQ(noise_hashes::wrapCaseMismatches(c, leds), 0);
    }
}

} // FL_TEST_FILE
