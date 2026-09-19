/// @file five_bit_hd_gamma_monotonic.cpp
/// Monotonicity of the HD encoders, for colour pipeline P8 (#4042).
///
/// P8 owns the criterion "APA102/SK9822 HD + native 16-bit encoders consume
/// wide output; **monotonic**; improved low-light resolution". Monotonicity
/// was covered for the gamma LUTs (`tests/fl/gfx/gamma_lut.cpp`), the
/// transfer functions and OKLab, but not for the encoders themselves -- and
/// they are where it is least obvious.
///
/// Both of these are *joint* solves: a per-pixel code plus a shared 5-bit
/// current field. That shape is the classic source of non-monotonic output,
/// because a one-step rise in the input can move the solver to a different
/// field and land lower than it started. Nothing was checking that it does
/// not, so this does.
///
/// What is measured is emitted light, not the code: an APA102-class part
/// drives PWM duty times current field, so `code * field` is the quantity a
/// rising input must not reverse.

#include "FastLED.h"
#include "fl/chipsets/encoders/hd108.h"
#include "fl/gfx/five_bit_hd_gamma.h"
#include "fl/stl/array.h"
#include "fl/stl/vector.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

namespace {

using fl::CRGB;
using fl::u8;
using fl::u16;

struct Emitted {
    CRGB color;
    u8 field;
};

Emitted fiveBitHd(const CRGB& in, const CRGB& scale, u8 global_brightness) {
    Emitted out;
    out.field = 0;
    fl::five_bit_hd_gamma_bitshift(fl::span<const CRGB>(&in, 1), scale,
                                   global_brightness,
                                   fl::span<CRGB>(&out.color, 1),
                                   fl::span<u8>(&out.field, 1));
    return out;
}

/// PWM duty times current field: what the die actually emits.
long emittedLight(const Emitted& e, int channel) {
    return static_cast<long>(e.color.raw[channel]) * static_cast<long>(e.field);
}

/// Runs `probe` over `steps` rising inputs and returns how many times the
/// emitted light went backwards.
template <typename Probe>
int countReversals(int steps, Probe probe) {
    long previous = -1;
    int reversals = 0;
    for (int step = 0; step <= steps; ++step) {
        const long value = probe(step);
        if (previous >= 0 && value < previous) {
            ++reversals;
        }
        previous = value;
    }
    return reversals;
}

}  // namespace

FL_TEST_CASE("Five-bit HD: a rising neutral never emits less light") {
    const u8 kBrightnesses[] = {255, 200, 128, 64, 32, 8};
    for (u8 brightness : kBrightnesses) {
        const int reversals = countReversals(255, [&](int v) {
            const u8 code = static_cast<u8>(v);
            return emittedLight(
                fiveBitHd(CRGB(code, code, code), CRGB(255, 255, 255), brightness), 0);
        });
        FL_CHECK_EQ(reversals, 0);
    }
}

FL_TEST_CASE("Five-bit HD: the 5-bit field really moves across that sweep") {
    // Vacuity guard for the case above. If the field were constant, the
    // sweep would be exercising the 8-bit code alone and would say nothing
    // about the joint solve -- which is the part that can reverse.
    bool seen[32] = {false};
    int distinct = 0;
    for (int v = 0; v <= 255; ++v) {
        const u8 code = static_cast<u8>(v);
        const Emitted e = fiveBitHd(CRGB(code, code, code), CRGB(255, 255, 255), 255);
        FL_REQUIRE_LT(int(e.field), 32);
        if (!seen[e.field]) {
            seen[e.field] = true;
            ++distinct;
        }
    }
    // Measured: 31 of the 32 fields appear over a single neutral ramp.
    FL_CHECK_GT(distinct, 24);
}

FL_TEST_CASE("Five-bit HD: a channel rising under a pinned maximum does not reverse") {
    // The case a neutral ramp cannot reach. The field follows the largest
    // channel, so a rising channel below it rides a field it does not set --
    // and when it overtakes, the field changes underneath it.
    for (int other = 0; other <= 255; other += 51) {
        const u8 pinned = static_cast<u8>(other);
        const int reversals = countReversals(255, [&](int v) {
            const u8 code = static_cast<u8>(v);
            return emittedLight(
                fiveBitHd(CRGB(code, pinned, pinned), CRGB(255, 255, 255), 255), 0);
        });
        FL_CHECK_EQ(reversals, 0);
    }
}

