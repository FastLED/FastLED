#pragma once

/// @file fl/gfx/binary_dither.h
/// Temporal binary dithering, as one self-contained, testable unit (#4672).
///
/// ============================================================================
/// TEMPORAL DITHERING OVERVIEW
/// ============================================================================
///
/// Temporal dithering recovers fractional brightness precision lost to integer
/// quantization by varying pixel values across frames. At refresh rates above
/// ~50Hz, human vision integrates these variations, perceiving the true
/// fractional brightness.
///
/// THE PROBLEM:
///   Integer scaling causes color shifts at low brightness. For example:
///     CRGB(100, 60, 20) at 20% brightness -> RGB(19, 11, 3)
///   Each channel loses different fractional precision, distorting the color.
///
/// THE SOLUTION:
///   Add frame-varying noise BEFORE scaling, causing different rounding outcomes:
///     Frame 1: scale8(100+0, 51) = 19
///     Frame 2: scale8(100+3, 51) = 20  <- noise pushed over threshold
///   Your eye averages these to perceive the correct fractional brightness.
///
/// THE ALGORITHM:
///   1. Frame counter R cycles 0-7, creating an 8-frame pattern
///   2. Bit-reverse R to Q (0->0, 1->128, 2->64...) to distribute pattern temporally
///   3. Center pattern: Q += 16
///   4. Scale per channel: e[i] = 256/brightness, d[i] = scale8(Q, e[i])
///      Lower brightness needs BIGGER dither to compensate for larger % error
///   5. Toggle between pixels: d[i] = e[i] - d[i] (spatial distribution)
///   6. Apply: pixel = scale8(qadd8(pixel, d[i]), brightness)
///
/// VIRTUAL BITS:
///   8-frame cycle at 400Hz = 50Hz complete cycle -> +3 "virtual" bits
///   Result: 8-bit hardware provides 11-bit perceived precision (0-2047 levels)
///
/// DISABLE FOR:
///   - Cameras/photography (captures individual frames, sees flicker)
///   - Slow refresh <50Hz (visible flickering)
///   - Video recording (frame rate mismatches create artifacts)
///   Use: FastLED.setDither(DISABLE_DITHER) at runtime, or define
///   NO_DITHERING=1 to compile the algorithm out (see `fl::Dither` below).
///
/// NOTE: This is NOT gamma correction. Dithering is pure temporal averaging
/// to recover quantization precision.
///
/// ============================================================================
///
/// The state is two caller-owned arrays, indexed by *source* channel:
///   - `d[3]`: the offset added to the current pixel.
///   - `e[3]`: the toggle range. Stored one less than the `256/scale + 1` it is
///     computed from, so `stepping` is the single subtraction `d = e - d`.
/// `PixelController` owns them as its public `d`/`e` members because
/// hand-written drivers (AVR clockless asm, the M0 and Teensy structs) read
/// them by name; this file owns everything that is done to them.
///
/// Two policies share one static interface:
///   - `fl::BinaryDither` -- the algorithm.
///   - `fl::NoDither`     -- `init` clears, `step` does nothing, `apply` is the
///                           identity. Every call compiles to nothing (or to
///                           the zeroing that keeps the asm drivers defined).
/// `fl::Dither` is the one selected by `NO_DITHERING`.

#include "fl/stl/int.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"
#include "fl/math/math8.h"
#include "fl/math/scale8.h"

/// Predicted max update rate, in Hertz
#ifndef MAX_LIKELY_UPDATE_RATE_HZ
#define MAX_LIKELY_UPDATE_RATE_HZ     400
#endif

/// Minimum acceptable dithering rate, in Hertz
#ifndef MIN_ACCEPTABLE_DITHER_RATE_HZ
#define MIN_ACCEPTABLE_DITHER_RATE_HZ  50
#endif

/// The number of updates in a single dither cycle
#ifndef UPDATES_PER_FULL_DITHER_CYCLE
#define UPDATES_PER_FULL_DITHER_CYCLE (MAX_LIKELY_UPDATE_RATE_HZ / MIN_ACCEPTABLE_DITHER_RATE_HZ)
#endif

