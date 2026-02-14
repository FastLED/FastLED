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
/// SIMD-optimized: vectorized angle decomposition, vector LUT loads with
/// 4x4 transpose (AoS→SoA), and vectorized quadratic interpolation.
///
/// @param angles 4 u32 angles (0 to 16777216 per angle is a full circle)
/// @return SinCos32_simd with raw i32 values (range: -2147418112 to 2147418112)
FASTLED_FORCE_INLINE SinCos32_simd sincos32_simd(simd::simd_u32x4 angles) {
    // ========== Phase 1a: SIMD angle decomposition ==========
    // Break down each of the 4 angles into components needed for LUT lookup

    // Extract high 8 bits (angle >> 16) → 0..255 range for quadrant + position
    simd::simd_u32x4 angle256_vec = simd::srl_u32_4(angles, 16);

    // Extract low 16 bits → fractional part for interpolation (0..65535)
    simd::simd_u32x4 t_vec = simd::and_u32_4(angles, simd::set1_u32_4(0xFFFF));

    // Determine quadrant (0=0°-90°, 1=90°-180°, 2=180°-270°, 3=270°-360°)
    simd::simd_u32x4 quadrant_vec = simd::srl_u32_4(angle256_vec, 6);

    // Position within quadrant (0..63) → maps to LUT indices 0..63
    simd::simd_u32x4 pos_vec = simd::and_u32_4(angle256_vec, simd::set1_u32_4(0x3F));

    // Mirror flag: quadrants 1 and 3 need mirrored LUT access
    simd::simd_u32x4 mirror_s_vec = simd::and_u32_4(quadrant_vec, simd::set1_u32_4(1));

    // ========== Phase 1b: SIMD mask computation ==========
    // Compute masks for branchless conditional negation in interpolation and sign application

    // Sin derivative mask: 0x00000000 (direct) or 0xFFFFFFFF (negate derivative in mirrored quadrants)
    simd::simd_u32x4 sdmask_vec = simd::sub_i32_4(simd::set1_u32_4(0), mirror_s_vec);

    // Cos derivative mask: opposite of sin (cos mirrors opposite direction)
    simd::simd_u32x4 cdmask_vec = simd::xor_u32_4(sdmask_vec, simd::set1_u32_4(0xFFFFFFFF));

    // Sin value sign mask: negative in quadrants 2 and 3 (bit 1 of quadrant)
    simd::simd_u32x4 quadrant_bit1 = simd::and_u32_4(simd::srl_u32_4(quadrant_vec, 1), simd::set1_u32_4(1));
    simd::simd_u32x4 svmask_vec = simd::sub_i32_4(simd::set1_u32_4(0), quadrant_bit1);

    // Cos value sign mask: negative in quadrants 1 and 2 (XOR of quadrant bits 0 and 1)
    simd::simd_u32x4 quadrant_xor = simd::xor_u32_4(quadrant_vec, simd::srl_u32_4(quadrant_vec, 1));
    simd::simd_u32x4 quadrant_xor_bit0 = simd::and_u32_4(quadrant_xor, simd::set1_u32_4(1));
    simd::simd_u32x4 cvmask_vec = simd::sub_i32_4(simd::set1_u32_4(0), quadrant_xor_bit0);

    // ========== Phase 1c: Vectorized quarter-wave index mapping ==========
    // Map position to LUT index using quarter-wave symmetry (branchless mirror logic)

    // Compute qi = pos + mirror * (64 - 2*pos)
    // This mirrors the position for odd quadrants: pos 0→64, pos 63→1
    simd::simd_u32x4 two_pos = simd::add_i32_4(pos_vec, pos_vec);
    simd::simd_u32x4 term_vec = simd::sub_i32_4(simd::set1_u32_4(64), two_pos);
    simd::simd_u32x4 masked_term = simd::and_u32_4(term_vec, sdmask_vec);  // Zero term if not mirrored
    simd::simd_u32x4 qi_s_vec = simd::add_i32_4(pos_vec, masked_term);

    // Compute qi_next = qi + 1 - 2*mirror
    // Direct: qi_next = qi + 1 (forward), Mirrored: qi_next = qi - 1 (backward)
    simd::simd_u32x4 two_mirror_s = simd::add_i32_4(mirror_s_vec, mirror_s_vec);
    simd::simd_u32x4 qi_next_s_vec = simd::sub_i32_4(simd::add_i32_4(qi_s_vec, simd::set1_u32_4(1)), two_mirror_s);

    // ========== Phase 2: Vector LUT loads + 4x4 transpose (AoS → SoA) ==========
    // Each LUT entry is 16 bytes: {y_sin, m_sin, y_cos, m_cos} (4 × i32)
    // We load 4 full entries (one per angle) and transpose to separate vectors
    // Input:  4 rows × 4 columns (AoS: each row is one angle's data)
    // Output: 4 columns as separate vectors (SoA: each vector contains same field for 4 angles)

    // Extract indices from SIMD registers for scalar LUT indexing
    // (SIMD gather is not available on all platforms, so we use scalar indexing)
    u32 qi0 = simd::extract_u32_4(qi_s_vec, 0);
    u32 qi1 = simd::extract_u32_4(qi_s_vec, 1);
    u32 qi2 = simd::extract_u32_4(qi_s_vec, 2);
    u32 qi3 = simd::extract_u32_4(qi_s_vec, 3);

    u32 qn0 = simd::extract_u32_4(qi_next_s_vec, 0);
    u32 qn1 = simd::extract_u32_4(qi_next_s_vec, 1);
    u32 qn2 = simd::extract_u32_4(qi_next_s_vec, 2);
    u32 qn3 = simd::extract_u32_4(qi_next_s_vec, 3);

    // Load full 16-byte qi entries: {y_sin, m_sin, y_cos, m_cos}
    // e0 = {y_s0, m_s0, y_c0, m_c0}, e1 = {y_s1, m_s1, y_c1, m_c1}, ...
    simd::simd_u32x4 e0 = simd::load_u32_4(reinterpret_cast<const u32*>(&sinCosPairedLut[qi0 * 4])); // ok reinterpret cast
    simd::simd_u32x4 e1 = simd::load_u32_4(reinterpret_cast<const u32*>(&sinCosPairedLut[qi1 * 4])); // ok reinterpret cast
    simd::simd_u32x4 e2 = simd::load_u32_4(reinterpret_cast<const u32*>(&sinCosPairedLut[qi2 * 4])); // ok reinterpret cast
    simd::simd_u32x4 e3 = simd::load_u32_4(reinterpret_cast<const u32*>(&sinCosPairedLut[qi3 * 4])); // ok reinterpret cast

    // 4x4 transpose qi entries: AoS {y_s, m_s, y_c, m_c} → SoA
    // Classic SIMD matrix transpose using interleaved unpacks
    // Step 1: interleave pairs at 32-bit granularity
    simd::simd_u32x4 t01lo = simd::unpacklo_u32_4(e0, e1);  // {y_s0, y_s1, m_s0, m_s1}
    simd::simd_u32x4 t01hi = simd::unpackhi_u32_4(e0, e1);  // {y_c0, y_c1, m_c0, m_c1}
    simd::simd_u32x4 t23lo = simd::unpacklo_u32_4(e2, e3);  // {y_s2, y_s3, m_s2, m_s3}
    simd::simd_u32x4 t23hi = simd::unpackhi_u32_4(e2, e3);  // {y_c2, y_c3, m_c2, m_c3}
    // Step 2: final interleave at 64-bit granularity → complete SoA vectors
    simd::simd_u32x4 y0_s_v = simd::unpacklo_u64_as_u32_4(t01lo, t23lo);  // {y_s0, y_s1, y_s2, y_s3}
    simd::simd_u32x4 m0_s_v = simd::unpackhi_u64_as_u32_4(t01lo, t23lo);  // {m_s0, m_s1, m_s2, m_s3}
    simd::simd_u32x4 y0_c_v = simd::unpacklo_u64_as_u32_4(t01hi, t23hi);  // {y_c0, y_c1, y_c2, y_c3}
    simd::simd_u32x4 m0_c_v = simd::unpackhi_u64_as_u32_4(t01hi, t23hi);  // {m_c0, m_c1, m_c2, m_c3}

    // Load full 16-byte qi_next entries (only need y_sin and y_cos for interpolation endpoint)
    simd::simd_u32x4 n0 = simd::load_u32_4(reinterpret_cast<const u32*>(&sinCosPairedLut[qn0 * 4])); // ok reinterpret cast
    simd::simd_u32x4 n1 = simd::load_u32_4(reinterpret_cast<const u32*>(&sinCosPairedLut[qn1 * 4])); // ok reinterpret cast
    simd::simd_u32x4 n2 = simd::load_u32_4(reinterpret_cast<const u32*>(&sinCosPairedLut[qn2 * 4])); // ok reinterpret cast
    simd::simd_u32x4 n3 = simd::load_u32_4(reinterpret_cast<const u32*>(&sinCosPairedLut[qn3 * 4])); // ok reinterpret cast

    // Partial transpose qi_next: only extract y_sin and y_cos (derivatives not needed)
    simd::simd_u32x4 n01lo = simd::unpacklo_u32_4(n0, n1);  // {y_s0, y_s1, m_s0, m_s1}
    simd::simd_u32x4 n01hi = simd::unpackhi_u32_4(n0, n1);  // {y_c0, y_c1, m_c0, m_c1}
    simd::simd_u32x4 n23lo = simd::unpacklo_u32_4(n2, n3);  // {y_s2, y_s3, m_s2, m_s3}
    simd::simd_u32x4 n23hi = simd::unpackhi_u32_4(n2, n3);  // {y_c2, y_c3, m_c2, m_c3}
    simd::simd_u32x4 y1_s_v = simd::unpacklo_u64_as_u32_4(n01lo, n23lo);  // {y1_s0, y1_s1, y1_s2, y1_s3}
    simd::simd_u32x4 y1_c_v = simd::unpacklo_u64_as_u32_4(n01hi, n23hi);  // {y1_c0, y1_c1, y1_c2, y1_c3}

    // ========== Phase 3: SIMD quadratic interpolation ==========
    // Vectorized version of scalar interpolation formula from sin32_interp()
    // P(t) = y0 + T*(m0 + T*(y1 - y0 - m0))  where T = t/65536
    // Implemented as: c = y1 - y0 - m0; r = c*T + m0; result = r*T + y0
    //
    // Optimization: Uses mulhi_su32_4 (signed × unsigned-positive, 11 ops on SSE2)
    // instead of mulhi_i32_4 (14 ops on SSE2). Safe because t_vec is always in [0, 65535].

    // Sin interpolation
    // Apply derivative mask: conditionally negate m0 for mirrored quadrants
    m0_s_v = simd::sub_i32_4(simd::xor_u32_4(m0_s_v, sdmask_vec), sdmask_vec);  // (m0 ^ mask) - mask
    // Compute quadratic coefficient: c = y1 - y0 - m0
    simd::simd_u32x4 c_s = simd::sub_i32_4(simd::sub_i32_4(y1_s_v, y0_s_v), m0_s_v);
    // First multiply: r = c*t/65536 + m0  (linear term + derivative)
    simd::simd_u32x4 r_s = simd::add_i32_4(simd::mulhi_su32_4(c_s, t_vec), m0_s_v);
    // Second multiply: result = r*t/65536 + y0  (quadratic interpolation complete)
    simd::simd_u32x4 s_raw = simd::add_i32_4(simd::mulhi_su32_4(r_s, t_vec), y0_s_v);

    // Cos interpolation (identical structure to sin, but uses cos-specific masks and data)
    m0_c_v = simd::sub_i32_4(simd::xor_u32_4(m0_c_v, cdmask_vec), cdmask_vec);  // Conditionally negate m0
    simd::simd_u32x4 c_c = simd::sub_i32_4(simd::sub_i32_4(y1_c_v, y0_c_v), m0_c_v);  // c = y1 - y0 - m0
    simd::simd_u32x4 r_c = simd::add_i32_4(simd::mulhi_su32_4(c_c, t_vec), m0_c_v);   // r = c*t/65536 + m0
    simd::simd_u32x4 c_raw = simd::add_i32_4(simd::mulhi_su32_4(r_c, t_vec), y0_c_v); // result = r*t/65536 + y0

    // ========== Apply final sign masks ==========
    // Use branchless negate: (val ^ mask) - mask
    // If mask = 0x00000000: val unchanged
    // If mask = 0xFFFFFFFF: val negated (two's complement)
    SinCos32_simd result;
    result.sin_vals = simd::sub_i32_4(simd::xor_u32_4(s_raw, svmask_vec), svmask_vec);  // Negate if quadrant 2 or 3
    result.cos_vals = simd::sub_i32_4(simd::xor_u32_4(c_raw, cvmask_vec), cvmask_vec);  // Negate if quadrant 1 or 2
    return result;
}

} // namespace fl