FL_TEST_CASE("Five-bit HD: rising global brightness does not reverse") {
    // Brightness reaches the same joint solve, so it can pick a different
    // field for the same colour.
    const u8 kColors[] = {1, 85, 169, 253};
    for (u8 value : kColors) {
        const int reversals = countReversals(255, [&](int gb) {
            const CRGB in(value, static_cast<u8>(value / 2), static_cast<u8>(value / 4));
            return emittedLight(fiveBitHd(in, CRGB(255, 255, 255), static_cast<u8>(gb)), 0);
        });
        FL_CHECK_EQ(reversals, 0);
    }
}

FL_TEST_CASE("Five-bit HD: a non-uniform colour scale does not reverse either") {
    // `colors_scale` is the per-channel correction, and it is not uniform in
    // any sketch that sets a colour temperature.
    for (int channel = 0; channel < 3; ++channel) {
        const int reversals = countReversals(255, [&](int v) {
            u8 rgb[3] = {40, 90, 200};
            rgb[channel] = static_cast<u8>(v);
            return emittedLight(
                fiveBitHd(CRGB(rgb[0], rgb[1], rgb[2]), CRGB(255, 200, 150), 200),
                channel);
        });
        FL_CHECK_EQ(reversals, 0);
    }
}

FL_TEST_CASE("HD108 is not a joint solve: the gain header is a constant maximum") {
    // Worth stating, because the shape differs from the five-bit path above
    // and the difference is what decides what monotonicity even means here.
    //
    // `hd108BrightnessHeader` discards its brightness argument and pins all
    // three gains to 31 -- "maximum gain for all channels for maximum
    // precision", with brightness applied to the 16-bit values before
    // encoding instead. So there is no field for a rising input to move, and
    // HD108's monotonicity is that of its 16-bit code alone.
    //
    // f0 = 1 RRRRR GG and f1 = GGG BBBBB, so gains of 31/31/31 are 0xFF 0xFF.
    const u8 kBrightnesses[] = {255, 128, 32, 1, 0};
    for (u8 brightness : kBrightnesses) {
        fl::array<u8, 3> pixel = {200, 100, 50};
        fl::vector<u8> encoded;
        encodeHD108(&pixel, &pixel + 1, fl::back_inserter(encoded), brightness);
        FL_REQUIRE_GE(encoded.size(), fl::size(16));
        FL_CHECK_EQ(int(encoded[8]), 0xFF);
        FL_CHECK_EQ(int(encoded[9]), 0xFF);
    }
}

FL_TEST_CASE("HD108: the 16-bit code never goes backwards as the input rises") {
    // What is left once the gain is constant. Read off the wire rather than
    // from `hd108GammaCorrect`, so the byte order and offsets are covered
    // too -- 8 start bytes, then 2 header, then big-endian RGB16.
    const int reversals = countReversals(255, [&](int v) {
        const u8 code = static_cast<u8>(v);
        fl::array<u8, 3> pixel = {code, code, code};
        fl::vector<u8> encoded;
        encodeHD108(&pixel, &pixel + 1, fl::back_inserter(encoded), 255);
        FL_REQUIRE_GE(encoded.size(), fl::size(16));
        return (static_cast<long>(encoded[10]) << 8) | static_cast<long>(encoded[11]);
    });
    FL_CHECK_EQ(reversals, 0);
}

