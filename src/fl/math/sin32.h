#pragma once


#include "fl/stl/int.h"
#include "fl/stl/compiler_control.h"
#include "fastled_progmem.h"
#include "fl/stl/noexcept.h"
namespace fl {

// Paired sin/cos quarter-wave LUT with interleaved values and derivatives.
// Layout per entry [qi]: { y_sin(qi), m_sin(qi), y_cos(qi), m_cos(qi) }
// where y_cos(qi) = y_sin(64-qi), m_cos(qi) = m_sin(64-qi).
// 65 entries (indices 0..64), covering 0 to pi/2.
// Full sine/cosine reconstructed via quarter-wave symmetry.
// Quadratic interpolation with exact stored derivative for O(h^3) accuracy.
//
// Output range: [-2147418112, 2147418112] (= 32767 * 65536)
// Stride: 4 i32 per entry (16 bytes). 65 entries = 1040 bytes.
// Stored in FL_PROGMEM for AVR flash placement.
extern const i32 FL_PROGMEM sinCosPairedLut[];

struct SinCos32 {
    i32 sin_val;
    i32 cos_val;
};

// Read an i32 from the PROGMEM-qualified LUT.
FASTLED_FORCE_INLINE i32 read_sin32_lut(const i32* addr) FL_NO_EXCEPT {
    return (i32)FL_PGM_READ_DWORD_ALIGNED(addr);
}

// Core branchless quadratic interpolation from paired LUT.
// qi: quarter-wave table index (0..64)
// qi_next: adjacent index (qi+1 for direct, qi-1 for mirrored)
// dmask: 0x00000000 (direct) or 0xFFFFFFFF (mirrored, negates derivative)
// t: fraction in [0, 65535]
// offset: 0 for sin, 2 for cos (selects which pair within the stride-4 entry)
FASTLED_FORCE_INLINE i32 sin32_interp(u8 qi, u8 qi_next, i32 dmask, u32 t, u8 offset = 0) FL_NO_EXCEPT {
    i32 y0 = read_sin32_lut(&sinCosPairedLut[qi * 4 + offset]);
    i32 m0 = read_sin32_lut(&sinCosPairedLut[qi * 4 + offset + 1]);
    i32 y1 = read_sin32_lut(&sinCosPairedLut[qi_next * 4 + offset]);

    // Branchless conditional negate derivative
    m0 = (m0 ^ dmask) - dmask;

    // Quadratic interpolation (Horner form, 2 muls):
    // P(t) = y0 + T*(m0 + T*(y1 - y0 - m0))  where T = t/65536
    i32 c = y1 - y0 - m0;
    i32 r = (i32)((i64)c * t >> 16) + m0;
    return (i32)(((i64)r * t >> 16) + (i64)y0);
}

// 0 to 16777216 is a full circle
// output is between -2147418112 and 2147418112
// Branchless quarter-wave lookup with quadratic interpolation.
// Cost: 3 table loads, 2 i64 multiplies, no branches.
FASTLED_FORCE_INLINE i32 sin32(u32 angle) FL_NO_EXCEPT {
    u8 angle256 = static_cast<u8>(angle >> 16);  // 0..255
    u32 t = angle & 0xFFFF;                       // 0..65535

    u8 quadrant = angle256 >> 6;    // 0..3
    u8 pos = angle256 & 0x3F;      // 0..63

    // Branchless quarter-wave mapping
    u8 mirror = quadrant & 1;
    u8 qi = static_cast<u8>(pos + mirror * (64 - 2 * pos));
    u8 qi_next = static_cast<u8>(qi + 1 - 2 * mirror);

    i32 dmask = -static_cast<i32>(mirror);
    i32 raw = sin32_interp(qi, qi_next, dmask, t);

    // Branchless sign: negative in quadrants 2, 3
    i32 vmask = -static_cast<i32>((quadrant >> 1) & 1);
    return (i32)(((i64)raw ^ vmask) - vmask);
}

// 0 to 16777216 is a full circle
// output is between -2147418112 and 2147418112
FASTLED_FORCE_INLINE i32 cos32(u32 angle) FL_NO_EXCEPT {
    return sin32(angle + 4194304u);
}

// Compute sin and cos simultaneously, faster than separate sin32+cos32 calls.
// Uses paired LUT: sin and cos data colocated at same index (no qi_c computation).
// Cost: 6 table loads, 4 i64 multiplies, no branches.
FASTLED_FORCE_INLINE SinCos32 sincos32(u32 angle) FL_NO_EXCEPT {
    u8 angle256 = static_cast<u8>(angle >> 16);
    u32 t = angle & 0xFFFF;

    u8 quadrant = angle256 >> 6;
    u8 pos = angle256 & 0x3F;

    // Quarter-wave mapping (same for both sin and cos)
    u8 mirror_s = quadrant & 1;
    u8 qi = static_cast<u8>(pos + mirror_s * (64 - 2 * pos));
    u8 qi_next = static_cast<u8>(qi + 1 - 2 * mirror_s);

    // Derivative masks: sin and cos have opposite mirror states
    i32 sdmask = -static_cast<i32>(mirror_s);
    i32 cdmask = ~sdmask;  // opposite mirror

    // Sin at offset 0, cos at offset 2 — same qi, same cache line
    i32 s_raw = sin32_interp(qi, qi_next, sdmask, t, 0);
    i32 c_raw = sin32_interp(qi, qi_next, cdmask, t, 2);

    // Sin sign: negative in quadrants 2, 3
    i32 svmask = -static_cast<i32>((quadrant >> 1) & 1);
    // Cos sign: negative in quadrants 1, 2 (XOR of quadrant bits)
    i32 cvmask = -static_cast<i32>((quadrant ^ (quadrant >> 1)) & 1);

    SinCos32 out;
    out.sin_val = (i32)(((i64)s_raw ^ svmask) - svmask);
    out.cos_val = (i32)(((i64)c_raw ^ cvmask) - cvmask);
    return out;
}

// 0 to 65536 is a full circle
// output is between -32767 and 32767
FASTLED_FORCE_INLINE i16 sin16lut(u16 angle) FL_NO_EXCEPT {
    u32 angle32 = static_cast<u32>(angle) << 8;
    return static_cast<i16>(sin32(angle32) >> 16);
}

// 0 to 65536 is a full circle
// output is between -32767 and 32767
FASTLED_FORCE_INLINE i16 cos16lut(u16 angle) FL_NO_EXCEPT {
    u32 angle32 = static_cast<u32>(angle) << 8;
    return static_cast<i16>(cos32(angle32) >> 16);
}

} // namespace fl