/// Set "virtual bits" of dithering to the highest level
/// that is not likely to cause excessive flickering at
/// low brightness levels + low update rates.
/// These pre-set values are a little ambitious, since
/// a 400Hz update rate for WS2811-family LEDs is only
/// possible with 85 pixels or fewer.
/// The division is done at compile time, so there's no runtime
/// cost, but the values are still hard-coded.
#ifndef RECOMMENDED_VIRTUAL_BITS
#define RECOMMENDED_VIRTUAL_BITS ((UPDATES_PER_FULL_DITHER_CYCLE>1) + \
                                  (UPDATES_PER_FULL_DITHER_CYCLE>2) + \
                                  (UPDATES_PER_FULL_DITHER_CYCLE>4) + \
                                  (UPDATES_PER_FULL_DITHER_CYCLE>8) + \
                                  (UPDATES_PER_FULL_DITHER_CYCLE>16) + \
                                  (UPDATES_PER_FULL_DITHER_CYCLE>32) + \
                                  (UPDATES_PER_FULL_DITHER_CYCLE>64) + \
                                  (UPDATES_PER_FULL_DITHER_CYCLE>128) )
#endif

/// Alias for RECOMMENDED_VIRTUAL_BITS
#ifndef VIRTUAL_BITS
#define VIRTUAL_BITS RECOMMENDED_VIRTUAL_BITS
#endif

namespace fl {

/// Temporal binary dithering over caller-owned `d[3]`/`e[3]`.
struct BinaryDither {
    enum { kEnabled = 1 };

    /// The frame's offset pattern value Q: `frame` wrapped to `VIRTUAL_BITS`,
    /// bit-reversed so consecutive frames land far apart (0,1,2,3 ->
    /// 0,128,64,192), then shifted to the middle of its bin.
    static FASTLED_FORCE_INLINE u8 phase(u8 frame) FL_NO_EXCEPT {
        frame &= (0x01 << VIRTUAL_BITS) - 1;
        u8 q = 0;
        if (frame & 0x01) { q |= 0x80; }
        if (frame & 0x02) { q |= 0x40; }
        if (frame & 0x04) { q |= 0x20; }
        if (frame & 0x08) { q |= 0x10; }
        if (frame & 0x10) { q |= 0x08; }
        if (frame & 0x20) { q |= 0x04; }
        if (frame & 0x40) { q |= 0x02; }
        if (frame & 0x80) { q |= 0x01; }
        if (VIRTUAL_BITS < 8) {
            q += 0x01 << (7 - VIRTUAL_BITS);
        }
        return q;
    }

    /// Q scaled into `[0, range)`: the offset for one channel.
    static FASTLED_FORCE_INLINE u8 offset(u8 q, u8 range) FL_NO_EXCEPT {
        u8 d = fl::scale8(q, range);
#if (FASTLED_SCALE8_FIXED == 1)
        // Adjust for the fixed scale8's rounding up.
        if (d) { --d; }
#endif
        return d;
    }

    /// Set up `e` from the per-channel brightness scale and `d` from `frame`.
    /// Lower brightness needs a bigger offset: `e = 256/scale`, so the offset
    /// spans one output step's worth of input. A zero scale (unlit channel)
    /// gets no dither.
    static FASTLED_FORCE_INLINE void init(u8 (&d)[3], u8 (&e)[3], const u8 (&scale)[3],
                                          u8 frame) FL_NO_EXCEPT {
        const u8 q = phase(frame);
        for (int i = 0; i < 3; ++i) {
            // Computed in a local and stored once; re-reading `e[i]` after
            // the `d[i]` store cost 36 B on AVR (#4672).
            const u8 s = scale[i];
            u8 range = s ? (256 / s) + 1 : 0;
            d[i] = offset(q, range);
            if (range) { --range; }
            e[i] = range;
        }
    }

