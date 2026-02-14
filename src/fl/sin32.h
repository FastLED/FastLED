#pragma once


#include "fl/int.h"
#include "force_inline.h"
#include "fastled_progmem.h"
#include "fl/simd.h"  // Platform-dispatched SIMD (SSE2/NEON/scalar)
#include "fl/align.h"
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
FASTLED_FORCE_INLINE i32 read_sin32_lut(const i32* addr) {
    return (i32)FL_PGM_READ_DWORD_ALIGNED(addr);
}

// Core branchless quadratic interpolation from paired LUT.
// qi: quarter-wave table index (0..64)
// qi_next: adjacent index (qi+1 for direct, qi-1 for mirrored)
// dmask: 0x00000000 (direct) or 0xFFFFFFFF (mirrored, negates derivative)
// t: fraction in [0, 65535]
// offset: 0 for sin, 2 for cos (selects which pair within the stride-4 entry)
FASTLED_FORCE_INLINE i32 sin32_interp(u8 qi, u8 qi_next, i32 dmask, u32 t, u8 offset = 0) {
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
FASTLED_FORCE_INLINE i32 sin32(u32 angle) {
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
FASTLED_FORCE_INLINE i32 cos32(u32 angle) {
    return sin32(angle + 4194304u);
}

// Compute sin and cos simultaneously, faster than separate sin32+cos32 calls.
// Uses paired LUT: sin and cos data colocated at same index (no qi_c computation).
// Cost: 6 table loads, 4 i64 multiplies, no branches.
FASTLED_FORCE_INLINE SinCos32 sincos32(u32 angle) {
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
FASTLED_FORCE_INLINE i16 sin16lut(u16 angle) {
    u32 angle32 = static_cast<u32>(angle) << 8;
    return static_cast<i16>(sin32(angle32) >> 16);
}

// 0 to 65536 is a full circle
// output is between -32767 and 32767
FASTLED_FORCE_INLINE i16 cos16lut(u16 angle) {
    u32 angle32 = static_cast<u32>(angle) << 8;
    return static_cast<i16>(cos32(angle32) >> 16);
}

/// Combined sin+cos result for 4 angles (raw SIMD primitives)
struct FL_ALIGNAS(16) SinCos32_simd {
    simd::simd_u32x4 sin_vals;  // 4 sin results (raw i32, range: -2147418112 to 2147418112)
    simd::simd_u32x4 cos_vals;  // 4 cos results (raw i32, range: -2147418112 to 2147418112)
};

/// Process 4 angles simultaneously, returning vectorized sin/cos values
/// SIMD-optimized implementation with sequential LUT access and vectorized arithmetic
///
/// @param angles 4 u32 angles (0 to 16777216 per angle is a full circle)
/// @return SinCos32_simd with raw i32 values (range: -2147418112 to 2147418112)
FASTLED_FORCE_INLINE SinCos32_simd sincos32_simd(simd::simd_u32x4 angles) {
    // Phase 1a: SIMD angle decomposition (extract components vectorially)
    // Extract angle256 (top 8 bits) using vectorized right shift
    simd::simd_u32x4 angle256_vec = simd::srl_u32_4(angles, 16);

    // Extract t (fractional part, bottom 16 bits) using vectorized AND
    simd::simd_u32x4 t_vec = simd::and_u32_4(angles, simd::set1_u32_4(0xFFFF));

    // Extract quadrant (top 2 bits of angle256) using vectorized shift
    simd::simd_u32x4 quadrant_vec = simd::srl_u32_4(angle256_vec, 6);

    // Extract pos (bottom 6 bits of angle256) using vectorized AND
    simd::simd_u32x4 pos_vec = simd::and_u32_4(angle256_vec, simd::set1_u32_4(0x3F));

    // Extract mirror_s (LSB of quadrant) using vectorized AND
    simd::simd_u32x4 mirror_s_vec = simd::and_u32_4(quadrant_vec, simd::set1_u32_4(1));

    // Store vectorized results for scalar processing
    u32 pos_array[4], mirror_s_array[4], quadrant_array[4];
    simd::store_u32_4(pos_array, pos_vec);
    simd::store_u32_4(mirror_s_array, mirror_s_vec);
    simd::store_u32_4(quadrant_array, quadrant_vec);

    // Phase 1b: SIMD mask computation (fully parallelizable)
    // Compute all masks in SIMD before scalar quarter-wave mapping

    // Derivative masks: sdmask = -mirror_s, cdmask = ~sdmask
    simd::simd_u32x4 sdmask_vec = simd::sub_i32_4(simd::set1_u32_4(0), mirror_s_vec);  // 0 - mirror_s = -mirror_s
    simd::simd_u32x4 cdmask_vec = simd::xor_u32_4(sdmask_vec, simd::set1_u32_4(0xFFFFFFFF));  // ~sdmask

    // Sign masks: extract bits and negate
    simd::simd_u32x4 quadrant_bit1 = simd::and_u32_4(simd::srl_u32_4(quadrant_vec, 1), simd::set1_u32_4(1));  // (quadrant >> 1) & 1
    simd::simd_u32x4 svmask_vec = simd::sub_i32_4(simd::set1_u32_4(0), quadrant_bit1);  // -bit

    simd::simd_u32x4 quadrant_xor = simd::xor_u32_4(quadrant_vec, simd::srl_u32_4(quadrant_vec, 1));  // quadrant ^ (quadrant >> 1)
    simd::simd_u32x4 quadrant_xor_bit0 = simd::and_u32_4(quadrant_xor, simd::set1_u32_4(1));  // & 1
    simd::simd_u32x4 cvmask_vec = simd::sub_i32_4(simd::set1_u32_4(0), quadrant_xor_bit0);  // -bit

    // Phase 1c: Vectorized quarter-wave index mapping (SIMD branchless bit manipulation)
    // Compute all 4 LUT indices in parallel using SIMD operations

    // Compute qi_s vectorially: qi_s = pos + mirror_s * (64 - 2*pos)
    // term = 64 - 2*pos = 64 - (pos + pos)
    simd::simd_u32x4 two_pos = simd::add_i32_4(pos_vec, pos_vec);  // 2*pos
    simd::simd_u32x4 term_vec = simd::sub_i32_4(simd::set1_u32_4(64), two_pos);  // 64 - 2*pos
    simd::simd_u32x4 masked_term = simd::and_u32_4(term_vec, sdmask_vec);  // term & (-mirror_s), sdmask = -mirror_s
    simd::simd_u32x4 qi_s_vec = simd::add_i32_4(pos_vec, masked_term);  // pos + masked_term

    // Compute qi_next_s vectorially: qi_next_s = qi_s + 1 - 2*mirror_s
    simd::simd_u32x4 two_mirror_s = simd::add_i32_4(mirror_s_vec, mirror_s_vec);  // 2*mirror_s
    simd::simd_u32x4 qi_next_s_vec = simd::sub_i32_4(simd::add_i32_4(qi_s_vec, simd::set1_u32_4(1)), two_mirror_s);  // qi_s + 1 - 2*mirror_s

    // Store sin indices as u32 directly (no u8 conversion needed)
    u32 qi_s_array[4], qi_next_s_array[4];
    simd::store_u32_4(qi_s_array, qi_s_vec);
    simd::store_u32_4(qi_next_s_array, qi_next_s_vec);

    // Phase 2: LUT lookups using sinCosPairedLut (stride 4 per entry, 1040 bytes)
    // Layout: [y_sin, m_sin, y_cos, m_cos] — all data colocated for cache locality
    i32 y0_s[4], m0_s[4], y1_s[4];
    i32 y0_c[4], m0_c[4], y1_c[4];

    // Prefetch paired LUT entries (sin+cos colocated, stride 4)
    __builtin_prefetch(&sinCosPairedLut[qi_s_array[0] * 4], 0, 0);
    __builtin_prefetch(&sinCosPairedLut[qi_s_array[1] * 4], 0, 0);
    __builtin_prefetch(&sinCosPairedLut[qi_s_array[2] * 4], 0, 0);
    __builtin_prefetch(&sinCosPairedLut[qi_s_array[3] * 4], 0, 0);

    // Unrolled LUT lookups: y_sin, m_sin, y_cos, m_cos per entry
    y0_s[0] = read_sin32_lut(&sinCosPairedLut[qi_s_array[0] * 4]);
    m0_s[0] = read_sin32_lut(&sinCosPairedLut[qi_s_array[0] * 4 + 1]);
    y0_c[0] = read_sin32_lut(&sinCosPairedLut[qi_s_array[0] * 4 + 2]);
    m0_c[0] = read_sin32_lut(&sinCosPairedLut[qi_s_array[0] * 4 + 3]);
    y1_s[0] = read_sin32_lut(&sinCosPairedLut[qi_next_s_array[0] * 4]);
    y1_c[0] = read_sin32_lut(&sinCosPairedLut[qi_next_s_array[0] * 4 + 2]);

    y0_s[1] = read_sin32_lut(&sinCosPairedLut[qi_s_array[1] * 4]);
    m0_s[1] = read_sin32_lut(&sinCosPairedLut[qi_s_array[1] * 4 + 1]);
    y0_c[1] = read_sin32_lut(&sinCosPairedLut[qi_s_array[1] * 4 + 2]);
    m0_c[1] = read_sin32_lut(&sinCosPairedLut[qi_s_array[1] * 4 + 3]);
    y1_s[1] = read_sin32_lut(&sinCosPairedLut[qi_next_s_array[1] * 4]);
    y1_c[1] = read_sin32_lut(&sinCosPairedLut[qi_next_s_array[1] * 4 + 2]);

    y0_s[2] = read_sin32_lut(&sinCosPairedLut[qi_s_array[2] * 4]);
    m0_s[2] = read_sin32_lut(&sinCosPairedLut[qi_s_array[2] * 4 + 1]);
    y0_c[2] = read_sin32_lut(&sinCosPairedLut[qi_s_array[2] * 4 + 2]);
    m0_c[2] = read_sin32_lut(&sinCosPairedLut[qi_s_array[2] * 4 + 3]);
    y1_s[2] = read_sin32_lut(&sinCosPairedLut[qi_next_s_array[2] * 4]);
    y1_c[2] = read_sin32_lut(&sinCosPairedLut[qi_next_s_array[2] * 4 + 2]);

    y0_s[3] = read_sin32_lut(&sinCosPairedLut[qi_s_array[3] * 4]);
    m0_s[3] = read_sin32_lut(&sinCosPairedLut[qi_s_array[3] * 4 + 1]);
    y0_c[3] = read_sin32_lut(&sinCosPairedLut[qi_s_array[3] * 4 + 2]);
    m0_c[3] = read_sin32_lut(&sinCosPairedLut[qi_s_array[3] * 4 + 3]);
    y1_s[3] = read_sin32_lut(&sinCosPairedLut[qi_next_s_array[3] * 4]);
    y1_c[3] = read_sin32_lut(&sinCosPairedLut[qi_next_s_array[3] * 4 + 2]);

    // Load into SIMD registers
    simd::simd_u32x4 y0_s_v = simd::load_u32_4(reinterpret_cast<const u32*>(y0_s)); // ok reinterpret cast
    simd::simd_u32x4 y1_s_v = simd::load_u32_4(reinterpret_cast<const u32*>(y1_s)); // ok reinterpret cast
    simd::simd_u32x4 y0_c_v = simd::load_u32_4(reinterpret_cast<const u32*>(y0_c)); // ok reinterpret cast
    simd::simd_u32x4 y1_c_v = simd::load_u32_4(reinterpret_cast<const u32*>(y1_c)); // ok reinterpret cast
    simd::simd_u32x4 m0_s_v = simd::load_u32_4(reinterpret_cast<const u32*>(m0_s)); // ok reinterpret cast
    simd::simd_u32x4 m0_c_v = simd::load_u32_4(reinterpret_cast<const u32*>(m0_c)); // ok reinterpret cast

    // Phase 3: SIMD quadratic interpolation for sin
    // m0 = (m0 ^ dmask) - dmask (conditional negate)
    m0_s_v = simd::sub_i32_4(simd::xor_u32_4(m0_s_v, sdmask_vec), sdmask_vec);
    // c = y1 - y0 - m0
    simd::simd_u32x4 c_s = simd::sub_i32_4(simd::sub_i32_4(y1_s_v, y0_s_v), m0_s_v);
    // r = (c * t >> 16) + m0
    simd::simd_u32x4 r_s = simd::add_i32_4(simd::mulhi_i32_4(c_s, t_vec), m0_s_v);
    // result = (r * t >> 16) + y0
    simd::simd_u32x4 s_raw = simd::add_i32_4(simd::mulhi_i32_4(r_s, t_vec), y0_s_v);

    // SIMD quadratic interpolation for cos
    m0_c_v = simd::sub_i32_4(simd::xor_u32_4(m0_c_v, cdmask_vec), cdmask_vec);
    simd::simd_u32x4 c_c = simd::sub_i32_4(simd::sub_i32_4(y1_c_v, y0_c_v), m0_c_v);
    simd::simd_u32x4 r_c = simd::add_i32_4(simd::mulhi_i32_4(c_c, t_vec), m0_c_v);
    simd::simd_u32x4 c_raw = simd::add_i32_4(simd::mulhi_i32_4(r_c, t_vec), y0_c_v);

    // Apply sign masks (branchless negate: (val ^ mask) - mask)
    SinCos32_simd result;
    result.sin_vals = simd::sub_i32_4(simd::xor_u32_4(s_raw, svmask_vec), svmask_vec);
    result.cos_vals = simd::sub_i32_4(simd::xor_u32_4(c_raw, cvmask_vec), cvmask_vec);
    return result;
}

} // namespace fl