FL_TEST_CASE("HD108: those bytes are the 16-bit code, not something constant") {
    // Vacuity guard for the case above, and it earned its place: an earlier
    // draft of this file read the gain nibble at `encoded[8] & 0x1F` as if
    // HD108 had a per-input field. It does not, that value is constant, and
    // the sweep was trivially monotonic until this check said so.
    long lowest = 65536;
    long highest = -1;
    for (int v = 0; v <= 255; ++v) {
        const u8 code = static_cast<u8>(v);
        fl::array<u8, 3> pixel = {code, code, code};
        fl::vector<u8> encoded;
        encodeHD108(&pixel, &pixel + 1, fl::back_inserter(encoded), 255);
        const long red16 = (static_cast<long>(encoded[10]) << 8) |
                           static_cast<long>(encoded[11]);
        if (red16 < lowest) { lowest = red16; }
        if (red16 > highest) { highest = red16; }
    }
    FL_CHECK_EQ(lowest, 0);
    // Above the 8-bit range, which is the "improved low-light resolution"
    // half of P8's criterion having somewhere to live.
    FL_CHECK_GT(highest, 60000);
}


// ---------------------------------------------------------------------------
// The same joint solve on 16-bit linear drives (`five_bit_hd_solve16`), which
// is what a colour-managed APA102 channel feeds it (#4042, B1). `min_field` is
// the flicker floor: 31 pins the field, lower values let the solve use it for
// low-light resolution.
// ---------------------------------------------------------------------------

namespace {

Emitted solve16(u16 r, u16 g, u16 b, u8 min_field) {
    Emitted e;
    fl::detail::five_bit_hd_solve16(r, g, b, min_field, &e.color, &e.field);
    return e;
}

}  // namespace

FL_TEST_CASE("Five-bit HD, 16-bit drive: a rising neutral never emits less light") {
    const u8 kFloors[] = {1, 2, 4, 8, 16, 31};
    for (u8 floor : kFloors) {
        // Every drive value, not a sample: the legacy field rule reversed
        // 5-8 times on this ramp, all at points a coarser sweep can step over.
        const int reversals = countReversals(65535, [&](int step) {
            const u16 v = static_cast<u16>(step);
            return emittedLight(solve16(v, v, v, floor), 0);
        });
        FL_CHECK_EQ(reversals, 0);
    }
}

FL_TEST_CASE("Five-bit HD, 16-bit drive: a channel under a pinned maximum does not reverse") {
    const u8 kFloors[] = {1, 8, 31};
    for (u8 floor : kFloors) {
        for (int other = 0; other <= 65535; other += 8191) {
            const u16 pinned = static_cast<u16>(other);
            const int reversals = countReversals(65535 / 32, [&](int step) {
                const u16 v = static_cast<u16>(step * 32);
                return emittedLight(solve16(v, pinned, pinned, floor), 0);
            });
            FL_CHECK_EQ(reversals, 0);
        }
    }
}

FL_TEST_CASE("Five-bit HD, 16-bit drive: the floor is honoured, and 31 pins the field") {
    for (int v = 0; v <= 65535; v += 97) {
        const u16 d = static_cast<u16>(v);
        FL_CHECK_EQ(int(solve16(d, d / 2, d / 3, 31).field), 31);
        FL_CHECK_GE(int(solve16(d, d / 2, d / 3, 8).field), 8);
        FL_CHECK_LE(int(solve16(d, d / 2, d / 3, 1).field), 31);
    }
}

FL_TEST_CASE("Five-bit HD, 16-bit drive: a lower floor resolves more low-light levels") {
    // The criterion's "improved low-light resolution": over the dimmest 1/32
    // of the drive range, count distinct emitted light levels (duty x field).
    // Pinned at 31 the field adds nothing, and the code alone has about eight
    // steps there; with the floor at 1 the field carries the range and the
    // code keeps its eight bits.
    auto distinct = [](u8 floor) {
        fl::vector<long> levels;
        for (int v = 0; v <= 2048; ++v) {
            const long light = emittedLight(solve16(static_cast<u16>(v), 0, 0, floor), 0);
            bool seen = false;
            for (long l : levels) {
                if (l == light) { seen = true; break; }
            }
            if (!seen) { levels.push_back(light); }
        }
        return static_cast<int>(levels.size());
    };
    const int pinned = distinct(31);
    const int free_field = distinct(1);
    FL_CHECK_LT(pinned, 12);
    FL_CHECK_GT(free_field, pinned * 8);
}
}