    /// Re-point `d` at `frame`, keeping the ranges `init` left in `e`.
    /// `reseed(d, e, f)` leaves the same `d` as `init(d, e, scale, f)`.
    static FASTLED_FORCE_INLINE void reseed(u8 (&d)[3], const u8 (&e)[3], u8 frame) FL_NO_EXCEPT {
        const u8 q = phase(frame);
        for (int i = 0; i < 3; ++i) {
            // `e[i]` is init's range less one; zero only for an unlit
            // channel, which carries no offset.
            d[i] = e[i] ? offset(q, static_cast<u8>(e[i] + 1)) : 0;
        }
    }

    static FASTLED_FORCE_INLINE void clear(u8 (&d)[3], u8 (&e)[3]) FL_NO_EXCEPT {
        d[0] = d[1] = d[2] = e[0] = e[1] = e[2] = 0;
    }

    static FASTLED_FORCE_INLINE bool active(const u8 (&e)[3]) FL_NO_EXCEPT {
        return (e[0] | e[1] | e[2]) != 0;
    }

    /// Next pixel: toggle each offset to its complement in `[0, e]`, so
    /// neighbours dither in opposite directions.
    static FASTLED_FORCE_INLINE void step(u8 (&d)[3], const u8 (&e)[3]) FL_NO_EXCEPT {
        d[0] = e[0] - d[0];
        d[1] = e[1] - d[1];
        d[2] = e[2] - d[2];
    }

    /// `step` for a single channel (drivers that pre-step their first byte).
    static FASTLED_FORCE_INLINE void stepChannel(u8 (&d)[3], const u8 (&e)[3], int c) FL_NO_EXCEPT {
        d[c] = e[c] - d[c];
    }

    /// Add the offset to a source byte, before scaling. Black is never lifted.
    static FASTLED_FORCE_INLINE u8 apply(u8 b, u8 d) FL_NO_EXCEPT {
        return b ? fl::qadd8(b, d) : 0;
    }
};

/// The disabled policy: same interface, no work. `init` still zeroes the
/// state, because hand-written drivers read `d`/`e` directly.
struct NoDither {
    enum { kEnabled = 0 };

    static FASTLED_FORCE_INLINE u8 phase(u8) FL_NO_EXCEPT { return 0; }
    static FASTLED_FORCE_INLINE u8 offset(u8, u8) FL_NO_EXCEPT { return 0; }
    static FASTLED_FORCE_INLINE void init(u8 (&d)[3], u8 (&e)[3], const u8 (&)[3], u8) FL_NO_EXCEPT {
        clear(d, e);
    }
    static FASTLED_FORCE_INLINE void reseed(u8 (&)[3], const u8 (&)[3], u8) FL_NO_EXCEPT {}
    static FASTLED_FORCE_INLINE void clear(u8 (&d)[3], u8 (&e)[3]) FL_NO_EXCEPT {
        d[0] = d[1] = d[2] = e[0] = e[1] = e[2] = 0;
    }
    static FASTLED_FORCE_INLINE bool active(const u8 (&)[3]) FL_NO_EXCEPT { return false; }
    static FASTLED_FORCE_INLINE void step(u8 (&)[3], const u8 (&)[3]) FL_NO_EXCEPT {}
    static FASTLED_FORCE_INLINE void stepChannel(u8 (&)[3], const u8 (&)[3], int) FL_NO_EXCEPT {}
    static FASTLED_FORCE_INLINE u8 apply(u8 b, u8) FL_NO_EXCEPT { return b; }
};

/// The policy this build uses. `NO_DITHERING=1` (forced on limited AVR parts
/// by led_sysdefs_avr.h) selects `NoDither`. Like the code this replaced, it
/// reads `NO_DITHERING` as already defined by `FastLED.h` -> `led_sysdefs.h`;
/// it deliberately does not include `led_sysdefs.h` itself, because that pulls
/// in `<Arduino.h>` and every unity TU that reaches `pixel_controller.h`
/// would parse the whole platform SDK (#4672).
#if defined(NO_DITHERING) && (NO_DITHERING == 1)
typedef NoDither Dither;
#else
typedef BinaryDither Dither;
#endif

}  // namespace fl
