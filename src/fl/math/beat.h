#pragma once

/// @file fl/math/beat.h
/// Waveform beat generators — sawtooth and sine waves at a given BPM.

#include "fl/stl/int.h"
#include "fl/stl/chrono.h" // fl::millis()
#include "fl/math/lib8static.h"
#include "fl/math/qfx.h"   // accum88
#include "fl/math/trig8.h"  // sin8, sin16
#include "fl/math/scale8.h" // scale8, scale16
#include "fl/stl/noexcept.h"

namespace fl {

/// @defgroup BeatGenerators Waveform Beat Generators
/// @{

/// Generates a 16-bit "sawtooth" wave at a given BPM, with BPM
/// specified in Q8.8 fixed-point format.
/// @param beats_per_minute_88 the frequency of the wave, in Q8.8 format
/// @param timebase the time offset of the wave from the millis() timer
/// @warning The BPM parameter **MUST** be provided in Q8.8 format! E.g.
/// for 120 BPM it would be 120*256 = 30720. If you just want to specify
/// "120", use beat16() or beat8().
LIB8STATIC u16 beat88(accum88 beats_per_minute_88, u32 timebase = 0) FL_NOEXCEPT {
    // BPM is 'beats per minute', or 'beats per 60000ms'.
    // To avoid using the (slower) division operator, we
    // want to convert 'beats per 60000ms' to 'beats per 65536ms',
    // and then use a simple, fast bit-shift to divide by 65536.
    //
    // The ratio 65536:60000 is 279.620266667:256; we'll call it 280:256.
    // The conversion is accurate to about 0.05%, more or less,
    // e.g. if you ask for "120 BPM", you'll get about "119.93".
    return (((fl::millis()) - timebase) * beats_per_minute_88 * 280) >> 16;
}

/// Generates a 16-bit "sawtooth" wave at a given BPM
/// @param beats_per_minute the frequency of the wave, in decimal
/// @param timebase the time offset of the wave from the millis() timer
LIB8STATIC u16 beat16(accum88 beats_per_minute, u32 timebase = 0) FL_NOEXCEPT {
    // Convert simple 8-bit BPM's to full Q8.8 accum88's if needed
    if (beats_per_minute < 256)
        beats_per_minute <<= 8;
    return beat88(beats_per_minute, timebase);
}

/// Generates an 8-bit "sawtooth" wave at a given BPM
/// @param beats_per_minute the frequency of the wave, in decimal
/// @param timebase the time offset of the wave from the millis() timer
LIB8STATIC u8 beat8(accum88 beats_per_minute, u32 timebase = 0) FL_NOEXCEPT {
    return beat16(beats_per_minute, timebase) >> 8;
}

/// Generates a 16-bit sine wave at a given BPM that oscillates within
/// a given range.
/// @param beats_per_minute_88 the frequency of the wave, in Q8.8 format
/// @param lowest the lowest output value of the sine wave
/// @param highest the highest output value of the sine wave
/// @param timebase the time offset of the wave from the millis() timer
/// @param phase_offset phase offset of the wave from the current position
/// @warning The BPM parameter **MUST** be provided in Q8.8 format!
LIB8STATIC u16 beatsin88(accum88 beats_per_minute_88, u16 lowest = 0,
                         u16 highest = 65535, u32 timebase = 0,
                         u16 phase_offset = 0) FL_NOEXCEPT {
    u16 beat = beat88(beats_per_minute_88, timebase);
    u16 beatsin = (sin16(beat + phase_offset) + 32768);
    u16 rangewidth = highest - lowest;
    u16 scaledbeat = scale16(beatsin, rangewidth);
    u16 result = lowest + scaledbeat;
    if (result > highest) {
        result = highest;
    }
    return result;
}

/// Generates a 16-bit sine wave at a given BPM that oscillates within
/// a given range.
/// @param beats_per_minute the frequency of the wave, in decimal
/// @param lowest the lowest output value of the sine wave
/// @param highest the highest output value of the sine wave
/// @param timebase the time offset of the wave from the millis() timer
/// @param phase_offset phase offset of the wave from the current position
LIB8STATIC u16 beatsin16(accum88 beats_per_minute, u16 lowest = 0,
                         u16 highest = 65535, u32 timebase = 0,
                         u16 phase_offset = 0) FL_NOEXCEPT {
    u16 beat = beat16(beats_per_minute, timebase);
    u16 beatsin = (sin16(beat + phase_offset) + 32768);
    u16 rangewidth = highest - lowest;
    u16 scaledbeat = scale16(beatsin, rangewidth);
    u16 result = lowest + scaledbeat;
    if (result > highest) {
        result = highest;
    }
    return result;
}

/// Generates an 8-bit sine wave at a given BPM that oscillates within
/// a given range.
/// @param beats_per_minute the frequency of the wave, in decimal
/// @param lowest the lowest output value of the sine wave
/// @param highest the highest output value of the sine wave
/// @param timebase the time offset of the wave from the millis() timer
/// @param phase_offset phase offset of the wave from the current position
LIB8STATIC u8 beatsin8(accum88 beats_per_minute, u8 lowest = 0,
                       u8 highest = 255, u32 timebase = 0,
                       u8 phase_offset = 0) FL_NOEXCEPT {
    u8 beat = beat8(beats_per_minute, timebase);
    u8 beatsin = sin8(beat + phase_offset);
    u8 rangewidth = highest - lowest;
    u8 scaledbeat = scale8(beatsin, rangewidth);
    u8 result = lowest + scaledbeat;
    if (result > highest) {
        result = highest;
    }
    return result;
}

/// @}

} // namespace fl
