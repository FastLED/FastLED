#pragma once


#include "fl/int.h"
#include "force_inline.h"
#include "fastled_progmem.h"

namespace fl {

// Quarter-wave sine LUT with interleaved [value, derivative] pairs (i32).
// 65 entries (indices 0..64), covering 0 to pi/2.
// Full sine is reconstructed via quarter-wave symmetry.
// Quadratic interpolation with exact stored derivative for O(h^3) accuracy.
//
// Output range: [-2147418112, 2147418112] (= 32767 * 65536)
// Table size: 130 i32 = 520 bytes (was 321 i16 = 642 bytes)
// Stored in FL_PROGMEM for AVR flash placement.

extern const i32 FL_PROGMEM sinQuarterLut[];

struct SinCos32 {
    i32 sin_val;
    i32 cos_val;
};

// Read an i32 from the PROGMEM-qualified LUT.
FASTLED_FORCE_INLINE static i32 sin32_pgm_read(const i32* addr) {
    return (i32)FL_PGM_READ_DWORD_ALIGNED(addr);
}

// Core branchless quadratic interpolation from quarter-wave table.
// qi: quarter-wave table index (0..64)
// qi_next: adjacent index (qi+1 for direct, qi-1 for mirrored)
// dmask: 0x00000000 (direct) or 0xFFFFFFFF (mirrored, negates derivative)
// t: fraction in [0, 65535]
FASTLED_FORCE_INLINE static i32 sin32_interp(u8 qi, u8 qi_next, i32 dmask, u32 t) {
    i32 y0 = sin32_pgm_read(&sinQuarterLut[qi * 2]);
    i32 m0 = sin32_pgm_read(&sinQuarterLut[qi * 2 + 1]);
    i32 y1 = sin32_pgm_read(&sinQuarterLut[qi_next * 2]);

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
FASTLED_FORCE_INLINE static i32 sin32(u32 angle) {
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
FASTLED_FORCE_INLINE static i32 cos32(u32 angle) {
    return sin32(angle + 4194304u);
}

// Compute sin and cos simultaneously, faster than separate sin32+cos32 calls.
// Shares angle decomposition; derives cos table index from sin's (cos_qi = 64 - sin_qi).
// Cost: 6 table loads, 4 i64 multiplies, no branches.
// (vs 2 separate calls: 6 loads, 4 muls, but duplicate angle decomposition)
FASTLED_FORCE_INLINE static SinCos32 sincos32(u32 angle) {
    u8 angle256 = static_cast<u8>(angle >> 16);
    u32 t = angle & 0xFFFF;

    u8 quadrant = angle256 >> 6;
    u8 pos = angle256 & 0x3F;

    // Sin quarter-wave mapping
    u8 mirror_s = quadrant & 1;
    u8 qi_s = static_cast<u8>(pos + mirror_s * (64 - 2 * pos));
    u8 qi_next_s = static_cast<u8>(qi_s + 1 - 2 * mirror_s);

    // Cos quarter-wave: cos(x) = sin(x + pi/2), so cos's quadrant = sin's + 1.
    // This means cos mirrors when sin doesn't and vice versa.
    // Key identity: cos_qi = 64 - sin_qi, cos_qi_next = 64 - sin_qi_next.
    u8 qi_c = static_cast<u8>(64 - qi_s);
    u8 qi_next_c = static_cast<u8>(64 - qi_next_s);

    // Derivative masks: sin and cos have opposite mirror states
    i32 sdmask = -static_cast<i32>(mirror_s);
    i32 cdmask = ~sdmask;  // opposite mirror

    // Interleave sin/cos loads and computation for ILP
    i32 s_raw = sin32_interp(qi_s, qi_next_s, sdmask, t);
    i32 c_raw = sin32_interp(qi_c, qi_next_c, cdmask, t);

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
FASTLED_FORCE_INLINE static i16 sin16lut(u16 angle) {
    u32 angle32 = static_cast<u32>(angle) << 8;
    return static_cast<i16>(sin32(angle32) >> 16);
}

// 0 to 65536 is a full circle
// output is between -32767 and 32767
FASTLED_FORCE_INLINE static i16 cos16lut(u16 angle) {
    u32 angle32 = static_cast<u32>(angle) << 8;
    return static_cast<i16>(cos32(angle32) >> 16);
}


} // namespace fl
