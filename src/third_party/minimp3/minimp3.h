#ifndef MINIMP3_H
#define MINIMP3_H
// SPDX-License-Identifier: CC0-1.0
/*
    https://github.com/lieff/minimp3
    To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights to this software to the public domain worldwide.
    This software is distributed without any warranty.
    See <http://creativecommons.org/publicdomain/zero/1.0/>.
*/
#include "fl/stl/cstring.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/stdint.h"

#define MINIMP3_MAX_SAMPLES_PER_FRAME (1152*2)


/* FastLED: fixed-point mode. MINIMP3_HAVE_FIXED_POINT reports whether this
   instantiation actually built the integer DSP path. The macro is recomputed on
   every (re-)inclusion so that a float variant and a fixed variant can coexist
   in one translation unit.

   HOW TO TURN IT ON, AND HOW NOT TO. The selection changes MINIMP3_SCRATCH_SIZE
   and the layout of the scratch arena, so every translation unit that sees this
   header must agree on it. Build-wide, that means defining
   FASTLED_MP3_FIXED_POINT as a compiler flag, which is why it is honoured here:
   a flag reaches every TU, and a #define in one .cpp does not.

   Defining MINIMP3_FIXED_POINT directly in a single translation unit -- the
   unity build, say -- while `fl/codec/mp3.cpp.hpp` includes this header without
   it gives the two a different sizeof(mp3dec_scratch_t). The caller then
   allocates the smaller arena and the decoder writes the larger one, and the
   symptom is a glibc "malloc.c: assertion failed" from somewhere unrelated
   rather than anything pointing at MP3. Confirmed by doing exactly that.

   The test harness in tests/fl/codec/minimp3_variants.hpp is the one safe
   exception: it sets the macro per-inclusion but also renames MINIMP3_NAMESPACE,
   so its types are distinct from production's and never meet them. */
#if defined(FASTLED_MP3_FIXED_POINT) && !defined(MINIMP3_FIXED_POINT)
#define MINIMP3_FIXED_POINT 1
#endif

#undef MINIMP3_HAVE_FIXED_POINT
#if defined(MINIMP3_FIXED_POINT)
#define MINIMP3_HAVE_FIXED_POINT 1
#else
#define MINIMP3_HAVE_FIXED_POINT 0
#endif

/* FastLED: 1 once the integer kernels have actually replaced the float
   pipeline for this instantiation. Deliberately separate from
   MINIMP3_HAVE_FIXED_POINT: the golden gate has to be able to tell "this build
   accepted -DMINIMP3_FIXED_POINT" apart from "this build decodes with integer
   arithmetic", and during the staged conversion those are not the same thing.
   The fixed-point kernels flip this to MINIMP3_HAVE_FIXED_POINT. */
#undef MINIMP3_DSP_INTEGER
#define MINIMP3_DSP_INTEGER MINIMP3_HAVE_FIXED_POINT

/* Scratch arena size. The fixed-point build needs a little more than the float
   build because scalefactor gains are carried as mantissa+exponent rather than
   as a single float, so it gets its own figure rather than inflating the
   shipping float decoder's working-RAM ledger for a variant it does not use.
   A static_assert on mp3dec_scratch_internal_t below enforces both. */
#undef MINIMP3_SCRATCH_SIZE
#if MINIMP3_HAVE_FIXED_POINT
#define MINIMP3_SCRATCH_SIZE 7936
#else
#define MINIMP3_SCRATCH_SIZE 7808
#endif

/* FastLED: number of fractional bits in a fixed-point DSP sample.

   Measured (tests/fl/codec/mp3_fixed_point.hpp, "stage dynamic range") the
   float pipeline peaks near 0.65 at every stage across the conformance corpus
   and real encoded music -- minimp3 normalises internally and the polyphase
   window carries the scaling back up to int16 only at the very end. So one Q
   format serves the whole pipeline and block floating point is only needed
   where the dynamic range genuinely is unbounded: the scalefactor gains and
   x**(4/3), which are carried as mantissa+exponent instead.

   Q26 in int32 leaves range +/-32 (about 50x the measured peak, and dequantised
   samples are clamped to +/-1 besides) and a resolution of 2**-26, which is
   roughly 1/1000 of an int16 LSB at output scale. Every multiply widens to
   int64 first, so this is the only headroom that has to be reasoned about. */
#undef MINIMP3_FRAC_BITS
#define MINIMP3_FRAC_BITS 26

/* FastLED: per-stage observation hook. Define MINIMP3_STAGE_DUMP to a callable
   accepting (int stage, int channel, const T *buf, int count) where T is the
   variant's DSP sample type. The golden harness uses it to compare the fixed
   and float pipelines one stage at a time, which is what localizes a numeric
   regression to a single kernel instead of to "the decoder". */
#define MINIMP3_STAGE_HUFFMAN   0
#define MINIMP3_STAGE_STEREO    1
#define MINIMP3_STAGE_ANTIALIAS 2
#define MINIMP3_STAGE_IMDCT     3
#define MINIMP3_STAGE_DCT2      4
#define MINIMP3_STAGE_COUNT     5

/* FastLED: the decoder is instantiated more than once in the same program so
   that the fixed-point build can be compared against the float build inside a
   single test binary. Each instantiation lands in its own C++ namespace, so
   the header carries no C linkage: `extern "C"` ignores namespaces and the
   variants would collide at link time. Re-include with MINIMP3_H and
   _MINIMP3_IMPLEMENTATION_GUARD undefined and MINIMP3_NAMESPACE set to a
   fresh name to build another variant. */
#ifndef MINIMP3_NAMESPACE
#define MINIMP3_NAMESPACE third_party
#endif

#ifdef __cplusplus
namespace fl {
namespace MINIMP3_NAMESPACE {
#endif

typedef struct
{
    int frame_bytes, frame_offset, channels, hz, layer, bitrate_kbps;
} mp3dec_frame_info_t;

/* Sample type flowing through the DSP pipeline: Q(MINIMP3_FRAC_BITS) integers
   in the fixed-point build, plain floats otherwise. Both are 4 bytes wide, so
   the persistent decoder state below keeps its size either way. */
#if MINIMP3_HAVE_FIXED_POINT
typedef int32_t mp3d_dsp_t;
#else
typedef float mp3d_dsp_t;
#endif

typedef struct mp3dec_t
{
    mp3d_dsp_t mdct_overlap[2][9*32], qmf_state[(15 + 18)*2*32];
    int reserv, free_format_bytes;
    unsigned char header[4], reserv_buf[511];
} mp3dec_t;

typedef union mp3dec_scratch_t
{
    uint64_t alignment;
    uint8_t buffer[MINIMP3_SCRATCH_SIZE];
} mp3dec_scratch_t;

void mp3dec_init(mp3dec_t *dec) FL_NO_EXCEPT;
/* 1 when this instantiation was asked for the integer DSP path, 0 for float. */
int mp3dec_is_fixed_point(void) FL_NO_EXCEPT;
/* 1 when this instantiation actually decodes with integer arithmetic. */
int mp3dec_dsp_is_integer(void) FL_NO_EXCEPT;
/* 1 when this instantiation compiled integer SIMD kernels. Reported
   separately from the two above for the same reason they are separate from
   each other: "the build accepted the flag" and "the build actually vectorised"
   are different claims, and only the second is worth gating on. */
int mp3dec_dsp_uses_simd(void) FL_NO_EXCEPT;
#ifndef MINIMP3_FLOAT_OUTPUT
typedef int16_t mp3d_sample_t;
#else /* MINIMP3_FLOAT_OUTPUT */
typedef float mp3d_sample_t;
void mp3dec_f32_to_s16(const float *in, int16_t *out, int num_samples) FL_NO_EXCEPT;
#endif /* MINIMP3_FLOAT_OUTPUT */
int mp3dec_decode_frame_r(mp3dec_t *dec, mp3dec_scratch_t *scratch,
                          const uint8_t *mp3, int mp3_bytes,
                          mp3d_sample_t *pcm,
                          mp3dec_frame_info_t *info) FL_NO_EXCEPT;

#ifdef __cplusplus
} /* namespace MINIMP3_NAMESPACE */
} /* namespace fl */
#endif /* __cplusplus */

#endif /* MINIMP3_H */
#if defined(MINIMP3_IMPLEMENTATION) && !defined(_MINIMP3_IMPLEMENTATION_GUARD)
#define _MINIMP3_IMPLEMENTATION_GUARD

#ifdef __cplusplus
namespace fl {
namespace MINIMP3_NAMESPACE {
#endif

#define MAX_FREE_FORMAT_FRAME_SIZE  2304    /* more than ISO spec's */
#ifndef MAX_FRAME_SYNC_MATCHES
#define MAX_FRAME_SYNC_MATCHES      10
#endif /* MAX_FRAME_SYNC_MATCHES */

#define MAX_L3_FRAME_PAYLOAD_BYTES  MAX_FREE_FORMAT_FRAME_SIZE /* MUST be >= 320000/8/32000*1152 = 1440 */

#define MAX_BITRESERVOIR_BYTES      511
#define SHORT_BLOCK_TYPE            2
#define STOP_BLOCK_TYPE             3
#define MODE_MONO                   3
#define MODE_JOINT_STEREO           1
#define HDR_SIZE                    4
#define HDR_IS_MONO(h)              (((h[3]) & 0xC0) == 0xC0)
#define HDR_IS_MS_STEREO(h)         (((h[3]) & 0xE0) == 0x60)
#define HDR_IS_FREE_FORMAT(h)       (((h[2]) & 0xF0) == 0)
#define HDR_IS_CRC(h)               (!((h[1]) & 1))
#define HDR_TEST_PADDING(h)         ((h[2]) & 0x2)
#define HDR_TEST_MPEG1(h)           ((h[1]) & 0x8)
#define HDR_TEST_NOT_MPEG25(h)      ((h[1]) & 0x10)
#define HDR_TEST_I_STEREO(h)        ((h[3]) & 0x10)
#define HDR_TEST_MS_STEREO(h)       ((h[3]) & 0x20)
#define HDR_GET_STEREO_MODE(h)      (((h[3]) >> 6) & 3)
#define HDR_GET_STEREO_MODE_EXT(h)  (((h[3]) >> 4) & 3)
#define HDR_GET_LAYER(h)            (((h[1]) >> 1) & 3)
#define HDR_GET_BITRATE(h)          ((h[2]) >> 4)
#define HDR_GET_SAMPLE_RATE(h)      (((h[2]) >> 2) & 3)
#define HDR_GET_MY_SAMPLE_RATE(h)   (HDR_GET_SAMPLE_RATE(h) + (((h[1] >> 3) & 1) + ((h[1] >> 4) & 1))*3)
#define HDR_IS_FRAME_576(h)         ((h[1] & 14) == 2)
#define HDR_IS_LAYER_1(h)           ((h[1] & 6) == 6)

#define BITS_DEQUANTIZER_OUT        -1
#define MAX_SCF                     (255 + BITS_DEQUANTIZER_OUT*4 - 210)
#define MAX_SCFI                    ((MAX_SCF + 3) & ~3)

#define MINIMP3_MIN(a, b)           ((a) > (b) ? (b) : (a))
#define MINIMP3_MAX(a, b)           ((a) < (b) ? (b) : (a))

#if MINIMP3_HAVE_FIXED_POINT
#if defined(MINIMP3_FLOAT_OUTPUT)
#error "MINIMP3_FIXED_POINT and MINIMP3_FLOAT_OUTPUT are mutually exclusive"
#endif
#include "third_party/minimp3/minimp3_fixed_tables.h" // ok cpp include

/* FastLED: the fixed-point arithmetic contract.

   ONE rounding rule for the whole decoder: add half, then arithmetic-shift
   right. Because the shift floors, that is round-half-toward-+infinity, NOT
   round-half-away-from-zero -- -1.5 rounds to -1. Stated precisely because
   Phase 4 (#4055) has to prove SSE2/NEON kernels bit-identical to this scalar
   path, and it must implement this rule rather than the one the phrase
   "rounding" usually implies. It is also the cheaper rule to vectorise, which
   is why it is worth keeping rather than "correcting".

   (Two deliberate exceptions, both documented at their definitions: the Python
   table generator rounds half away from zero, because it runs once at build
   time and symmetry matters for a constant; and mp3d_scale_pcm reproduces the
   float build's own output rounding quirk so the two builds' PCM agrees.)

   Every product widens to int64 before shifting, so a 32x32 multiply can never
   overflow. Shifts are written to keep UBSan quiet: no left shift of a
   negative value anywhere (those go through MP3D_SHL, which multiplies), and
   every variable shift distance is clamped to [0, 63] by its caller. */
/* Symmetric on purpose: L3_change_sign negates samples in place, and negating
   INT32_MIN is signed-overflow UB. Giving up one representable value buys a
   pipeline where every sample can be negated unconditionally. */
#define MP3D_SAT_MAX ((int32_t)0x7fffffff)
#define MP3D_SAT_MIN (-MP3D_SAT_MAX)

/* Saturating narrow of a 64-bit accumulator to a Q(MINIMP3_FRAC_BITS) sample. */
static int32_t mp3d_sat64(int64_t value) FL_NO_EXCEPT
{
    if (value > MP3D_SAT_MAX) return MP3D_SAT_MAX;
    if (value < MP3D_SAT_MIN) return MP3D_SAT_MIN;
    return (int32_t)value;
}

/* value * coefficient, where the coefficient is Q`bits`.

   Saturates on the way down to 32 bits. The coefficients are not all <= 1 --
   the DCT-32 secants reach 10.19 -- so the product can leave int32 range for an
   operand far below MP3D_SAT_MAX, which means the saturating adds around it
   never get the chance to fire. Concretely, at Q27 a 10.19 secant wraps once
   the operand passes about 3.14 in real units, an order of magnitude under the
   format's +/-32; a mutated stream that drives dequantisation to the +/-1 clamp
   reaches that comfortably by the time mid/side, antialias and the IMDCT have
   each added gain. Wrapping there produces a sign-flipped full-scale sample --
   an audible bang instead of a bounded clip.

   Worth stating why this cannot be left to the fuzz gate: narrowing an int64 to
   int32 is implementation-defined, not undefined, so UBSan does not see it. */
static int32_t mp3d_mulshift(int32_t value, int32_t coef, int bits) FL_NO_EXCEPT
{
    int64_t product = (int64_t)value * (int64_t)coef;
    product += (int64_t)1 << (bits - 1);
    return mp3d_sat64(product >> bits);
}

/* Left shift that is defined for negative inputs, with saturation. `shift` is
   assumed to be in [0, 62]; callers clamp before calling. */
static int32_t mp3d_shl_sat(int32_t value, int shift) FL_NO_EXCEPT
{
    int64_t wide;
    if (shift >= 32)
    {
        return value == 0 ? 0 : (value > 0 ? MP3D_SAT_MAX : MP3D_SAT_MIN);
    }
    wide = (int64_t)value * ((int64_t)1 << shift);
    if (wide > MP3D_SAT_MAX) return MP3D_SAT_MAX;
    if (wide < MP3D_SAT_MIN) return MP3D_SAT_MIN;
    return (int32_t)wide;
}

/* Arithmetic right shift with the same round-half-toward-+infinity rule,
   tolerating shifts that discard the value entirely. */
static int32_t mp3d_shr_round(int32_t value, int shift) FL_NO_EXCEPT
{
    if (shift <= 0)
    {
        return value;
    }
    if (shift > 31)
    {
        return 0;
    }
    return (int32_t)(((int64_t)value + ((int64_t)1 << (shift - 1))) >> shift);
}

/* Narrow a Q30 accumulator to a sample, rounding once (half toward +infinity,
   as above) for the whole accumulation rather than once per product. */
static int32_t mp3d_narrow_q30(int64_t acc) FL_NO_EXCEPT
{
    return mp3d_sat64((acc + ((int64_t)1 << 29)) >> 30);
}

/* Saturating add/subtract, computed entirely in 32 bits.

   These run tens of times per butterfly in the DCT-32 and IMDCT, which is the
   hot path on exactly the 32-bit MCUs this path exists for, and there a 64-bit
   add costs a register pair and a carry chain. The overflow test is the
   standard one: a signed add overflows exactly when both operands share a sign
   that the result does not, which is what `(a ^ sum) & (b ^ sum) < 0` says. The
   wrapping sum is computed on unsigned operands, where wraparound is defined
   rather than UB.

   The trailing check preserves the previous behaviour exactly: the saturation
   range is symmetric (see MP3D_SAT_MIN), so INT32_MIN is out of range even
   though it does not overflow a 32-bit add. */
static int32_t mp3d_add_sat(int32_t a, int32_t b) FL_NO_EXCEPT
{
    const int32_t sum = (int32_t)((uint32_t)a + (uint32_t)b);
    if (((a ^ sum) & (b ^ sum)) < 0)
    {
        return a < 0 ? MP3D_SAT_MIN : MP3D_SAT_MAX;
    }
    return sum < MP3D_SAT_MIN ? MP3D_SAT_MIN : sum;
}

static int32_t mp3d_sub_sat(int32_t a, int32_t b) FL_NO_EXCEPT
{
    const int32_t diff = (int32_t)((uint32_t)a - (uint32_t)b);
    if (((a ^ b) & (a ^ diff)) < 0)
    {
        return a < 0 ? MP3D_SAT_MIN : MP3D_SAT_MAX;
    }
    return diff < MP3D_SAT_MIN ? MP3D_SAT_MIN : diff;
}

/* One Q26 unit of 1.0, and the clamp applied to dequantised samples.

   Clamping to +/-1.0 rather than to the Q format's +/-32 is deliberate. The
   measured legitimate peak is 0.65, so this costs nothing on real streams, and
   it is what bounds every downstream butterfly: a fuzzed bitstream can ask for
   an astronomically large dequantised value, and without this the DCT-32
   secants (up to 10.19) stacked on three levels of adds would overflow int32
   and hand UBSan a signed-overflow report. */
#define MP3D_ONE ((int32_t)1 << MINIMP3_FRAC_BITS)

static int32_t mp3d_clamp_sample(int64_t value) FL_NO_EXCEPT
{
    if (value > MP3D_ONE) return MP3D_ONE;
    if (value < -MP3D_ONE) return -MP3D_ONE;
    return (int32_t)value;
}

/* Combine a mantissa/exponent pair (value = mant * 2**(exp - 30)) into a
   Q(MINIMP3_FRAC_BITS) sample, clamped. */
static int32_t mp3d_scale_to_q(int32_t mant, int exp) FL_NO_EXCEPT
{
    const int shift = 30 - MINIMP3_FRAC_BITS - exp;
    if (mant == 0)
    {
        return 0;
    }
    if (shift > 0)
    {
        return mp3d_clamp_sample(mp3d_shr_round(mant, shift));
    }
    return mp3d_clamp_sample((int64_t)mp3d_shl_sat(mant, -shift));
}
#endif /* MINIMP3_HAVE_FIXED_POINT */

/* FastLED: see MINIMP3_STAGE_* in the public section. Compiles away entirely
   unless the including translation unit asked for stage observation. */
#ifdef MINIMP3_STAGE_DUMP
#define MP3D_STAGE(stage, ch, buf, n) MINIMP3_STAGE_DUMP((stage), (ch), (buf), (n))
#else
#define MP3D_STAGE(stage, ch, buf, n) ((void)0)
#endif

/* FastLED: the vector kernels come in two families that must not be confused.
   The `HAVE_SIMD` block below is the float one, and it stays exactly as
   upstream wrote it -- the fixed-point build must never compile float vector
   code. The integer kernels live in their own MP3D_HAVE_INT_SIMD block and are
   selected only for the fixed build. */
#if MINIMP3_HAVE_FIXED_POINT
/* Keep upstream's float SIMD out of the fixed build without disturbing the
   caller's own MINIMP3_NO_SIMD, which still has to mean "scalar everywhere"
   and is what the Phase 4 opt-out proof relies on. */
#define MP3D_FLOAT_SIMD_OFF
#endif

#if !defined(MINIMP3_NO_SIMD) && !defined(MP3D_FLOAT_SIMD_OFF)

#if !defined(MINIMP3_ONLY_SIMD) && (defined(_M_X64) || defined(__x86_64__) || defined(__aarch64__) || defined(_M_ARM64))
/* x64 always have SSE2, arm64 always have neon, no need for generic code */
#define MINIMP3_ONLY_SIMD
#endif /* SIMD checks... */

#if (defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))) || ((defined(__i386__) || defined(__x86_64__)) && defined(__SSE2__))
#if defined(_MSC_VER)
#include <intrin.h>
#endif /* defined(_MSC_VER) */
#include <immintrin.h>
#define HAVE_SSE 1
#define HAVE_SIMD 1
#define VSTORE _mm_storeu_ps
#define VLD _mm_loadu_ps
#define VSET _mm_set1_ps
#define VADD _mm_add_ps
#define VSUB _mm_sub_ps
#define VMUL _mm_mul_ps
#define VMAC(a, x, y) _mm_add_ps(a, _mm_mul_ps(x, y))
#define VMSB(a, x, y) _mm_sub_ps(a, _mm_mul_ps(x, y))
#define VMUL_S(x, s)  _mm_mul_ps(x, _mm_set1_ps(s))
#define VREV(x) _mm_shuffle_ps(x, x, _MM_SHUFFLE(0, 1, 2, 3))
typedef __m128 f4;
#if defined(_MSC_VER) || defined(MINIMP3_ONLY_SIMD)
#define minimp3_cpuid __cpuid
#else /* defined(_MSC_VER) || defined(MINIMP3_ONLY_SIMD) */
static __inline__ __attribute__((always_inline)) void minimp3_cpuid(int CPUInfo[], const int InfoType)
{
#if defined(__PIC__)
    __asm__ __volatile__(
#if defined(__x86_64__)
        "push %%rbx\n"
        "cpuid\n"
        "xchgl %%ebx, %1\n"
        "pop  %%rbx\n"
#else /* defined(__x86_64__) */
        "xchgl %%ebx, %1\n"
        "cpuid\n"
        "xchgl %%ebx, %1\n"
#endif /* defined(__x86_64__) */
        : "=a" (CPUInfo[0]), "=r" (CPUInfo[1]), "=c" (CPUInfo[2]), "=d" (CPUInfo[3])
        : "a" (InfoType));
#else /* defined(__PIC__) */
    __asm__ __volatile__(
        "cpuid"
        : "=a" (CPUInfo[0]), "=b" (CPUInfo[1]), "=c" (CPUInfo[2]), "=d" (CPUInfo[3])
        : "a" (InfoType));
#endif /* defined(__PIC__)*/
}
#endif /* defined(_MSC_VER) || defined(MINIMP3_ONLY_SIMD) */
static int have_simd(void) FL_NO_EXCEPT
{
#ifdef MINIMP3_ONLY_SIMD
    return 1;
#else /* MINIMP3_ONLY_SIMD */
    static int g_have_simd;
    int CPUInfo[4];
#ifdef MINIMP3_TEST
    static int g_counter;
    if (g_counter++ > 100)
        return 0;
#endif /* MINIMP3_TEST */
    if (g_have_simd)
        goto end;
    minimp3_cpuid(CPUInfo, 0);
    g_have_simd = 1;
    if (CPUInfo[0] > 0)
    {
        minimp3_cpuid(CPUInfo, 1);
        g_have_simd = (CPUInfo[3] & (1 << 26)) + 1; /* SSE2 */
    }
end:
    return g_have_simd - 1;
#endif /* MINIMP3_ONLY_SIMD */
}
#elif defined(__ARM_NEON) || defined(__aarch64__) || defined(_M_ARM64)
#include <arm_neon.h>
#define HAVE_SSE 0
#define HAVE_SIMD 1
#define VSTORE vst1q_f32
#define VLD vld1q_f32
#define VSET vmovq_n_f32
#define VADD vaddq_f32
#define VSUB vsubq_f32
#define VMUL vmulq_f32
#define VMAC(a, x, y) vmlaq_f32(a, x, y)
#define VMSB(a, x, y) vmlsq_f32(a, x, y)
#define VMUL_S(x, s)  vmulq_f32(x, vmovq_n_f32(s))
#define VREV(x) vcombine_f32(vget_high_f32(vrev64q_f32(x)), vget_low_f32(vrev64q_f32(x)))
typedef float32x4_t f4;
static int have_simd()
{   /* TODO: detect neon for !MINIMP3_ONLY_SIMD */
    return 1;
}
#else /* SIMD checks... */
#define HAVE_SSE 0
#define HAVE_SIMD 0
#ifdef MINIMP3_ONLY_SIMD
#error MINIMP3_ONLY_SIMD used, but SSE/NEON not enabled
#endif /* MINIMP3_ONLY_SIMD */
#endif /* SIMD checks... */
#else /* !defined(MINIMP3_NO_SIMD) && !defined(MP3D_FLOAT_SIMD_OFF) */
#define HAVE_SIMD 0
#endif /* !defined(MINIMP3_NO_SIMD) && !defined(MP3D_FLOAT_SIMD_OFF) */

/* FastLED: integer SIMD for the fixed-point path (FastLED/FastLED#4055).
   Deliberately a separate detection block from upstream's float one above --
   the two select different instruction sets and must never both be live.
   MINIMP3_NO_SIMD suppresses this as well, which is what makes the scalar
   opt-out proof meaningful. */
#if MINIMP3_HAVE_FIXED_POINT && !defined(MINIMP3_NO_SIMD)
#if (defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))) || \
    ((defined(__i386__) || defined(__x86_64__)) && defined(__SSE2__))
#include <immintrin.h>
#if !defined(_MSC_VER) && (defined(__GNUC__) || defined(__clang__))
#include <cpuid.h>
#endif
#define MP3D_HAVE_INT_SIMD 1
#define MP3D_INT_SIMD_SSE  1
typedef __m128i mp3d_i32x4;
#elif defined(__ARM_NEON) || defined(__aarch64__) || defined(_M_ARM64)
#include <arm_neon.h>
#define MP3D_HAVE_INT_SIMD 1
#define MP3D_INT_SIMD_NEON 1
typedef int32x4_t mp3d_i32x4;
#endif
#endif /* MINIMP3_HAVE_FIXED_POINT && !MINIMP3_NO_SIMD */

#ifndef MP3D_HAVE_INT_SIMD
#define MP3D_HAVE_INT_SIMD 0
#endif

/* 1 once integer kernels actually replace scalar ones. Separate from
   MP3D_HAVE_INT_SIMD -- having the intrinsics available is not the same as
   using them, and the gate is on the second. */
#undef MP3D_SIMD_KERNELS_LIVE
#define MP3D_SIMD_KERNELS_LIVE MP3D_HAVE_INT_SIMD

#if defined(__ARM_ARCH) && (__ARM_ARCH >= 6) && !defined(__aarch64__) && !defined(_M_ARM64) && !defined(__ARM_ARCH_6M__)
#define HAVE_ARMV6 1
static __inline__ __attribute__((always_inline)) int32_t minimp3_clip_int16_arm(int32_t a)
{
    int32_t x = 0;
    __asm__ ("ssat %0, #16, %1" : "=r"(x) : "r"(a));
    return x;
}
#else
#define HAVE_ARMV6 0
#endif

typedef struct
{
    const uint8_t *buf;
    int pos, limit;
} bs_t;

typedef struct
{
#if MINIMP3_HAVE_FIXED_POINT
    /* Same mantissa/exponent split the Layer III gains use: the Layer I/II
       dequantiser steps are around 1e-7 before a runtime scale that can move
       them 21 binary places either way, so they do not fit one Q format.

       int8_t, unlike the Layer III gains' int16_t, because this array lives on
       the stack inside mp3dec_decode_frame_r rather than in the heap scratch
       arena, and 192 entries of it is the difference between meeting the 2 KiB
       decode-stack budget and missing it. The exponents here span [-37, -1]:
       the table's own range plus the runtime 2**(21 - b/3) scale. */
    int32_t scf_mant[3*64];
    int8_t scf_exp[3*64];
#else
    float scf[3*64];
#endif
    uint8_t total_bands, stereo_bands, bitalloc[64], scfcod[64];
} L12_scale_info;

typedef struct
{
    uint8_t tab_offset, code_tab_width, band_count;
} L12_subband_alloc_t;

typedef struct
{
    const uint8_t *sfbtab;
    uint16_t part_23_length, big_values, scalefac_compress;
    uint8_t global_gain, block_type, mixed_block_flag, n_long_sfb, n_short_sfb;
    uint8_t table_select[3], region_count[3], subblock_gain[3];
    uint8_t preflag, scalefac_scale, count1_table, scfsi;
} L3_gr_info_t;

typedef struct
{
    bs_t bs;
    uint8_t maindata[MAX_BITRESERVOIR_BYTES + MAX_L3_FRAME_PAYLOAD_BYTES];
    L3_gr_info_t gr_info[4];
    mp3d_dsp_t grbuf[2][576];
#if MINIMP3_HAVE_FIXED_POINT
    /* Scalefactor gains span roughly 2**-181 to 2**10, so they are the one
       quantity in the pipeline that genuinely needs an exponent of its own.
       int16 rather than int8 because that range does not fit a signed byte. */
    int32_t scf_mant[40];
    int16_t scf_exp[40];
#else
    float scf[40];
#endif
    uint8_t ist_pos[2][39];
} mp3dec_scratch_internal_t;

#ifdef __cplusplus
static_assert(sizeof(mp3dec_scratch_internal_t) <= MINIMP3_SCRATCH_SIZE,
              "MINIMP3_SCRATCH_SIZE is too small");
static_assert(alignof(mp3dec_scratch_internal_t) <= alignof(mp3dec_scratch_t),
              "mp3dec_scratch_t alignment is too small");
#endif

static void bs_init(bs_t *bs, const uint8_t *data, int bytes) FL_NO_EXCEPT
{
    bs->buf   = data;
    bs->pos   = 0;
    bs->limit = bytes*8;
}

static uint32_t get_bits(bs_t *bs, int n) FL_NO_EXCEPT
{
    uint32_t next, cache = 0, s = bs->pos & 7;
    int shl = n + s;
    const uint8_t *p = bs->buf + (bs->pos >> 3);
    if ((bs->pos += n) > bs->limit)
        return 0;
    next = *p++ & (255 >> s);
    while ((shl -= 8) > 0)
    {
        cache |= next << shl;
        next = *p++;
    }
    return cache | (next >> -shl);
}

static int hdr_valid(const uint8_t *h) FL_NO_EXCEPT
{
    return h[0] == 0xff &&
        ((h[1] & 0xF0) == 0xf0 || (h[1] & 0xFE) == 0xe2) &&
        (HDR_GET_LAYER(h) != 0) &&
        (HDR_GET_BITRATE(h) != 15) &&
        (HDR_GET_SAMPLE_RATE(h) != 3);
}

static int hdr_compare(const uint8_t *h1, const uint8_t *h2) FL_NO_EXCEPT
{
    return hdr_valid(h2) &&
        ((h1[1] ^ h2[1]) & 0xFE) == 0 &&
        ((h1[2] ^ h2[2]) & 0x0C) == 0 &&
        !(HDR_IS_FREE_FORMAT(h1) ^ HDR_IS_FREE_FORMAT(h2));
}

static unsigned hdr_bitrate_kbps(const uint8_t *h) FL_NO_EXCEPT
{
    static const uint8_t halfrate[2][3][15] = {
        { { 0,4,8,12,16,20,24,28,32,40,48,56,64,72,80 }, { 0,4,8,12,16,20,24,28,32,40,48,56,64,72,80 }, { 0,16,24,28,32,40,48,56,64,72,80,88,96,112,128 } },
        { { 0,16,20,24,28,32,40,48,56,64,80,96,112,128,160 }, { 0,16,24,28,32,40,48,56,64,80,96,112,128,160,192 }, { 0,16,32,48,64,80,96,112,128,144,160,176,192,208,224 } },
    };
    return 2*halfrate[!!HDR_TEST_MPEG1(h)][HDR_GET_LAYER(h) - 1][HDR_GET_BITRATE(h)];
}

static unsigned hdr_sample_rate_hz(const uint8_t *h) FL_NO_EXCEPT
{
    static const unsigned g_hz[3] = { 44100, 48000, 32000 };
    return g_hz[HDR_GET_SAMPLE_RATE(h)] >> (int)!HDR_TEST_MPEG1(h) >> (int)!HDR_TEST_NOT_MPEG25(h);
}

static unsigned hdr_frame_samples(const uint8_t *h) FL_NO_EXCEPT
{
    return HDR_IS_LAYER_1(h) ? 384 : (1152 >> (int)HDR_IS_FRAME_576(h));
}

static int hdr_frame_bytes(const uint8_t *h, int free_format_size) FL_NO_EXCEPT
{
    int frame_bytes = hdr_frame_samples(h)*hdr_bitrate_kbps(h)*125/hdr_sample_rate_hz(h);
    if (HDR_IS_LAYER_1(h))
    {
        frame_bytes &= ~3; /* slot align */
    }
    return frame_bytes ? frame_bytes : free_format_size;
}

static int hdr_padding(const uint8_t *h) FL_NO_EXCEPT
{
    return HDR_TEST_PADDING(h) ? (HDR_IS_LAYER_1(h) ? 4 : 1) : 0;
}

#ifndef MINIMP3_ONLY_MP3
static const L12_subband_alloc_t *L12_subband_alloc_table(const uint8_t *hdr, L12_scale_info *sci) FL_NO_EXCEPT
{
    const L12_subband_alloc_t *alloc;
    int mode = HDR_GET_STEREO_MODE(hdr);
    int nbands, stereo_bands = (mode == MODE_MONO) ? 0 : (mode == MODE_JOINT_STEREO) ? (HDR_GET_STEREO_MODE_EXT(hdr) << 2) + 4 : 32;

    if (HDR_IS_LAYER_1(hdr))
    {
        static const L12_subband_alloc_t g_alloc_L1[] = { { 76, 4, 32 } };
        alloc = g_alloc_L1;
        nbands = 32;
    } else if (!HDR_TEST_MPEG1(hdr))
    {
        static const L12_subband_alloc_t g_alloc_L2M2[] = { { 60, 4, 4 }, { 44, 3, 7 }, { 44, 2, 19 } };
        alloc = g_alloc_L2M2;
        nbands = 30;
    } else
    {
        static const L12_subband_alloc_t g_alloc_L2M1[] = { { 0, 4, 3 }, { 16, 4, 8 }, { 32, 3, 12 }, { 40, 2, 7 } };
        int sample_rate_idx = HDR_GET_SAMPLE_RATE(hdr);
        unsigned kbps = hdr_bitrate_kbps(hdr) >> (int)(mode != MODE_MONO);
        if (!kbps) /* free-format */
        {
            kbps = 192;
        }

        alloc = g_alloc_L2M1;
        nbands = 27;
        if (kbps < 56)
        {
            static const L12_subband_alloc_t g_alloc_L2M1_lowrate[] = { { 44, 4, 2 }, { 44, 3, 10 } };
            alloc = g_alloc_L2M1_lowrate;
            nbands = sample_rate_idx == 2 ? 12 : 8;
        } else if (kbps >= 96 && sample_rate_idx != 1)
        {
            nbands = 30;
        }
    }

    sci->total_bands = (uint8_t)nbands;
    sci->stereo_bands = (uint8_t)MINIMP3_MIN(stereo_bands, nbands);

    return alloc;
}

#if MINIMP3_HAVE_FIXED_POINT
/* raw quantised value * Layer I/II step, landed in Q(MINIMP3_FRAC_BITS). */
static int32_t mp3d_l12_scale(int32_t raw, int32_t mant, int exp) FL_NO_EXCEPT
{
    int64_t product = (int64_t)raw * (int64_t)mant;
    int shift;
    if (product == 0)
    {
        return 0;
    }
    shift = 30 - MINIMP3_FRAC_BITS - exp;
    if (shift >= 63)
    {
        return 0;
    }
    if (shift <= 0)
    {
        return mp3d_clamp_sample(
            (int64_t)mp3d_shl_sat(mp3d_sat64(product), -shift));
    }
    product = (product + ((int64_t)1 << (shift - 1))) >> shift;
    return mp3d_clamp_sample(product);
}

static void L12_read_scalefactors(bs_t *bs, uint8_t *pba, uint8_t *scfcod, int bands, int32_t *scf_mant, int8_t *scf_exp) FL_NO_EXCEPT
{
    int i, m;
    for (i = 0; i < bands; i++)
    {
        int32_t mant = 0;
        int exp = 0;
        int ba = *pba++;
        int mask = ba ? 4 + ((19 >> scfcod[i]) & 3) : 0;
        for (m = 4; m; m >>= 1)
        {
            if (mask & m)
            {
                const int b = get_bits(bs, 6);
                const int idx = ba*3 - 6 + b % 3;
                mant = g_deq_L12_mant[idx];
                /* The float build multiplies by (1 << 21 >> b/3); in
                   mantissa/exponent form that is purely an exponent shift. */
                exp = g_deq_L12_exp[idx] + 21 - b/3;
            }
            *scf_mant++ = mant;
            *scf_exp++ = (int8_t)exp;
        }
    }
}
#else
static void L12_read_scalefactors(bs_t *bs, uint8_t *pba, uint8_t *scfcod, int bands, float *scf) FL_NO_EXCEPT
{
    static const float g_deq_L12[18*3] = {
#define DQ(x) 9.53674316e-07f/x, 7.56931807e-07f/x, 6.00777173e-07f/x
        DQ(3),DQ(7),DQ(15),DQ(31),DQ(63),DQ(127),DQ(255),DQ(511),DQ(1023),DQ(2047),DQ(4095),DQ(8191),DQ(16383),DQ(32767),DQ(65535),DQ(3),DQ(5),DQ(9)
    };
    int i, m;
    for (i = 0; i < bands; i++)
    {
        float s = 0;
        int ba = *pba++;
        int mask = ba ? 4 + ((19 >> scfcod[i]) & 3) : 0;
        for (m = 4; m; m >>= 1)
        {
            if (mask & m)
            {
                int b = get_bits(bs, 6);
                s = g_deq_L12[ba*3 - 6 + b % 3]*(1 << 21 >> b/3);
            }
            *scf++ = s;
        }
    }
}
#endif /* MINIMP3_HAVE_FIXED_POINT */

static void L12_read_scale_info(const uint8_t *hdr, bs_t *bs, L12_scale_info *sci) FL_NO_EXCEPT
{
    static const uint8_t g_bitalloc_code_tab[] = {
        0,17, 3, 4, 5,6,7, 8,9,10,11,12,13,14,15,16,
        0,17,18, 3,19,4,5, 6,7, 8, 9,10,11,12,13,16,
        0,17,18, 3,19,4,5,16,
        0,17,18,16,
        0,17,18,19, 4,5,6, 7,8, 9,10,11,12,13,14,15,
        0,17,18, 3,19,4,5, 6,7, 8, 9,10,11,12,13,14,
        0, 2, 3, 4, 5,6,7, 8,9,10,11,12,13,14,15,16
    };
    const L12_subband_alloc_t *subband_alloc = L12_subband_alloc_table(hdr, sci);

    int i, k = 0, ba_bits = 0;
    const uint8_t *ba_code_tab = g_bitalloc_code_tab;

    for (i = 0; i < sci->total_bands; i++)
    {
        uint8_t ba;
        if (i == k)
        {
            k += subband_alloc->band_count;
            ba_bits = subband_alloc->code_tab_width;
            ba_code_tab = g_bitalloc_code_tab + subband_alloc->tab_offset;
            subband_alloc++;
        }
        ba = ba_code_tab[get_bits(bs, ba_bits)];
        sci->bitalloc[2*i] = ba;
        if (i < sci->stereo_bands)
        {
            ba = ba_code_tab[get_bits(bs, ba_bits)];
        }
        sci->bitalloc[2*i + 1] = sci->stereo_bands ? ba : 0;
    }

    for (i = 0; i < 2*sci->total_bands; i++)
    {
        sci->scfcod[i] = sci->bitalloc[i] ? HDR_IS_LAYER_1(hdr) ? 2 : get_bits(bs, 2) : 6;
    }

#if MINIMP3_HAVE_FIXED_POINT
    L12_read_scalefactors(bs, sci->bitalloc, sci->scfcod, sci->total_bands*2, sci->scf_mant, sci->scf_exp);
#else
    L12_read_scalefactors(bs, sci->bitalloc, sci->scfcod, sci->total_bands*2, sci->scf);
#endif

    for (i = sci->stereo_bands; i < sci->total_bands; i++)
    {
        sci->bitalloc[2*i + 1] = 0;
    }
}

/* Writes the raw quantised integers, exactly as the float build writes raw
   integer-valued floats. L12_apply_scf_384 is what turns them into
   Q(MINIMP3_FRAC_BITS) samples, because the step size is not known until the
   scalefactors are applied. */
static int L12_dequantize_granule(mp3d_dsp_t *grbuf, bs_t *bs, L12_scale_info *sci, int group_size) FL_NO_EXCEPT
{
    int i, j, k, choff = 576;
    for (j = 0; j < 4; j++)
    {
        mp3d_dsp_t *dst = grbuf + group_size*j;
        for (i = 0; i < 2*sci->total_bands; i++)
        {
            int ba = sci->bitalloc[i];
            if (ba != 0)
            {
                if (ba < 17)
                {
                    int half = (1 << (ba - 1)) - 1;
                    for (k = 0; k < group_size; k++)
                    {
                        dst[k] = (mp3d_dsp_t)((int)get_bits(bs, ba) - half);
                    }
                } else
                {
                    unsigned mod = (2 << (ba - 17)) + 1;    /* 3, 5, 9 */
                    unsigned code = get_bits(bs, mod + 2 - (mod >> 3));  /* 5, 7, 10 */
                    for (k = 0; k < group_size; k++, code /= mod)
                    {
                        dst[k] = (mp3d_dsp_t)((int)(code % mod - mod/2));
                    }
                }
            }
            dst += choff;
            choff = 18 - choff;
        }
    }
    return group_size*4;
}

#if MINIMP3_HAVE_FIXED_POINT
static void L12_apply_scf_384(L12_scale_info *sci, const int32_t *scf_mant, const int8_t *scf_exp, mp3d_dsp_t *dst) FL_NO_EXCEPT
{
    int i, k;
    memcpy(dst + 576 + sci->stereo_bands*18, dst + sci->stereo_bands*18, (sci->total_bands - sci->stereo_bands)*18*sizeof(mp3d_dsp_t));
    for (i = 0; i < sci->total_bands; i++, dst += 18, scf_mant += 6, scf_exp += 6)
    {
        for (k = 0; k < 12; k++)
        {
            dst[k + 0]   = mp3d_l12_scale(dst[k + 0], scf_mant[0], scf_exp[0]);
            dst[k + 576] = mp3d_l12_scale(dst[k + 576], scf_mant[3], scf_exp[3]);
        }
    }
}
#else
static void L12_apply_scf_384(L12_scale_info *sci, const float *scf, float *dst) FL_NO_EXCEPT
{
    int i, k;
    memcpy(dst + 576 + sci->stereo_bands*18, dst + sci->stereo_bands*18, (sci->total_bands - sci->stereo_bands)*18*sizeof(float));
    for (i = 0; i < sci->total_bands; i++, dst += 18, scf += 6)
    {
        for (k = 0; k < 12; k++)
        {
            dst[k + 0]   *= scf[0];
            dst[k + 576] *= scf[3];
        }
    }
}
#endif /* MINIMP3_HAVE_FIXED_POINT */
#endif /* MINIMP3_ONLY_MP3 */

static int L3_read_side_info(bs_t *bs, L3_gr_info_t *gr, const uint8_t *hdr) FL_NO_EXCEPT
{
    static const uint8_t g_scf_long[8][23] = {
        { 6,6,6,6,6,6,8,10,12,14,16,20,24,28,32,38,46,52,60,68,58,54,0 },
        { 12,12,12,12,12,12,16,20,24,28,32,40,48,56,64,76,90,2,2,2,2,2,0 },
        { 6,6,6,6,6,6,8,10,12,14,16,20,24,28,32,38,46,52,60,68,58,54,0 },
        { 6,6,6,6,6,6,8,10,12,14,16,18,22,26,32,38,46,54,62,70,76,36,0 },
        { 6,6,6,6,6,6,8,10,12,14,16,20,24,28,32,38,46,52,60,68,58,54,0 },
        { 4,4,4,4,4,4,6,6,8,8,10,12,16,20,24,28,34,42,50,54,76,158,0 },
        { 4,4,4,4,4,4,6,6,6,8,10,12,16,18,22,28,34,40,46,54,54,192,0 },
        { 4,4,4,4,4,4,6,6,8,10,12,16,20,24,30,38,46,56,68,84,102,26,0 }
    };
    static const uint8_t g_scf_short[8][40] = {
        { 4,4,4,4,4,4,4,4,4,6,6,6,8,8,8,10,10,10,12,12,12,14,14,14,18,18,18,24,24,24,30,30,30,40,40,40,18,18,18,0 },
        { 8,8,8,8,8,8,8,8,8,12,12,12,16,16,16,20,20,20,24,24,24,28,28,28,36,36,36,2,2,2,2,2,2,2,2,2,26,26,26,0 },
        { 4,4,4,4,4,4,4,4,4,6,6,6,6,6,6,8,8,8,10,10,10,14,14,14,18,18,18,26,26,26,32,32,32,42,42,42,18,18,18,0 },
        { 4,4,4,4,4,4,4,4,4,6,6,6,8,8,8,10,10,10,12,12,12,14,14,14,18,18,18,24,24,24,32,32,32,44,44,44,12,12,12,0 },
        { 4,4,4,4,4,4,4,4,4,6,6,6,8,8,8,10,10,10,12,12,12,14,14,14,18,18,18,24,24,24,30,30,30,40,40,40,18,18,18,0 },
        { 4,4,4,4,4,4,4,4,4,4,4,4,6,6,6,8,8,8,10,10,10,12,12,12,14,14,14,18,18,18,22,22,22,30,30,30,56,56,56,0 },
        { 4,4,4,4,4,4,4,4,4,4,4,4,6,6,6,6,6,6,10,10,10,12,12,12,14,14,14,16,16,16,20,20,20,26,26,26,66,66,66,0 },
        { 4,4,4,4,4,4,4,4,4,4,4,4,6,6,6,8,8,8,12,12,12,16,16,16,20,20,20,26,26,26,34,34,34,42,42,42,12,12,12,0 }
    };
    static const uint8_t g_scf_mixed[8][40] = {
        { 6,6,6,6,6,6,6,6,6,8,8,8,10,10,10,12,12,12,14,14,14,18,18,18,24,24,24,30,30,30,40,40,40,18,18,18,0 },
        { 12,12,12,4,4,4,8,8,8,12,12,12,16,16,16,20,20,20,24,24,24,28,28,28,36,36,36,2,2,2,2,2,2,2,2,2,26,26,26,0 },
        { 6,6,6,6,6,6,6,6,6,6,6,6,8,8,8,10,10,10,14,14,14,18,18,18,26,26,26,32,32,32,42,42,42,18,18,18,0 },
        { 6,6,6,6,6,6,6,6,6,8,8,8,10,10,10,12,12,12,14,14,14,18,18,18,24,24,24,32,32,32,44,44,44,12,12,12,0 },
        { 6,6,6,6,6,6,6,6,6,8,8,8,10,10,10,12,12,12,14,14,14,18,18,18,24,24,24,30,30,30,40,40,40,18,18,18,0 },
        { 4,4,4,4,4,4,6,6,4,4,4,6,6,6,8,8,8,10,10,10,12,12,12,14,14,14,18,18,18,22,22,22,30,30,30,56,56,56,0 },
        { 4,4,4,4,4,4,6,6,4,4,4,6,6,6,6,6,6,10,10,10,12,12,12,14,14,14,16,16,16,20,20,20,26,26,26,66,66,66,0 },
        { 4,4,4,4,4,4,6,6,4,4,4,6,6,6,8,8,8,12,12,12,16,16,16,20,20,20,26,26,26,34,34,34,42,42,42,12,12,12,0 }
    };

    unsigned tables, scfsi = 0;
    int main_data_begin, part_23_sum = 0;
    int sr_idx = HDR_GET_MY_SAMPLE_RATE(hdr); sr_idx -= (sr_idx != 0);
    int gr_count = HDR_IS_MONO(hdr) ? 1 : 2;

    if (HDR_TEST_MPEG1(hdr))
    {
        gr_count *= 2;
        main_data_begin = get_bits(bs, 9);
        scfsi = get_bits(bs, 7 + gr_count);
    } else
    {
        main_data_begin = get_bits(bs, 8 + gr_count) >> gr_count;
    }

    do
    {
        if (HDR_IS_MONO(hdr))
        {
            scfsi <<= 4;
        }
        gr->part_23_length = (uint16_t)get_bits(bs, 12);
        part_23_sum += gr->part_23_length;
        gr->big_values = (uint16_t)get_bits(bs,  9);
        if (gr->big_values > 288)
        {
            return -1;
        }
        gr->global_gain = (uint8_t)get_bits(bs, 8);
        gr->scalefac_compress = (uint16_t)get_bits(bs, HDR_TEST_MPEG1(hdr) ? 4 : 9);
        gr->sfbtab = g_scf_long[sr_idx];
        gr->n_long_sfb  = 22;
        gr->n_short_sfb = 0;
        if (get_bits(bs, 1))
        {
            gr->block_type = (uint8_t)get_bits(bs, 2);
            if (!gr->block_type)
            {
                return -1;
            }
            gr->mixed_block_flag = (uint8_t)get_bits(bs, 1);
            gr->region_count[0] = 7;
            gr->region_count[1] = 255;
            if (gr->block_type == SHORT_BLOCK_TYPE)
            {
                scfsi &= 0x0F0F;
                if (!gr->mixed_block_flag)
                {
                    gr->region_count[0] = 8;
                    gr->sfbtab = g_scf_short[sr_idx];
                    gr->n_long_sfb = 0;
                    gr->n_short_sfb = 39;
                } else
                {
                    gr->sfbtab = g_scf_mixed[sr_idx];
                    gr->n_long_sfb = HDR_TEST_MPEG1(hdr) ? 8 : 6;
                    gr->n_short_sfb = 30;
                }
            }
            tables = get_bits(bs, 10);
            tables <<= 5;
            gr->subblock_gain[0] = (uint8_t)get_bits(bs, 3);
            gr->subblock_gain[1] = (uint8_t)get_bits(bs, 3);
            gr->subblock_gain[2] = (uint8_t)get_bits(bs, 3);
        } else
        {
            gr->block_type = 0;
            gr->mixed_block_flag = 0;
            tables = get_bits(bs, 15);
            gr->region_count[0] = (uint8_t)get_bits(bs, 4);
            gr->region_count[1] = (uint8_t)get_bits(bs, 3);
            gr->region_count[2] = 255;
        }
        gr->table_select[0] = (uint8_t)(tables >> 10);
        gr->table_select[1] = (uint8_t)((tables >> 5) & 31);
        gr->table_select[2] = (uint8_t)((tables) & 31);
        gr->preflag = HDR_TEST_MPEG1(hdr) ? get_bits(bs, 1) : (gr->scalefac_compress >= 500);
        gr->scalefac_scale = (uint8_t)get_bits(bs, 1);
        gr->count1_table = (uint8_t)get_bits(bs, 1);
        gr->scfsi = (uint8_t)((scfsi >> 12) & 15);
        scfsi <<= 4;
        gr++;
    } while(--gr_count);

    if (part_23_sum + bs->pos > bs->limit + main_data_begin*8)
    {
        return -1;
    }

    return main_data_begin;
}

static void L3_read_scalefactors(uint8_t *scf, uint8_t *ist_pos, const uint8_t *scf_size, const uint8_t *scf_count, bs_t *bitbuf, int scfsi) FL_NO_EXCEPT
{
    int i, k;
    for (i = 0; i < 4 && scf_count[i]; i++, scfsi *= 2)
    {
        int cnt = scf_count[i];
        if (scfsi & 8)
        {
            memcpy(scf, ist_pos, cnt);
        } else
        {
            int bits = scf_size[i];
            if (!bits)
            {
                memset(scf, 0, cnt);
                memset(ist_pos, 0, cnt);
            } else
            {
                int max_scf = (scfsi < 0) ? (1 << bits) - 1 : -1;
                for (k = 0; k < cnt; k++)
                {
                    int s = get_bits(bitbuf, bits);
                    ist_pos[k] = (s == max_scf ? -1 : s);
                    scf[k] = s;
                }
            }
        }
        ist_pos += cnt;
        scf += cnt;
    }
    scf[0] = scf[1] = scf[2] = 0;
}

#if MINIMP3_HAVE_FIXED_POINT
/* Split a gain exponent expressed in quarter-powers of two into the
   mantissa/exponent pair the dequantiser multiplies with:
   2**(quarters/4) == g_expfrac_q30[r] * 2**(a - 30) for quarters == 4a + r. */
static void mp3d_gain_from_quarters(int quarters, int32_t *mant,
                                    int *exp) FL_NO_EXCEPT
{
    /* Floor division that stays correct for negative `quarters` without
       shifting a negative value, which is what UBSan objects to. */
    int r = quarters % 4;
    int a;
    if (r < 0)
    {
        r += 4;
    }
    a = (quarters - r) / 4;
    *mant = g_expfrac_q30[r];
    *exp = a;
}

/* x**(4/3) as mantissa * 2**(exp - 30).

   Below 129 this is a table lookup, laid out exactly like upstream's float
   table so the Huffman loop keeps folding the sign into the index. Above it,
   upstream interpolates from the same table with a quadratic in the fractional
   part; that is reproduced here in Q30, which is the most delicate arithmetic
   in the conversion and is covered exhaustively by a unit test over every
   reachable x. */
static int32_t mp3d_pow43(int x, int *exp) FL_NO_EXCEPT
{
    int mult_log2 = 8;
    int sign, num, den, idx;
    int32_t frac, poly, term;

    if (x < 129)
    {
        *exp = g_pow43_exp[16 + x];
        return g_pow43_mant[16 + x];
    }
    if (x < 1024)
    {
        mult_log2 = 4;
        x <<= 3;
    }
    sign = 2*x & 64;
    num = (x & 63) - sign;
    den = (x & ~63) + sign;
    /* Multiply rather than shift: `num` is negative for half of all inputs
       and shifting a negative value left is UB, which the differential fuzzer
       caught under UBSan. */
    frac = (int32_t)(((int64_t)num * ((int64_t)1 << 30)) / den);
    term = MP3D_Q30_POW43_C1 + mp3d_mulshift(frac, MP3D_Q30_POW43_C2, 30);
    /* Shift by 31 rather than 30: the polynomial reaches 1.72, which would
       carry a normalised mantissa past INT32_MAX. The lost bit is paid back
       in the exponent. */
    poly = ((int32_t)1 << 30) + mp3d_mulshift(frac, term, 30);
    idx = 16 + ((x + sign) >> 6);
    *exp = g_pow43_exp[idx] + mult_log2 + 1;
    return mp3d_mulshift(g_pow43_mant[idx], poly, 31);
}

/* scalefactor gain * x**(4/3), landed in Q(MINIMP3_FRAC_BITS) and clamped.
   Both inputs carry their own exponent, so this is where the pipeline's one
   unbounded quantity collapses into the fixed Q format. */
static int32_t mp3d_dequant(int32_t gain_mant, int gain_exp,
                            int32_t pow_mant, int pow_exp) FL_NO_EXCEPT
{
    int64_t product;
    int shift;
    if (pow_mant == 0)
    {
        return 0;
    }
    product = (int64_t)gain_mant * (int64_t)pow_mant;
    if (product == 0)
    {
        return 0;
    }
    /* value = product * 2**(gain_exp + pow_exp - 60); want Q(FRAC_BITS). */
    shift = 60 - MINIMP3_FRAC_BITS - gain_exp - pow_exp;
    if (shift >= 63)
    {
        return 0;
    }
    if (shift <= 0)
    {
        return product > 0 ? MP3D_ONE : -MP3D_ONE;
    }
    product = (product + ((int64_t)1 << (shift - 1))) >> shift;
    return mp3d_clamp_sample(product);
}
#else
static float L3_ldexp_q2(float y, int exp_q2) FL_NO_EXCEPT
{
    static const float g_expfrac[4] = { 9.31322575e-10f,7.83145814e-10f,6.58544508e-10f,5.53767716e-10f };
    int e;
    do
    {
        e = MINIMP3_MIN(30*4, exp_q2);
        y *= g_expfrac[e & 3]*(1 << 30 >> (e >> 2));
    } while ((exp_q2 -= e) > 0);
    return y;
}
#endif /* MINIMP3_HAVE_FIXED_POINT */

/* The bitstream half of scalefactor decoding is shared: only how the decoded
   integers are turned into gains differs between the two builds. Keeping
   `iscf`/`ist_pos` handling common is deliberate -- `ist_pos` drives intensity
   stereo positioning, and any drift there would be a parsing divergence rather
   than a rounding one. */
#if MINIMP3_HAVE_FIXED_POINT
static void L3_decode_scalefactors(const uint8_t *hdr, uint8_t *ist_pos, bs_t *bs, const L3_gr_info_t *gr, int32_t *scf_mant, int16_t *scf_exp, int ch) FL_NO_EXCEPT
#else
static void L3_decode_scalefactors(const uint8_t *hdr, uint8_t *ist_pos, bs_t *bs, const L3_gr_info_t *gr, float *scf, int ch) FL_NO_EXCEPT
#endif
{
    static const uint8_t g_scf_partitions[3][28] = {
        { 6,5,5, 5,6,5,5,5,6,5, 7,3,11,10,0,0, 7, 7, 7,0, 6, 6,6,3, 8, 8,5,0 },
        { 8,9,6,12,6,9,9,9,6,9,12,6,15,18,0,0, 6,15,12,0, 6,12,9,6, 6,18,9,0 },
        { 9,9,6,12,9,9,9,9,9,9,12,6,18,18,0,0,12,12,12,0,12, 9,9,6,15,12,9,0 }
    };
    const uint8_t *scf_partition = g_scf_partitions[!!gr->n_short_sfb + !gr->n_long_sfb];
    uint8_t scf_size[4], iscf[40];
    int i, scf_shift = gr->scalefac_scale + 1, gain_exp, scfsi = gr->scfsi;
#if !MINIMP3_HAVE_FIXED_POINT
    float gain;
#endif

    if (HDR_TEST_MPEG1(hdr))
    {
        static const uint8_t g_scfc_decode[16] = { 0,1,2,3, 12,5,6,7, 9,10,11,13, 14,15,18,19 };
        int part = g_scfc_decode[gr->scalefac_compress];
        scf_size[1] = scf_size[0] = (uint8_t)(part >> 2);
        scf_size[3] = scf_size[2] = (uint8_t)(part & 3);
    } else
    {
        static const uint8_t g_mod[6*4] = { 5,5,4,4,5,5,4,1,4,3,1,1,5,6,6,1,4,4,4,1,4,3,1,1 };
        int k, modprod, sfc, ist = HDR_TEST_I_STEREO(hdr) && ch;
        sfc = gr->scalefac_compress >> ist;
        for (k = ist*3*4; sfc >= 0; sfc -= modprod, k += 4)
        {
            for (modprod = 1, i = 3; i >= 0; i--)
            {
                scf_size[i] = (uint8_t)(sfc / modprod % g_mod[k + i]);
                modprod *= g_mod[k + i];
            }
        }
        scf_partition += k;
        scfsi = -16;
    }
    L3_read_scalefactors(iscf, ist_pos, scf_size, scf_partition, bs, scfsi);

    if (gr->n_short_sfb)
    {
        int sh = 3 - scf_shift;
        for (i = 0; i < gr->n_short_sfb; i += 3)
        {
            iscf[gr->n_long_sfb + i + 0] += gr->subblock_gain[0] << sh;
            iscf[gr->n_long_sfb + i + 1] += gr->subblock_gain[1] << sh;
            iscf[gr->n_long_sfb + i + 2] += gr->subblock_gain[2] << sh;
        }
    } else if (gr->preflag)
    {
        static const uint8_t g_preamp[10] = { 1,1,1,1,2,2,3,3,3,2 };
        for (i = 0; i < 10; i++)
        {
            iscf[11 + i] += g_preamp[i];
        }
    }

    gain_exp = gr->global_gain + BITS_DEQUANTIZER_OUT*4 - 210 - (HDR_IS_MS_STEREO(hdr) ? 2 : 0);
#if MINIMP3_HAVE_FIXED_POINT
    /* The float build computes 2**11 * 2**((gain_exp - 44 - iscf*2**shift)/4)
       by repeated multiplication. In quarter-power terms that whole expression
       is just an integer exponent, so the fixed build carries the integer and
       looks the fractional quarter up -- exact, and with none of
       L3_ldexp_q2's rounding. */
    for (i = 0; i < (int)(gr->n_long_sfb + gr->n_short_sfb); i++)
    {
        int32_t mant;
        int exp;
        mp3d_gain_from_quarters(gain_exp - MAX_SCFI - (iscf[i] << scf_shift),
                                &mant, &exp);
        scf_mant[i] = mant;
        scf_exp[i] = (int16_t)(exp + (MAX_SCFI/4));
    }
#else
    gain = L3_ldexp_q2(1 << (MAX_SCFI/4),  MAX_SCFI - gain_exp);
    for (i = 0; i < (int)(gr->n_long_sfb + gr->n_short_sfb); i++)
    {
        scf[i] = L3_ldexp_q2(gain, iscf[i] << scf_shift);
    }
#endif
}

#if MINIMP3_HAVE_FIXED_POINT
static int32_t mp3d_huff_escape(int32_t gain_mant, int gain_exp, int lsb,
                                int negative) FL_NO_EXCEPT
{
    int pow_exp;
    const int32_t pow_mant = mp3d_pow43(lsb, &pow_exp);
    const int32_t value = mp3d_dequant(gain_mant, gain_exp, pow_mant, pow_exp);
    return negative ? -value : value;
}

static int32_t mp3d_huff_one(int32_t gain_mant, int gain_exp,
                             int negative) FL_NO_EXCEPT
{
    const int32_t value = mp3d_scale_to_q(gain_mant, gain_exp);
    return negative ? -value : value;
}

/* The Huffman loop itself is bit-exact between the two builds -- only the four
   places that turn a decoded magnitude into a sample differ, so they go
   through these and the loop stays single-sourced. Frame acceptance and bit
   consumption are parsing properties, and the golden gate treats any
   divergence in them as a hard failure rather than a rounding one. */
#define MP3D_HUFF_SCF_ARGS      const int32_t *scf_mant, const int16_t *scf_exp
#define MP3D_HUFF_ONE_VARS      int32_t one_m = 0; int one_e = 0
#define MP3D_HUFF_NEXT_SCF()    (one_m = *scf_mant++, one_e = *scf_exp++)
#define MP3D_HUFF_ESC(lsb, neg) mp3d_huff_escape(one_m, one_e, (lsb), (neg))
#define MP3D_HUFF_TAB(idx)      mp3d_dequant(one_m, one_e, g_pow43_mant[idx], \
                                             g_pow43_exp[idx])
#define MP3D_HUFF_ONE(neg)      mp3d_huff_one(one_m, one_e, (neg))
#else
#define MP3D_HUFF_SCF_ARGS      const float *scf
#define MP3D_HUFF_ONE_VARS      float one = 0.0f
#define MP3D_HUFF_NEXT_SCF()    (one = *scf++)
#define MP3D_HUFF_ESC(lsb, neg) (one*L3_pow_43(lsb)*((neg) ? -1 : 1))
#define MP3D_HUFF_TAB(idx)      (g_pow43[idx]*one)
#define MP3D_HUFF_ONE(neg)      ((neg) ? -one : one)

static const float g_pow43[129 + 16] = {
    0,-1,-2.519842f,-4.326749f,-6.349604f,-8.549880f,-10.902724f,-13.390518f,-16.000000f,-18.720754f,-21.544347f,-24.463781f,-27.473142f,-30.567351f,-33.741992f,-36.993181f,
    0,1,2.519842f,4.326749f,6.349604f,8.549880f,10.902724f,13.390518f,16.000000f,18.720754f,21.544347f,24.463781f,27.473142f,30.567351f,33.741992f,36.993181f,40.317474f,43.711787f,47.173345f,50.699631f,54.288352f,57.937408f,61.644865f,65.408941f,69.227979f,73.100443f,77.024898f,81.000000f,85.024491f,89.097188f,93.216975f,97.382800f,101.593667f,105.848633f,110.146801f,114.487321f,118.869381f,123.292209f,127.755065f,132.257246f,136.798076f,141.376907f,145.993119f,150.646117f,155.335327f,160.060199f,164.820202f,169.614826f,174.443577f,179.305980f,184.201575f,189.129918f,194.090580f,199.083145f,204.107210f,209.162385f,214.248292f,219.364564f,224.510845f,229.686789f,234.892058f,240.126328f,245.389280f,250.680604f,256.000000f,261.347174f,266.721841f,272.123723f,277.552547f,283.008049f,288.489971f,293.998060f,299.532071f,305.091761f,310.676898f,316.287249f,321.922592f,327.582707f,333.267377f,338.976394f,344.709550f,350.466646f,356.247482f,362.051866f,367.879608f,373.730522f,379.604427f,385.501143f,391.420496f,397.362314f,403.326427f,409.312672f,415.320884f,421.350905f,427.402579f,433.475750f,439.570269f,445.685987f,451.822757f,457.980436f,464.158883f,470.357960f,476.577530f,482.817459f,489.077615f,495.357868f,501.658090f,507.978156f,514.317941f,520.677324f,527.056184f,533.454404f,539.871867f,546.308458f,552.764065f,559.238575f,565.731879f,572.243870f,578.774440f,585.323483f,591.890898f,598.476581f,605.080431f,611.702349f,618.342238f,625.000000f,631.675540f,638.368763f,645.079578f
};

static float L3_pow_43(int x) FL_NO_EXCEPT
{
    float frac;
    int sign, mult = 256;

    if (x < 129)
    {
        return g_pow43[16 + x];
    }

    if (x < 1024)
    {
        mult = 16;
        x <<= 3;
    }

    sign = 2*x & 64;
    frac = (float)((x & 63) - sign) / ((x & ~63) + sign);
    return g_pow43[16 + ((x + sign) >> 6)]*(1.f + frac*((4.f/3) + frac*(2.f/9)))*mult;
}
#endif /* MINIMP3_HAVE_FIXED_POINT */

static void L3_huffman(mp3d_dsp_t *dst, bs_t *bs, const L3_gr_info_t *gr_info, MP3D_HUFF_SCF_ARGS, int layer3gr_limit) FL_NO_EXCEPT
{
    static const int16_t tabs[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        785,785,785,785,784,784,784,784,513,513,513,513,513,513,513,513,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,
        -255,1313,1298,1282,785,785,785,785,784,784,784,784,769,769,769,769,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,290,288,
        -255,1313,1298,1282,769,769,769,769,529,529,529,529,529,529,529,529,528,528,528,528,528,528,528,528,512,512,512,512,512,512,512,512,290,288,
        -253,-318,-351,-367,785,785,785,785,784,784,784,784,769,769,769,769,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,819,818,547,547,275,275,275,275,561,560,515,546,289,274,288,258,
        -254,-287,1329,1299,1314,1312,1057,1057,1042,1042,1026,1026,784,784,784,784,529,529,529,529,529,529,529,529,769,769,769,769,768,768,768,768,563,560,306,306,291,259,
        -252,-413,-477,-542,1298,-575,1041,1041,784,784,784,784,769,769,769,769,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,-383,-399,1107,1092,1106,1061,849,849,789,789,1104,1091,773,773,1076,1075,341,340,325,309,834,804,577,577,532,532,516,516,832,818,803,816,561,561,531,531,515,546,289,289,288,258,
        -252,-429,-493,-559,1057,1057,1042,1042,529,529,529,529,529,529,529,529,784,784,784,784,769,769,769,769,512,512,512,512,512,512,512,512,-382,1077,-415,1106,1061,1104,849,849,789,789,1091,1076,1029,1075,834,834,597,581,340,340,339,324,804,833,532,532,832,772,818,803,817,787,816,771,290,290,290,290,288,258,
        -253,-349,-414,-447,-463,1329,1299,-479,1314,1312,1057,1057,1042,1042,1026,1026,785,785,785,785,784,784,784,784,769,769,769,769,768,768,768,768,-319,851,821,-335,836,850,805,849,341,340,325,336,533,533,579,579,564,564,773,832,578,548,563,516,321,276,306,291,304,259,
        -251,-572,-733,-830,-863,-879,1041,1041,784,784,784,784,769,769,769,769,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,-511,-527,-543,1396,1351,1381,1366,1395,1335,1380,-559,1334,1138,1138,1063,1063,1350,1392,1031,1031,1062,1062,1364,1363,1120,1120,1333,1348,881,881,881,881,375,374,359,373,343,358,341,325,791,791,1123,1122,-703,1105,1045,-719,865,865,790,790,774,774,1104,1029,338,293,323,308,-799,-815,833,788,772,818,803,816,322,292,307,320,561,531,515,546,289,274,288,258,
        -251,-525,-605,-685,-765,-831,-846,1298,1057,1057,1312,1282,785,785,785,785,784,784,784,784,769,769,769,769,512,512,512,512,512,512,512,512,1399,1398,1383,1367,1382,1396,1351,-511,1381,1366,1139,1139,1079,1079,1124,1124,1364,1349,1363,1333,882,882,882,882,807,807,807,807,1094,1094,1136,1136,373,341,535,535,881,775,867,822,774,-591,324,338,-671,849,550,550,866,864,609,609,293,336,534,534,789,835,773,-751,834,804,308,307,833,788,832,772,562,562,547,547,305,275,560,515,290,290,
        -252,-397,-477,-557,-622,-653,-719,-735,-750,1329,1299,1314,1057,1057,1042,1042,1312,1282,1024,1024,785,785,785,785,784,784,784,784,769,769,769,769,-383,1127,1141,1111,1126,1140,1095,1110,869,869,883,883,1079,1109,882,882,375,374,807,868,838,881,791,-463,867,822,368,263,852,837,836,-543,610,610,550,550,352,336,534,534,865,774,851,821,850,805,593,533,579,564,773,832,578,578,548,548,577,577,307,276,306,291,516,560,259,259,
        -250,-2107,-2507,-2764,-2909,-2974,-3007,-3023,1041,1041,1040,1040,769,769,769,769,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,-767,-1052,-1213,-1277,-1358,-1405,-1469,-1535,-1550,-1582,-1614,-1647,-1662,-1694,-1726,-1759,-1774,-1807,-1822,-1854,-1886,1565,-1919,-1935,-1951,-1967,1731,1730,1580,1717,-1983,1729,1564,-1999,1548,-2015,-2031,1715,1595,-2047,1714,-2063,1610,-2079,1609,-2095,1323,1323,1457,1457,1307,1307,1712,1547,1641,1700,1699,1594,1685,1625,1442,1442,1322,1322,-780,-973,-910,1279,1278,1277,1262,1276,1261,1275,1215,1260,1229,-959,974,974,989,989,-943,735,478,478,495,463,506,414,-1039,1003,958,1017,927,942,987,957,431,476,1272,1167,1228,-1183,1256,-1199,895,895,941,941,1242,1227,1212,1135,1014,1014,490,489,503,487,910,1013,985,925,863,894,970,955,1012,847,-1343,831,755,755,984,909,428,366,754,559,-1391,752,486,457,924,997,698,698,983,893,740,740,908,877,739,739,667,667,953,938,497,287,271,271,683,606,590,712,726,574,302,302,738,736,481,286,526,725,605,711,636,724,696,651,589,681,666,710,364,467,573,695,466,466,301,465,379,379,709,604,665,679,316,316,634,633,436,436,464,269,424,394,452,332,438,363,347,408,393,448,331,422,362,407,392,421,346,406,391,376,375,359,1441,1306,-2367,1290,-2383,1337,-2399,-2415,1426,1321,-2431,1411,1336,-2447,-2463,-2479,1169,1169,1049,1049,1424,1289,1412,1352,1319,-2495,1154,1154,1064,1064,1153,1153,416,390,360,404,403,389,344,374,373,343,358,372,327,357,342,311,356,326,1395,1394,1137,1137,1047,1047,1365,1392,1287,1379,1334,1364,1349,1378,1318,1363,792,792,792,792,1152,1152,1032,1032,1121,1121,1046,1046,1120,1120,1030,1030,-2895,1106,1061,1104,849,849,789,789,1091,1076,1029,1090,1060,1075,833,833,309,324,532,532,832,772,818,803,561,561,531,560,515,546,289,274,288,258,
        -250,-1179,-1579,-1836,-1996,-2124,-2253,-2333,-2413,-2477,-2542,-2574,-2607,-2622,-2655,1314,1313,1298,1312,1282,785,785,785,785,1040,1040,1025,1025,768,768,768,768,-766,-798,-830,-862,-895,-911,-927,-943,-959,-975,-991,-1007,-1023,-1039,-1055,-1070,1724,1647,-1103,-1119,1631,1767,1662,1738,1708,1723,-1135,1780,1615,1779,1599,1677,1646,1778,1583,-1151,1777,1567,1737,1692,1765,1722,1707,1630,1751,1661,1764,1614,1736,1676,1763,1750,1645,1598,1721,1691,1762,1706,1582,1761,1566,-1167,1749,1629,767,766,751,765,494,494,735,764,719,749,734,763,447,447,748,718,477,506,431,491,446,476,461,505,415,430,475,445,504,399,460,489,414,503,383,474,429,459,502,502,746,752,488,398,501,473,413,472,486,271,480,270,-1439,-1455,1357,-1471,-1487,-1503,1341,1325,-1519,1489,1463,1403,1309,-1535,1372,1448,1418,1476,1356,1462,1387,-1551,1475,1340,1447,1402,1386,-1567,1068,1068,1474,1461,455,380,468,440,395,425,410,454,364,467,466,464,453,269,409,448,268,432,1371,1473,1432,1417,1308,1460,1355,1446,1459,1431,1083,1083,1401,1416,1458,1445,1067,1067,1370,1457,1051,1051,1291,1430,1385,1444,1354,1415,1400,1443,1082,1082,1173,1113,1186,1066,1185,1050,-1967,1158,1128,1172,1097,1171,1081,-1983,1157,1112,416,266,375,400,1170,1142,1127,1065,793,793,1169,1033,1156,1096,1141,1111,1155,1080,1126,1140,898,898,808,808,897,897,792,792,1095,1152,1032,1125,1110,1139,1079,1124,882,807,838,881,853,791,-2319,867,368,263,822,852,837,866,806,865,-2399,851,352,262,534,534,821,836,594,594,549,549,593,593,533,533,848,773,579,579,564,578,548,563,276,276,577,576,306,291,516,560,305,305,275,259,
        -251,-892,-2058,-2620,-2828,-2957,-3023,-3039,1041,1041,1040,1040,769,769,769,769,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,-511,-527,-543,-559,1530,-575,-591,1528,1527,1407,1526,1391,1023,1023,1023,1023,1525,1375,1268,1268,1103,1103,1087,1087,1039,1039,1523,-604,815,815,815,815,510,495,509,479,508,463,507,447,431,505,415,399,-734,-782,1262,-815,1259,1244,-831,1258,1228,-847,-863,1196,-879,1253,987,987,748,-767,493,493,462,477,414,414,686,669,478,446,461,445,474,429,487,458,412,471,1266,1264,1009,1009,799,799,-1019,-1276,-1452,-1581,-1677,-1757,-1821,-1886,-1933,-1997,1257,1257,1483,1468,1512,1422,1497,1406,1467,1496,1421,1510,1134,1134,1225,1225,1466,1451,1374,1405,1252,1252,1358,1480,1164,1164,1251,1251,1238,1238,1389,1465,-1407,1054,1101,-1423,1207,-1439,830,830,1248,1038,1237,1117,1223,1148,1236,1208,411,426,395,410,379,269,1193,1222,1132,1235,1221,1116,976,976,1192,1162,1177,1220,1131,1191,963,963,-1647,961,780,-1663,558,558,994,993,437,408,393,407,829,978,813,797,947,-1743,721,721,377,392,844,950,828,890,706,706,812,859,796,960,948,843,934,874,571,571,-1919,690,555,689,421,346,539,539,944,779,918,873,932,842,903,888,570,570,931,917,674,674,-2575,1562,-2591,1609,-2607,1654,1322,1322,1441,1441,1696,1546,1683,1593,1669,1624,1426,1426,1321,1321,1639,1680,1425,1425,1305,1305,1545,1668,1608,1623,1667,1592,1638,1666,1320,1320,1652,1607,1409,1409,1304,1304,1288,1288,1664,1637,1395,1395,1335,1335,1622,1636,1394,1394,1319,1319,1606,1621,1392,1392,1137,1137,1137,1137,345,390,360,375,404,373,1047,-2751,-2767,-2783,1062,1121,1046,-2799,1077,-2815,1106,1061,789,789,1105,1104,263,355,310,340,325,354,352,262,339,324,1091,1076,1029,1090,1060,1075,833,833,788,788,1088,1028,818,818,803,803,561,561,531,531,816,771,546,546,289,274,288,258,
        -253,-317,-381,-446,-478,-509,1279,1279,-811,-1179,-1451,-1756,-1900,-2028,-2189,-2253,-2333,-2414,-2445,-2511,-2526,1313,1298,-2559,1041,1041,1040,1040,1025,1025,1024,1024,1022,1007,1021,991,1020,975,1019,959,687,687,1018,1017,671,671,655,655,1016,1015,639,639,758,758,623,623,757,607,756,591,755,575,754,559,543,543,1009,783,-575,-621,-685,-749,496,-590,750,749,734,748,974,989,1003,958,988,973,1002,942,987,957,972,1001,926,986,941,971,956,1000,910,985,925,999,894,970,-1071,-1087,-1102,1390,-1135,1436,1509,1451,1374,-1151,1405,1358,1480,1420,-1167,1507,1494,1389,1342,1465,1435,1450,1326,1505,1310,1493,1373,1479,1404,1492,1464,1419,428,443,472,397,736,526,464,464,486,457,442,471,484,482,1357,1449,1434,1478,1388,1491,1341,1490,1325,1489,1463,1403,1309,1477,1372,1448,1418,1433,1476,1356,1462,1387,-1439,1475,1340,1447,1402,1474,1324,1461,1371,1473,269,448,1432,1417,1308,1460,-1711,1459,-1727,1441,1099,1099,1446,1386,1431,1401,-1743,1289,1083,1083,1160,1160,1458,1445,1067,1067,1370,1457,1307,1430,1129,1129,1098,1098,268,432,267,416,266,400,-1887,1144,1187,1082,1173,1113,1186,1066,1050,1158,1128,1143,1172,1097,1171,1081,420,391,1157,1112,1170,1142,1127,1065,1169,1049,1156,1096,1141,1111,1155,1080,1126,1154,1064,1153,1140,1095,1048,-2159,1125,1110,1137,-2175,823,823,1139,1138,807,807,384,264,368,263,868,838,853,791,867,822,852,837,866,806,865,790,-2319,851,821,836,352,262,850,805,849,-2399,533,533,835,820,336,261,578,548,563,577,532,532,832,772,562,562,547,547,305,275,560,515,290,290,288,258 };
    static const uint8_t tab32[] = { 130,162,193,209,44,28,76,140,9,9,9,9,9,9,9,9,190,254,222,238,126,94,157,157,109,61,173,205 };
    static const uint8_t tab33[] = { 252,236,220,204,188,172,156,140,124,108,92,76,60,44,28,12 };
    static const int16_t tabindex[2*16] = { 0,32,64,98,0,132,180,218,292,364,426,538,648,746,0,1126,1460,1460,1460,1460,1460,1460,1460,1460,1842,1842,1842,1842,1842,1842,1842,1842 };
    static const uint8_t g_linbits[] =  { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,2,3,4,6,8,10,13,4,5,6,7,8,9,11,13 };

#define PEEK_BITS(n)  (bs_cache >> (32 - n))
#define FLUSH_BITS(n) { bs_cache <<= (n); bs_sh += (n); }
#define CHECK_BITS    while (bs_sh >= 0) { bs_cache |= (uint32_t)*bs_next_ptr++ << bs_sh; bs_sh -= 8; }
#define BSPOS         ((bs_next_ptr - bs->buf)*8 - 24 + bs_sh)

    MP3D_HUFF_ONE_VARS;
    int ireg = 0, big_val_cnt = gr_info->big_values;
    const uint8_t *sfb = gr_info->sfbtab;
    const uint8_t *bs_next_ptr = bs->buf + bs->pos/8;
    uint32_t bs_cache = (((bs_next_ptr[0]*256u + bs_next_ptr[1])*256u + bs_next_ptr[2])*256u + bs_next_ptr[3]) << (bs->pos & 7);
    int pairs_to_decode, np, bs_sh = (bs->pos & 7) - 8;
    bs_next_ptr += 4;

    while (big_val_cnt > 0)
    {
        int tab_num = gr_info->table_select[ireg];
        int sfb_cnt = gr_info->region_count[ireg++];
        const int16_t *codebook = tabs + tabindex[tab_num];
        int linbits = g_linbits[tab_num];
        if (linbits)
        {
            do
            {
                np = *sfb++ / 2;
                pairs_to_decode = MINIMP3_MIN(big_val_cnt, np);
                MP3D_HUFF_NEXT_SCF();
                do
                {
                    int j, w = 5;
                    int leaf = codebook[PEEK_BITS(w)];
                    while (leaf < 0)
                    {
                        FLUSH_BITS(w);
                        w = leaf & 7;
                        leaf = codebook[PEEK_BITS(w) - (leaf >> 3)];
                    }
                    FLUSH_BITS(leaf >> 8);

                    for (j = 0; j < 2; j++, dst++, leaf >>= 4)
                    {
                        int lsb = leaf & 0x0F;
                        if (lsb == 15)
                        {
                            lsb += PEEK_BITS(linbits);
                            FLUSH_BITS(linbits);
                            CHECK_BITS;
                            *dst = MP3D_HUFF_ESC(lsb, (int32_t)bs_cache < 0);
                        } else
                        {
                            *dst = MP3D_HUFF_TAB(16 + lsb - 16*(bs_cache >> 31));
                        }
                        FLUSH_BITS(lsb ? 1 : 0);
                    }
                    CHECK_BITS;
                } while (--pairs_to_decode);
            } while ((big_val_cnt -= np) > 0 && --sfb_cnt >= 0);
        } else
        {
            do
            {
                np = *sfb++ / 2;
                pairs_to_decode = MINIMP3_MIN(big_val_cnt, np);
                MP3D_HUFF_NEXT_SCF();
                do
                {
                    int j, w = 5;
                    int leaf = codebook[PEEK_BITS(w)];
                    while (leaf < 0)
                    {
                        FLUSH_BITS(w);
                        w = leaf & 7;
                        leaf = codebook[PEEK_BITS(w) - (leaf >> 3)];
                    }
                    FLUSH_BITS(leaf >> 8);

                    for (j = 0; j < 2; j++, dst++, leaf >>= 4)
                    {
                        int lsb = leaf & 0x0F;
                        *dst = MP3D_HUFF_TAB(16 + lsb - 16*(bs_cache >> 31));
                        FLUSH_BITS(lsb ? 1 : 0);
                    }
                    CHECK_BITS;
                } while (--pairs_to_decode);
            } while ((big_val_cnt -= np) > 0 && --sfb_cnt >= 0);
        }
    }

    for (np = 1 - big_val_cnt;; dst += 4)
    {
        const uint8_t *codebook_count1 = (gr_info->count1_table) ? tab33 : tab32;
        int leaf = codebook_count1[PEEK_BITS(4)];
        if (!(leaf & 8))
        {
            leaf = codebook_count1[(leaf >> 3) + (bs_cache << 4 >> (32 - (leaf & 3)))];
        }
        FLUSH_BITS(leaf & 7);
        if (BSPOS > layer3gr_limit)
        {
            break;
        }
#define RELOAD_SCALEFACTOR  if (!--np) { np = *sfb++/2; if (!np) break; MP3D_HUFF_NEXT_SCF(); }
#define DEQ_COUNT1(s) if (leaf & (128 >> s)) { dst[s] = MP3D_HUFF_ONE((int32_t)bs_cache < 0); FLUSH_BITS(1) }
        RELOAD_SCALEFACTOR;
        DEQ_COUNT1(0);
        DEQ_COUNT1(1);
        RELOAD_SCALEFACTOR;
        DEQ_COUNT1(2);
        DEQ_COUNT1(3);
        CHECK_BITS;
    }

    bs->pos = layer3gr_limit;
}

static void L3_midside_stereo(mp3d_dsp_t *left, int n) FL_NO_EXCEPT
{
    int i = 0;
    mp3d_dsp_t *right = left + 576;
#if HAVE_SIMD
    if (have_simd())
    {
        for (; i < n - 3; i += 4)
        {
            f4 vl = VLD(left + i);
            f4 vr = VLD(right + i);
            VSTORE(left + i, VADD(vl, vr));
            VSTORE(right + i, VSUB(vl, vr));
        }
#ifdef __GNUC__
        /* Workaround for spurious -Waggressive-loop-optimizations warning from gcc.
         * For more info see: https://github.com/lieff/minimp3/issues/88
         */
        if (__builtin_constant_p(n % 4 == 0) && n % 4 == 0)
            return;
#endif
    }
#endif /* HAVE_SIMD */
    for (; i < n; i++)
    {
#if MINIMP3_HAVE_FIXED_POINT
        /* Saturating rather than wrapping: mid/side sums can exceed the
           clamped input range on a hostile stream, and a wrap would be both
           signed-overflow UB and an audible discontinuity. */
        const int32_t a = left[i];
        const int32_t b = right[i];
        left[i] = mp3d_add_sat(a, b);
        right[i] = mp3d_sub_sat(a, b);
#else
        float a = left[i];
        float b = right[i];
        left[i] = a + b;
        right[i] = a - b;
#endif
    }
}

/* Intensity-stereo gains are Q30 in the fixed build: they reach sqrt(2) after
   the mid/side rescale, so Q30 is the widest format that still holds them. */
#if MINIMP3_HAVE_FIXED_POINT
typedef int32_t mp3d_coef_t;
#else
typedef float mp3d_coef_t;
#endif

static void L3_intensity_stereo_band(mp3d_dsp_t *left, int n, mp3d_coef_t kl, mp3d_coef_t kr) FL_NO_EXCEPT
{
    int i;
    for (i = 0; i < n; i++)
    {
#if MINIMP3_HAVE_FIXED_POINT
        left[i + 576] = mp3d_mulshift(left[i], kr, 30);
        left[i] = mp3d_mulshift(left[i], kl, 30);
#else
        left[i + 576] = left[i]*kr;
        left[i] = left[i]*kl;
#endif
    }
}

static void L3_stereo_top_band(const mp3d_dsp_t *right, const uint8_t *sfb, int nbands, int max_band[3]) FL_NO_EXCEPT // ok array parameter
{
    int i, k;

    max_band[0] = max_band[1] = max_band[2] = -1;

    for (i = 0; i < nbands; i++)
    {
        for (k = 0; k < sfb[i]; k += 2)
        {
            if (right[k] != 0 || right[k + 1] != 0)
            {
                max_band[i % 3] = i;
                break;
            }
        }
        right += sfb[i];
    }
}

#if MINIMP3_HAVE_FIXED_POINT
/* MPEG-2 intensity gain 2**(-quarters/4), as a Q30 coefficient. The exponent
   is always <= 0 here, so this is the same quarter-power split the scalefactor
   gains use, collapsed by a right shift instead of carried. */
static int32_t mp3d_ist_gain_q30(int quarters) FL_NO_EXCEPT
{
    int32_t mant;
    int exp;
    mp3d_gain_from_quarters(-quarters, &mant, &exp);
    return mp3d_shr_round(mant, -exp);
}
#endif

static void L3_stereo_process(mp3d_dsp_t *left, const uint8_t *ist_pos, const uint8_t *sfb, const uint8_t *hdr, int max_band[3], int mpeg2_sh) FL_NO_EXCEPT // ok array parameter
{
#if !MINIMP3_HAVE_FIXED_POINT
    static const float g_pan[7*2] = { 0,1,0.21132487f,0.78867513f,0.36602540f,0.63397460f,0.5f,0.5f,0.63397460f,0.36602540f,0.78867513f,0.21132487f,1,0 };
#endif
    unsigned i, max_pos = HDR_TEST_MPEG1(hdr) ? 7 : 64;

    for (i = 0; sfb[i]; i++)
    {
        unsigned ipos = ist_pos[i];
        if ((int)i > max_band[i % 3] && ipos < max_pos)
        {
#if MINIMP3_HAVE_FIXED_POINT
            int32_t kl, kr;
            const int32_t s = HDR_TEST_MS_STEREO(hdr) ? MP3D_Q30_SQRT2
                                                      : ((int32_t)1 << 30);
            if (HDR_TEST_MPEG1(hdr))
            {
                kl = g_pan_q30[2*ipos];
                kr = g_pan_q30[2*ipos + 1];
            } else
            {
                kl = (int32_t)1 << 30;
                kr = mp3d_ist_gain_q30((int)((ipos + 1) >> 1) << mpeg2_sh);
                if (ipos & 1)
                {
                    kl = kr;
                    kr = (int32_t)1 << 30;
                }
            }
            L3_intensity_stereo_band(left, sfb[i], mp3d_mulshift(kl, s, 30),
                                     mp3d_mulshift(kr, s, 30));
#else
            float kl, kr, s = HDR_TEST_MS_STEREO(hdr) ? 1.41421356f : 1;
            if (HDR_TEST_MPEG1(hdr))
            {
                kl = g_pan[2*ipos];
                kr = g_pan[2*ipos + 1];
            } else
            {
                kl = 1;
                kr = L3_ldexp_q2(1, (ipos + 1) >> 1 << mpeg2_sh);
                if (ipos & 1)
                {
                    kl = kr;
                    kr = 1;
                }
            }
            L3_intensity_stereo_band(left, sfb[i], kl*s, kr*s);
#endif
        } else if (HDR_TEST_MS_STEREO(hdr))
        {
            L3_midside_stereo(left, sfb[i]);
        }
        left += sfb[i];
    }
}

static void L3_intensity_stereo(mp3d_dsp_t *left, uint8_t *ist_pos, const L3_gr_info_t *gr, const uint8_t *hdr) FL_NO_EXCEPT
{
    int max_band[3], n_sfb = gr->n_long_sfb + gr->n_short_sfb;
    int i, max_blocks = gr->n_short_sfb ? 3 : 1;

    L3_stereo_top_band(left + 576, gr->sfbtab, n_sfb, max_band);
    if (gr->n_long_sfb)
    {
        max_band[0] = max_band[1] = max_band[2] = MINIMP3_MAX(MINIMP3_MAX(max_band[0], max_band[1]), max_band[2]);
    }
    for (i = 0; i < max_blocks; i++)
    {
        int default_pos = HDR_TEST_MPEG1(hdr) ? 3 : 0;
        int itop = n_sfb - max_blocks + i;
        int prev = itop - max_blocks;
        ist_pos[itop] = max_band[i] >= prev ? default_pos : ist_pos[prev];
    }
    L3_stereo_process(left, ist_pos, gr->sfbtab, hdr, max_band, gr[1].scalefac_compress & 1);
}

static void L3_reorder(mp3d_dsp_t *grbuf, mp3d_dsp_t *scratch, const uint8_t *sfb) FL_NO_EXCEPT
{
    int i, len;
    mp3d_dsp_t *src = grbuf, *dst = scratch;

    for (;0 != (len = *sfb); sfb += 3, src += 2*len)
    {
        for (i = 0; i < len; i++, src++)
        {
            *dst++ = src[0*len];
            *dst++ = src[1*len];
            *dst++ = src[2*len];
        }
    }
    memcpy(grbuf, scratch, (dst - scratch)*sizeof(mp3d_dsp_t));
}

static void L3_antialias(mp3d_dsp_t *grbuf, int nbands) FL_NO_EXCEPT
{
#if !MINIMP3_HAVE_FIXED_POINT
    static const float g_aa[2][8] = {
        {0.85749293f,0.88174200f,0.94962865f,0.98331459f,0.99551782f,0.99916056f,0.99989920f,0.99999316f},
        {0.51449576f,0.47173197f,0.31337745f,0.18191320f,0.09457419f,0.04096558f,0.01419856f,0.00369997f}
    };
#endif

    for (; nbands > 0; nbands--, grbuf += 18)
    {
        int i = 0;
#if HAVE_SIMD
        if (have_simd()) for (; i < 8; i += 4)
        {
            f4 vu = VLD(grbuf + 18 + i);
            f4 vd = VLD(grbuf + 14 - i);
            f4 vc0 = VLD(g_aa[0] + i);
            f4 vc1 = VLD(g_aa[1] + i);
            vd = VREV(vd);
            VSTORE(grbuf + 18 + i, VSUB(VMUL(vu, vc0), VMUL(vd, vc1)));
            vd = VADD(VMUL(vu, vc1), VMUL(vd, vc0));
            VSTORE(grbuf + 14 - i, VREV(vd));
        }
#endif /* HAVE_SIMD */
#ifndef MINIMP3_ONLY_SIMD
        for(; i < 8; i++)
        {
#if MINIMP3_HAVE_FIXED_POINT
            const int32_t u = grbuf[18 + i];
            const int32_t d = grbuf[17 - i];
            grbuf[18 + i] = mp3d_sub_sat(mp3d_mulshift(u, g_aa_cs_q31[i], 31),
                                         mp3d_mulshift(d, g_aa_ca_q31[i], 31));
            grbuf[17 - i] = mp3d_add_sat(mp3d_mulshift(u, g_aa_ca_q31[i], 31),
                                         mp3d_mulshift(d, g_aa_cs_q31[i], 31));
#else
            float u = grbuf[18 + i];
            float d = grbuf[17 - i];
            grbuf[18 + i] = u*g_aa[0][i] - d*g_aa[1][i];
            grbuf[17 - i] = u*g_aa[1][i] + d*g_aa[0][i];
#endif
        }
#endif /* MINIMP3_ONLY_SIMD */
    }
}

#if MINIMP3_HAVE_FIXED_POINT
/* 9-point DCT-III. Same butterfly graph as the float build, with every
   constant in Q31 and every add saturating. The two halvings are arithmetic
   right shifts with the pipeline's rounding rule rather than a multiply by a
   Q31 0.5, which would cost a 64-bit multiply for an exact power of two. */
static void L3_dct3_9(int32_t *y) FL_NO_EXCEPT
{
    int32_t s0, s1, s2, s3, s4, s5, s6, s7, s8, t0, t2, t4;

    s0 = y[0]; s2 = y[2]; s4 = y[4]; s6 = y[6]; s8 = y[8];
    t0 = mp3d_add_sat(s0, mp3d_shr_round(s6, 1));
    s0 = mp3d_sub_sat(s0, s6);
    t4 = mp3d_mulshift(mp3d_add_sat(s4, s2), MP3D_Q31_COS_PI_9, 31);
    t2 = mp3d_mulshift(mp3d_add_sat(s8, s2), MP3D_Q31_COS_2PI_9, 31);
    s6 = mp3d_mulshift(mp3d_sub_sat(s4, s8), MP3D_Q31_COS_4PI_9, 31);
    s4 = mp3d_add_sat(s4, mp3d_sub_sat(s8, s2));

    s2 = mp3d_sub_sat(s0, mp3d_shr_round(s4, 1));
    y[4] = mp3d_add_sat(s4, s0);
    s8 = mp3d_add_sat(mp3d_sub_sat(t0, t2), s6);
    s0 = mp3d_add_sat(mp3d_sub_sat(t0, t4), t2);
    s4 = mp3d_sub_sat(mp3d_add_sat(t0, t4), s6);

    s1 = y[1]; s3 = y[3]; s5 = y[5]; s7 = y[7];

    s3 = mp3d_mulshift(s3, MP3D_Q31_COS_PI_6, 31);
    t0 = mp3d_mulshift(mp3d_add_sat(s5, s1), MP3D_Q31_COS_PI_18, 31);
    t4 = mp3d_mulshift(mp3d_sub_sat(s5, s7), MP3D_Q31_COS_7PI_18, 31);
    t2 = mp3d_mulshift(mp3d_add_sat(s1, s7), MP3D_Q31_COS_5PI_18, 31);
    s1 = mp3d_mulshift(mp3d_sub_sat(mp3d_sub_sat(s1, s5), s7),
                       MP3D_Q31_COS_PI_6, 31);

    s5 = mp3d_sub_sat(mp3d_sub_sat(t0, s3), t2);
    s7 = mp3d_sub_sat(mp3d_sub_sat(t4, s3), t0);
    s3 = mp3d_sub_sat(mp3d_add_sat(t4, s3), t2);

    y[0] = mp3d_sub_sat(s4, s7);
    y[1] = mp3d_add_sat(s2, s1);
    y[2] = mp3d_sub_sat(s0, s3);
    y[3] = mp3d_add_sat(s8, s5);
    y[5] = mp3d_sub_sat(s8, s5);
    y[6] = mp3d_add_sat(s0, s3);
    y[7] = mp3d_sub_sat(s2, s1);
    y[8] = mp3d_add_sat(s4, s7);
}

static void L3_imdct36(int32_t *grbuf, int32_t *overlap, const int32_t *window, int nbands) FL_NO_EXCEPT
{
    int i, j;

    for (j = 0; j < nbands; j++, grbuf += 18, overlap += 9)
    {
        int32_t co[9], si[9];
        co[0] = -grbuf[0];
        si[0] = grbuf[17];
        for (i = 0; i < 4; i++)
        {
            si[8 - 2*i] =  mp3d_sub_sat(grbuf[4*i + 1], grbuf[4*i + 2]);
            co[1 + 2*i] =  mp3d_add_sat(grbuf[4*i + 1], grbuf[4*i + 2]);
            si[7 - 2*i] =  mp3d_sub_sat(grbuf[4*i + 4], grbuf[4*i + 3]);
            co[2 + 2*i] = -mp3d_add_sat(grbuf[4*i + 3], grbuf[4*i + 4]);
        }
        L3_dct3_9(co);
        L3_dct3_9(si);

        si[1] = -si[1];
        si[3] = -si[3];
        si[5] = -si[5];
        si[7] = -si[7];

        /* The twiddle and window products accumulate in int64 before a single
           rounded narrow, so each output carries one rounding step rather than
           two. That matters: this pair of multiply-accumulates runs 18 times
           per band per granule, and per-term rounding would bias the overlap
           state, which then feeds the next frame. */
        for (i = 0; i < 9; i++)
        {
            const int32_t ovl = overlap[i];
            const int32_t sum = mp3d_narrow_q30(
                (int64_t)co[i]*g_twid9_q30[9 + i] +
                (int64_t)si[i]*g_twid9_q30[0 + i]);
            overlap[i] = mp3d_narrow_q30(
                (int64_t)co[i]*g_twid9_q30[0 + i] -
                (int64_t)si[i]*g_twid9_q30[9 + i]);
            grbuf[i] = mp3d_narrow_q30(
                (int64_t)ovl*window[0 + i] - (int64_t)sum*window[9 + i]);
            grbuf[17 - i] = mp3d_narrow_q30(
                (int64_t)ovl*window[9 + i] + (int64_t)sum*window[0 + i]);
        }
    }
}

static void L3_idct3(int32_t x0, int32_t x1, int32_t x2, int32_t *dst) FL_NO_EXCEPT
{
    const int32_t m1 = mp3d_mulshift(x1, MP3D_Q31_COS_PI_6, 31);
    const int32_t a1 = mp3d_sub_sat(x0, mp3d_shr_round(x2, 1));
    dst[1] = mp3d_add_sat(x0, x2);
    dst[0] = mp3d_add_sat(a1, m1);
    dst[2] = mp3d_sub_sat(a1, m1);
}

static void L3_imdct12(int32_t *x, int32_t *dst, int32_t *overlap) FL_NO_EXCEPT
{
    int32_t co[3], si[3];
    int i;

    L3_idct3(-x[0], mp3d_add_sat(x[6], x[3]), mp3d_add_sat(x[12], x[9]), co);
    L3_idct3(x[15], mp3d_sub_sat(x[12], x[9]), mp3d_sub_sat(x[6], x[3]), si);
    si[1] = -si[1];

    for (i = 0; i < 3; i++)
    {
        const int32_t ovl = overlap[i];
        const int32_t sum = mp3d_narrow_q30(
            (int64_t)co[i]*g_twid3_q30[3 + i] +
            (int64_t)si[i]*g_twid3_q30[0 + i]);
        overlap[i] = mp3d_narrow_q30(
            (int64_t)co[i]*g_twid3_q30[0 + i] -
            (int64_t)si[i]*g_twid3_q30[3 + i]);
        dst[i] = mp3d_narrow_q30(
            (int64_t)ovl*g_twid3_q30[2 - i] - (int64_t)sum*g_twid3_q30[5 - i]);
        dst[5 - i] = mp3d_narrow_q30(
            (int64_t)ovl*g_twid3_q30[5 - i] + (int64_t)sum*g_twid3_q30[2 - i]);
    }
}

static void L3_imdct_short(int32_t *grbuf, int32_t *overlap, int nbands) FL_NO_EXCEPT
{
    for (;nbands > 0; nbands--, overlap += 9, grbuf += 18)
    {
        int32_t tmp[18];
        memcpy(tmp, grbuf, sizeof(tmp));
        memcpy(grbuf, overlap, 6*sizeof(int32_t));
        L3_imdct12(tmp, grbuf + 6, overlap + 6);
        L3_imdct12(tmp + 1, grbuf + 12, overlap + 6);
        L3_imdct12(tmp + 2, overlap, overlap + 6);
    }
}

static void L3_change_sign(int32_t *grbuf) FL_NO_EXCEPT
{
    int b, i;
    for (b = 0, grbuf += 18; b < 32; b += 2, grbuf += 36)
        for (i = 1; i < 18; i += 2)
            grbuf[i] = -grbuf[i];
}

static void L3_imdct_gr(int32_t *grbuf, int32_t *overlap, unsigned block_type, unsigned n_long_bands) FL_NO_EXCEPT
{
    /* Selected with a conditional rather than through a two-entry pointer
       table like the float build uses. The table would be a relocated array,
       which costs a .rel entry and load-time relocation work on exactly the
       embedded targets this path exists for. */
    if (n_long_bands)
    {
        L3_imdct36(grbuf, overlap, g_mdct_window_normal_q30, n_long_bands);
        grbuf += 18*n_long_bands;
        overlap += 9*n_long_bands;
    }
    if (block_type == SHORT_BLOCK_TYPE)
        L3_imdct_short(grbuf, overlap, 32 - n_long_bands);
    else
        L3_imdct36(grbuf, overlap,
                   block_type == STOP_BLOCK_TYPE ? g_mdct_window_stop_q30
                                                 : g_mdct_window_normal_q30,
                   32 - n_long_bands);
}
#else
static void L3_dct3_9(float *y) FL_NO_EXCEPT
{
    float s0, s1, s2, s3, s4, s5, s6, s7, s8, t0, t2, t4;

    s0 = y[0]; s2 = y[2]; s4 = y[4]; s6 = y[6]; s8 = y[8];
    t0 = s0 + s6*0.5f;
    s0 -= s6;
    t4 = (s4 + s2)*0.93969262f;
    t2 = (s8 + s2)*0.76604444f;
    s6 = (s4 - s8)*0.17364818f;
    s4 += s8 - s2;

    s2 = s0 - s4*0.5f;
    y[4] = s4 + s0;
    s8 = t0 - t2 + s6;
    s0 = t0 - t4 + t2;
    s4 = t0 + t4 - s6;

    s1 = y[1]; s3 = y[3]; s5 = y[5]; s7 = y[7];

    s3 *= 0.86602540f;
    t0 = (s5 + s1)*0.98480775f;
    t4 = (s5 - s7)*0.34202014f;
    t2 = (s1 + s7)*0.64278761f;
    s1 = (s1 - s5 - s7)*0.86602540f;

    s5 = t0 - s3 - t2;
    s7 = t4 - s3 - t0;
    s3 = t4 + s3 - t2;

    y[0] = s4 - s7;
    y[1] = s2 + s1;
    y[2] = s0 - s3;
    y[3] = s8 + s5;
    y[5] = s8 - s5;
    y[6] = s0 + s3;
    y[7] = s2 - s1;
    y[8] = s4 + s7;
}

static void L3_imdct36(float *grbuf, float *overlap, const float *window, int nbands) FL_NO_EXCEPT
{
    int i, j;
    static const float g_twid9[18] = {
        0.73727734f,0.79335334f,0.84339145f,0.88701083f,0.92387953f,0.95371695f,0.97629601f,0.99144486f,0.99904822f,0.67559021f,0.60876143f,0.53729961f,0.46174861f,0.38268343f,0.30070580f,0.21643961f,0.13052619f,0.04361938f
    };

    for (j = 0; j < nbands; j++, grbuf += 18, overlap += 9)
    {
        float co[9], si[9];
        co[0] = -grbuf[0];
        si[0] = grbuf[17];
        for (i = 0; i < 4; i++)
        {
            si[8 - 2*i] =   grbuf[4*i + 1] - grbuf[4*i + 2];
            co[1 + 2*i] =   grbuf[4*i + 1] + grbuf[4*i + 2];
            si[7 - 2*i] =   grbuf[4*i + 4] - grbuf[4*i + 3];
            co[2 + 2*i] = -(grbuf[4*i + 3] + grbuf[4*i + 4]);
        }
        L3_dct3_9(co);
        L3_dct3_9(si);

        si[1] = -si[1];
        si[3] = -si[3];
        si[5] = -si[5];
        si[7] = -si[7];

        i = 0;

#if HAVE_SIMD
        if (have_simd()) for (; i < 8; i += 4)
        {
            f4 vovl = VLD(overlap + i);
            f4 vc = VLD(co + i);
            f4 vs = VLD(si + i);
            f4 vr0 = VLD(g_twid9 + i);
            f4 vr1 = VLD(g_twid9 + 9 + i);
            f4 vw0 = VLD(window + i);
            f4 vw1 = VLD(window + 9 + i);
            f4 vsum = VADD(VMUL(vc, vr1), VMUL(vs, vr0));
            VSTORE(overlap + i, VSUB(VMUL(vc, vr0), VMUL(vs, vr1)));
            VSTORE(grbuf + i, VSUB(VMUL(vovl, vw0), VMUL(vsum, vw1)));
            vsum = VADD(VMUL(vovl, vw1), VMUL(vsum, vw0));
            VSTORE(grbuf + 14 - i, VREV(vsum));
        }
#endif /* HAVE_SIMD */
        for (; i < 9; i++)
        {
            float ovl  = overlap[i];
            float sum  = co[i]*g_twid9[9 + i] + si[i]*g_twid9[0 + i];
            overlap[i] = co[i]*g_twid9[0 + i] - si[i]*g_twid9[9 + i];
            grbuf[i]      = ovl*window[0 + i] - sum*window[9 + i];
            grbuf[17 - i] = ovl*window[9 + i] + sum*window[0 + i];
        }
    }
}

static void L3_idct3(float x0, float x1, float x2, float *dst) FL_NO_EXCEPT
{
    float m1 = x1*0.86602540f;
    float a1 = x0 - x2*0.5f;
    dst[1] = x0 + x2;
    dst[0] = a1 + m1;
    dst[2] = a1 - m1;
}

static void L3_imdct12(float *x, float *dst, float *overlap) FL_NO_EXCEPT
{
    static const float g_twid3[6] = { 0.79335334f,0.92387953f,0.99144486f, 0.60876143f,0.38268343f,0.13052619f };
    float co[3], si[3];
    int i;

    L3_idct3(-x[0], x[6] + x[3], x[12] + x[9], co);
    L3_idct3(x[15], x[12] - x[9], x[6] - x[3], si);
    si[1] = -si[1];

    for (i = 0; i < 3; i++)
    {
        float ovl  = overlap[i];
        float sum  = co[i]*g_twid3[3 + i] + si[i]*g_twid3[0 + i];
        overlap[i] = co[i]*g_twid3[0 + i] - si[i]*g_twid3[3 + i];
        dst[i]     = ovl*g_twid3[2 - i] - sum*g_twid3[5 - i];
        dst[5 - i] = ovl*g_twid3[5 - i] + sum*g_twid3[2 - i];
    }
}

static void L3_imdct_short(float *grbuf, float *overlap, int nbands) FL_NO_EXCEPT
{
    for (;nbands > 0; nbands--, overlap += 9, grbuf += 18)
    {
        float tmp[18];
        memcpy(tmp, grbuf, sizeof(tmp));
        memcpy(grbuf, overlap, 6*sizeof(float));
        L3_imdct12(tmp, grbuf + 6, overlap + 6);
        L3_imdct12(tmp + 1, grbuf + 12, overlap + 6);
        L3_imdct12(tmp + 2, overlap, overlap + 6);
    }
}

static void L3_change_sign(float *grbuf) FL_NO_EXCEPT
{
    int b, i;
    for (b = 0, grbuf += 18; b < 32; b += 2, grbuf += 36)
        for (i = 1; i < 18; i += 2)
            grbuf[i] = -grbuf[i];
}

static void L3_imdct_gr(float *grbuf, float *overlap, unsigned block_type, unsigned n_long_bands) FL_NO_EXCEPT
{
    static const float g_mdct_window[2][18] = {
        { 0.99904822f,0.99144486f,0.97629601f,0.95371695f,0.92387953f,0.88701083f,0.84339145f,0.79335334f,0.73727734f,0.04361938f,0.13052619f,0.21643961f,0.30070580f,0.38268343f,0.46174861f,0.53729961f,0.60876143f,0.67559021f },
        { 1,1,1,1,1,1,0.99144486f,0.92387953f,0.79335334f,0,0,0,0,0,0,0.13052619f,0.38268343f,0.60876143f }
    };
    if (n_long_bands)
    {
        L3_imdct36(grbuf, overlap, g_mdct_window[0], n_long_bands);
        grbuf += 18*n_long_bands;
        overlap += 9*n_long_bands;
    }
    if (block_type == SHORT_BLOCK_TYPE)
        L3_imdct_short(grbuf, overlap, 32 - n_long_bands);
    else
        L3_imdct36(grbuf, overlap, g_mdct_window[block_type == STOP_BLOCK_TYPE], 32 - n_long_bands);
}
#endif /* MINIMP3_HAVE_FIXED_POINT */


static void L3_save_reservoir(mp3dec_t *h, mp3dec_scratch_internal_t *s) FL_NO_EXCEPT
{
    int pos = (s->bs.pos + 7)/8u;
    int remains = s->bs.limit/8u - pos;
    if (remains > MAX_BITRESERVOIR_BYTES)
    {
        pos += remains - MAX_BITRESERVOIR_BYTES;
        remains = MAX_BITRESERVOIR_BYTES;
    }
    if (remains > 0)
    {
        memmove(h->reserv_buf, s->maindata + pos, remains);
    }
    h->reserv = remains;
}

static int L3_restore_reservoir(mp3dec_t *h, bs_t *bs, mp3dec_scratch_internal_t *s, int main_data_begin) FL_NO_EXCEPT
{
    int frame_bytes = (bs->limit - bs->pos)/8;
    int bytes_have = MINIMP3_MIN(h->reserv, main_data_begin);
    memcpy(s->maindata, h->reserv_buf + MINIMP3_MAX(0, h->reserv - main_data_begin), MINIMP3_MIN(h->reserv, main_data_begin));
    memcpy(s->maindata + bytes_have, bs->buf + bs->pos/8, frame_bytes);
    bs_init(&s->bs, s->maindata, bytes_have + frame_bytes);
    return h->reserv >= main_data_begin;
}

/* The scalefactor gains reach the dequantiser as one array in the float build
   and as a mantissa/exponent pair in the fixed build; this keeps L3_decode
   itself identical between the two. */
#if MINIMP3_HAVE_FIXED_POINT
#define MP3D_SCF_ARGS(s) (s)->scf_mant, (s)->scf_exp
#else
#define MP3D_SCF_ARGS(s) (s)->scf
#endif

static void L3_decode(mp3dec_t *h, mp3dec_scratch_internal_t *s, L3_gr_info_t *gr_info, int nch) FL_NO_EXCEPT
{
    int ch;

    for (ch = 0; ch < nch; ch++)
    {
        int layer3gr_limit = s->bs.pos + gr_info[ch].part_23_length;
        L3_decode_scalefactors(h->header, s->ist_pos[ch], &s->bs, gr_info + ch, MP3D_SCF_ARGS(s), ch);
        L3_huffman(s->grbuf[ch], &s->bs, gr_info + ch, MP3D_SCF_ARGS(s), layer3gr_limit);
        MP3D_STAGE(MINIMP3_STAGE_HUFFMAN, ch, s->grbuf[ch], 576);
    }

    if (HDR_TEST_I_STEREO(h->header))
    {
        L3_intensity_stereo(s->grbuf[0], s->ist_pos[1], gr_info, h->header);
    } else if (HDR_IS_MS_STEREO(h->header))
    {
        L3_midside_stereo(s->grbuf[0], 576);
    }
    MP3D_STAGE(MINIMP3_STAGE_STEREO, 0, s->grbuf[0], 576*nch);

    for (ch = 0; ch < nch; ch++, gr_info++)
    {
        int aa_bands = 31;
        int n_long_bands = (gr_info->mixed_block_flag ? 2 : 0) << (int)(HDR_GET_MY_SAMPLE_RATE(h->header) == 2);

        if (gr_info->n_short_sfb)
        {
            aa_bands = n_long_bands - 1;
            L3_reorder(s->grbuf[ch] + n_long_bands*18,
                       h->qmf_state + 15*2*32,
                       gr_info->sfbtab + gr_info->n_long_sfb);
        }

        L3_antialias(s->grbuf[ch], aa_bands);
        MP3D_STAGE(MINIMP3_STAGE_ANTIALIAS, ch, s->grbuf[ch], 576);
        L3_imdct_gr(s->grbuf[ch], h->mdct_overlap[ch], gr_info->block_type, n_long_bands);
        L3_change_sign(s->grbuf[ch]);
        MP3D_STAGE(MINIMP3_STAGE_IMDCT, ch, s->grbuf[ch], 576);
    }
}

#if MINIMP3_HAVE_FIXED_POINT
/* DCT-32. Same factorisation as the float build. The secants reach 10.19 so
   they are Q27; the rotation constants are Q31 and the output scalings Q29,
   each the widest format that still holds its largest value.

   Every add saturates. On a real stream the intermediates stay well inside the
   Q26 range -- the measured pipeline peak is 0.654 -- but a fuzzed bitstream
   can drive dequantised samples to the +/-1 clamp, and this butterfly stacks
   three levels of adds on top of a 10.19x multiply. Saturating there turns a
   signed-overflow UB report into a bounded, audible-at-worst result. */
#if MP3D_HAVE_INT_SIMD
/* FastLED: integer vector helpers for the polyphase back-end.

   The polyphase filter is the one kernel where vectorising is bit-exact for
   free: it is a pure int32 x int32 -> int64 multiply-accumulate with no
   intermediate rounding or saturation, and int64 addition is exact and
   associative, so any lane arrangement reproduces the scalar result exactly.
   Every other kernel rounds and saturates per operation, which is why they are
   not vectorised -- see the disposition note on mp3d_synth below.

   The MUL_LO/MUL_HI pair takes two int32 lanes and returns two int64 products
   -- `LO` for lanes 0 and 1, `HI` for lanes 2 and 3. ADDSAT/SUBSAT/MULSHIFT are
   the four-lane forms of the scalar helpers of the same name and must match
   them exactly, including the symmetric saturation range. */
#if MP3D_INT_SIMD_NEON
typedef int64x2_t mp3d_i64x2;
#define MP3D_V_ZERO64()        vdupq_n_s64(0)
#define MP3D_V_LOAD4(p)        vld1q_s32((const int32_t *)(p))
#define MP3D_V_SPLAT(x)        vdupq_n_s32(x)
#define MP3D_V_PREP(v)         (v)
#define MP3D_V_STORE4(p, v)    vst1q_s32((int32_t *)(p), (v))
#define MP3D_V_MUL_LO(v, s)    vmull_s32(vget_low_s32(v), vget_low_s32(s))
#define MP3D_V_MUL_HI(v, s)    vmull_s32(vget_high_s32(v), vget_high_s32(s))
#define MP3D_V_ADD64(x, y)     vaddq_s64((x), (y))
#define MP3D_V_SUB64(x, y)     vsubq_s64((x), (y))
#define MP3D_V_GET64(x, lane)  ((lane) ? vgetq_lane_s64((x), 1) : vgetq_lane_s64((x), 0))
/* NEON multiplies signed 32x32 -> 64 natively and is in the ARM64 baseline, so
   there is nothing to detect. */
#define MP3D_SIMD_AVAILABLE()  1
#define MP3D_SIMD_TARGET

/* Saturating add/subtract. vqaddq_s32 saturates to INT32_MIN/MAX; the decoder's
   range is symmetric, so the extra vmaxq_s32 pulls INT32_MIN up to
   MP3D_SAT_MIN exactly as the scalar helper does. */
#define MP3D_V_ADDSAT(a, b)                                                    \
    vmaxq_s32(vqaddq_s32((a), (b)), vdupq_n_s32(MP3D_SAT_MIN))
#define MP3D_V_SUBSAT(a, b)                                                    \
    vmaxq_s32(vqsubq_s32((a), (b)), vdupq_n_s32(MP3D_SAT_MIN))

/* value * Q`bits` coefficient, rounded and saturated -- the vector form of
   mp3d_mulshift, and it must round the same way: add half, then shift right
   with sign extension (round half toward +infinity). */
static int32x4_t mp3d_v_mulshift(int32x4_t v, int32_t coef,
                                 int64x2_t round, int shift) FL_NO_EXCEPT
{
    const int32x2_t c = vdup_n_s32(coef);
    int64x2_t lo = vaddq_s64(vmull_s32(vget_low_s32(v), c), round);
    int64x2_t hi = vaddq_s64(vmull_s32(vget_high_s32(v), c), round);
    lo = vshlq_s64(lo, vdupq_n_s64(-shift));
    hi = vshlq_s64(hi, vdupq_n_s64(-shift));
    return vmaxq_s32(vcombine_s32(vqmovn_s64(lo), vqmovn_s64(hi)),
                     vdupq_n_s32(MP3D_SAT_MIN));
}
#define MP3D_V_MULSHIFT(v, coef, bits)                                         \
    mp3d_v_mulshift((v), (coef), vdupq_n_s64((int64_t)1 << ((bits) - 1)),      \
                    (bits))
#else /* MP3D_INT_SIMD_SSE */
typedef __m128i mp3d_i64x2;
#define MP3D_V_ZERO64()        _mm_setzero_si128()
#define MP3D_V_LOAD4(p)        _mm_loadu_si128((const __m128i *)(const void *)(p))
#define MP3D_V_SPLAT(x)        _mm_set1_epi32(x)
/* _mm_mul_epi32 multiplies the even int32 lanes; shuffling to (0,1),(2,3) once
   per vector lets both architectures share the accumulator bookkeeping. */
#define MP3D_V_PREP(v)         _mm_shuffle_epi32((v), _MM_SHUFFLE(3, 1, 2, 0))
#define MP3D_V_STORE4(p, v)    _mm_storeu_si128((__m128i *)(void *)(p), (v))
#define MP3D_V_MUL_LO(v, s)    _mm_mul_epi32((v), (s))
#define MP3D_V_MUL_HI(v, s)    _mm_mul_epi32(_mm_srli_si128((v), 4), (s))
#define MP3D_V_ADD64(x, y)     _mm_add_epi64((x), (y))
#define MP3D_V_SUB64(x, y)     _mm_sub_epi64((x), (y))

static int64_t mp3d_get_i64(__m128i v, int lane) FL_NO_EXCEPT
{
    int64_t out[2];
    _mm_storeu_si128((__m128i *)(void *)out, v);
    return out[lane];
}
#define MP3D_V_GET64(x, lane)  mp3d_get_i64((x), (lane))

/* Signed 32x32 -> 64 needs SSE4.1. SSE2 can emulate it with an unsigned
   multiply plus a sign correction, and that was measured rather than assumed:
   0.66x of scalar in a standalone harness, 0.95x inside the decoder. Slower is
   not worth shipping, so SSE2-only hardware stays on the scalar kernel and the
   vector path is chosen at run time -- the same shape as upstream's
   have_simd() dispatch for the float kernels. The same harness measures 1.71x
   once _mm_mul_epi32 is available. */
#if defined(__GNUC__) || defined(__clang__)
#define MP3D_SIMD_TARGET __attribute__((target("sse4.1")))
#else
#define MP3D_SIMD_TARGET
#endif

/* Self-contained: upstream's minimp3_cpuid lives inside the float SIMD block,
   which the fixed build switches off, so this path cannot borrow it. */
static int mp3d_have_sse41(void) FL_NO_EXCEPT
{
#if defined(__SSE4_1__)
    return 1;
#elif defined(_MSC_VER)
    static int cached;
    int info[4];
    if (!cached)
    {
        __cpuid(info, 1);
        cached = ((info[2] & (1 << 19)) != 0) + 1; /* ECX.SSE4_1 */
    }
    return cached - 1;
#elif defined(__GNUC__) || defined(__clang__)
    static int cached;
    unsigned eax, ebx, ecx, edx;
    if (!cached)
    {
        cached = (__get_cpuid(1, &eax, &ebx, &ecx, &edx) &&
                  (ecx & (1u << 19))) + 1;
    }
    return cached - 1;
#else
    return 0;
#endif
}
#define MP3D_SIMD_AVAILABLE()  mp3d_have_sse41()

/* Saturating add/subtract on four int32 lanes. SSE has saturating add only for
   8- and 16-bit lanes, so the 32-bit form is the classic branchless test: a
   signed add overflows exactly when both operands share a sign the result does
   not. The trailing _mm_max_epi32 enforces the decoder's symmetric range, the
   same trailing clamp the scalar helper carries. */
MP3D_SIMD_TARGET static __m128i mp3d_v_addsat(__m128i a, __m128i b) FL_NO_EXCEPT
{
    const __m128i sum = _mm_add_epi32(a, b);
    /* _mm_blendv_epi8 selects per byte on that byte's high bit, so every mask
       here is broadcast to a full lane with _mm_srai_epi32(.., 31) first --
       handing it a raw value blends bytes independently and silently produces
       a wrong answer in the low bits. */
    const __m128i overflow = _mm_srai_epi32(
        _mm_and_si128(_mm_xor_si128(a, sum), _mm_xor_si128(b, sum)), 31);
    const __m128i rail = _mm_blendv_epi8(_mm_set1_epi32(MP3D_SAT_MAX),
                                         _mm_set1_epi32(MP3D_SAT_MIN),
                                         _mm_srai_epi32(a, 31));
    return _mm_max_epi32(_mm_blendv_epi8(sum, rail, overflow),
                         _mm_set1_epi32(MP3D_SAT_MIN));
}

MP3D_SIMD_TARGET static __m128i mp3d_v_subsat(__m128i a, __m128i b) FL_NO_EXCEPT
{
    const __m128i diff = _mm_sub_epi32(a, b);
    const __m128i overflow = _mm_srai_epi32(
        _mm_and_si128(_mm_xor_si128(a, b), _mm_xor_si128(a, diff)), 31);
    const __m128i rail = _mm_blendv_epi8(_mm_set1_epi32(MP3D_SAT_MAX),
                                         _mm_set1_epi32(MP3D_SAT_MIN),
                                         _mm_srai_epi32(a, 31));
    return _mm_max_epi32(_mm_blendv_epi8(diff, rail, overflow),
                         _mm_set1_epi32(MP3D_SAT_MIN));
}

/* value * Q`bits` coefficient, rounded and saturated. SSE has no arithmetic
   64-bit shift before AVX-512, so the sign bits are folded back in by hand;
   and no 64-bit compare before SSE4.2, so the narrow detects out-of-range by
   checking that the high word is the sign extension of the low one. */
MP3D_SIMD_TARGET static __m128i mp3d_v_mulshift(__m128i v, int32_t coef,
                                                int bits) FL_NO_EXCEPT
{
    const __m128i c = _mm_set1_epi32(coef);
    const __m128i round = _mm_set1_epi64x((int64_t)1 << (bits - 1));
    const __m128i s = _mm_shuffle_epi32(v, _MM_SHUFFLE(3, 1, 2, 0));
    __m128i p01 = _mm_add_epi64(_mm_mul_epi32(s, c), round);
    __m128i p23 = _mm_add_epi64(_mm_mul_epi32(_mm_srli_si128(s, 4), c), round);
    const __m128i sign01 =
        _mm_srai_epi32(_mm_shuffle_epi32(p01, _MM_SHUFFLE(3, 3, 1, 1)), 31);
    const __m128i sign23 =
        _mm_srai_epi32(_mm_shuffle_epi32(p23, _MM_SHUFFLE(3, 3, 1, 1)), 31);
    p01 = _mm_or_si128(_mm_srli_epi64(p01, bits),
                       _mm_slli_epi64(sign01, 64 - bits));
    p23 = _mm_or_si128(_mm_srli_epi64(p23, bits),
                       _mm_slli_epi64(sign23, 64 - bits));
    {
        const __m128i lo = _mm_castps_si128(
            _mm_shuffle_ps(_mm_castsi128_ps(p01), _mm_castsi128_ps(p23),
                           _MM_SHUFFLE(2, 0, 2, 0)));
        const __m128i hi = _mm_castps_si128(
            _mm_shuffle_ps(_mm_castsi128_ps(p01), _mm_castsi128_ps(p23),
                           _MM_SHUFFLE(3, 1, 3, 1)));
        const __m128i in_range = _mm_cmpeq_epi32(hi, _mm_srai_epi32(lo, 31));
        const __m128i rail = _mm_blendv_epi8(_mm_set1_epi32(MP3D_SAT_MAX),
                                             _mm_set1_epi32(MP3D_SAT_MIN),
                                             _mm_srai_epi32(hi, 31));
        return _mm_max_epi32(_mm_blendv_epi8(rail, lo, in_range),
                             _mm_set1_epi32(MP3D_SAT_MIN));
    }
}
#define MP3D_V_ADDSAT(a, b)            mp3d_v_addsat((a), (b))
#define MP3D_V_SUBSAT(a, b)            mp3d_v_subsat((a), (b))
#define MP3D_V_MULSHIFT(v, coef, bits) mp3d_v_mulshift((v), (coef), (bits))
#endif

#endif /* MP3D_HAVE_INT_SIMD */

#if MP3D_HAVE_INT_SIMD
/* Four bands of the DCT-32 at once.

   grbuf is laid out band-major, so `y[i*18]` for four consecutive k values is
   four consecutive int32 -- the same property upstream's float kernel relies
   on, and the reason this vectorises without gathers.

   Transcribed operation for operation from the scalar version below rather
   than re-associated. That matters here in a way it did not for the polyphase:
   every add saturates and every multiply rounds, so reordering them is
   observable, and #4055's gate is exact equality with the scalar path. */
MP3D_SIMD_TARGET static void mp3d_dct2_bands4(int32_t *grbuf, int k) FL_NO_EXCEPT
{
    mp3d_i32x4 t[4][8], *x;
    int32_t *y = grbuf + k;
    int i;

    for (x = t[0], i = 0; i < 8; i++, x++)
    {
        const mp3d_i32x4 x0 = MP3D_V_LOAD4(&y[i*18]);
        const mp3d_i32x4 x1 = MP3D_V_LOAD4(&y[(15 - i)*18]);
        const mp3d_i32x4 x2 = MP3D_V_LOAD4(&y[(16 + i)*18]);
        const mp3d_i32x4 x3 = MP3D_V_LOAD4(&y[(31 - i)*18]);
        const mp3d_i32x4 t0 = MP3D_V_ADDSAT(x0, x3);
        const mp3d_i32x4 t1 = MP3D_V_ADDSAT(x1, x2);
        const mp3d_i32x4 t2 =
            MP3D_V_MULSHIFT(MP3D_V_SUBSAT(x1, x2), g_sec_q27[3*i + 0], 27);
        const mp3d_i32x4 t3 =
            MP3D_V_MULSHIFT(MP3D_V_SUBSAT(x0, x3), g_sec_q27[3*i + 1], 27);
        x[0]  = MP3D_V_ADDSAT(t0, t1);
        x[8]  = MP3D_V_MULSHIFT(MP3D_V_SUBSAT(t0, t1), g_sec_q27[3*i + 2], 27);
        x[16] = MP3D_V_ADDSAT(t3, t2);
        x[24] = MP3D_V_MULSHIFT(MP3D_V_SUBSAT(t3, t2), g_sec_q27[3*i + 2], 27);
    }
    for (x = t[0], i = 0; i < 4; i++, x += 8)
    {
        mp3d_i32x4 x0 = x[0], x1 = x[1], x2 = x[2], x3 = x[3];
        mp3d_i32x4 x4 = x[4], x5 = x[5], x6 = x[6], x7 = x[7], xt;
        xt = MP3D_V_SUBSAT(x0, x7); x0 = MP3D_V_ADDSAT(x0, x7);
        x7 = MP3D_V_SUBSAT(x1, x6); x1 = MP3D_V_ADDSAT(x1, x6);
        x6 = MP3D_V_SUBSAT(x2, x5); x2 = MP3D_V_ADDSAT(x2, x5);
        x5 = MP3D_V_SUBSAT(x3, x4); x3 = MP3D_V_ADDSAT(x3, x4);
        x4 = MP3D_V_SUBSAT(x0, x3); x0 = MP3D_V_ADDSAT(x0, x3);
        x3 = MP3D_V_SUBSAT(x1, x2); x1 = MP3D_V_ADDSAT(x1, x2);
        x[0] = MP3D_V_ADDSAT(x0, x1);
        x[4] = MP3D_V_MULSHIFT(MP3D_V_SUBSAT(x0, x1), MP3D_Q31_COS_PI_4, 31);
        x5 = MP3D_V_ADDSAT(x5, x6);
        x6 = MP3D_V_MULSHIFT(MP3D_V_ADDSAT(x6, x7), MP3D_Q31_COS_PI_4, 31);
        x7 = MP3D_V_ADDSAT(x7, xt);
        x3 = MP3D_V_MULSHIFT(MP3D_V_ADDSAT(x3, x4), MP3D_Q31_COS_PI_4, 31);
        x5 = MP3D_V_SUBSAT(x5, MP3D_V_MULSHIFT(x7, MP3D_Q31_TAN_PI_16, 31));
        x7 = MP3D_V_ADDSAT(x7, MP3D_V_MULSHIFT(x5, MP3D_Q31_SIN_PI_8, 31));
        x5 = MP3D_V_SUBSAT(x5, MP3D_V_MULSHIFT(x7, MP3D_Q31_TAN_PI_16, 31));
        x0 = MP3D_V_SUBSAT(xt, x6); xt = MP3D_V_ADDSAT(xt, x6);
        x[1] = MP3D_V_MULSHIFT(MP3D_V_ADDSAT(xt, x7), MP3D_Q29_SEC_PI_16, 29);
        x[2] = MP3D_V_MULSHIFT(MP3D_V_ADDSAT(x4, x3), MP3D_Q29_SEC_PI_8, 29);
        x[3] = MP3D_V_MULSHIFT(MP3D_V_SUBSAT(x0, x5), MP3D_Q29_SEC_3PI_16, 29);
        x[5] = MP3D_V_MULSHIFT(MP3D_V_ADDSAT(x0, x5), MP3D_Q29_SEC_5PI_16, 29);
        x[6] = MP3D_V_MULSHIFT(MP3D_V_SUBSAT(x4, x3), MP3D_Q29_SEC_3PI_8, 29);
        x[7] = MP3D_V_MULSHIFT(MP3D_V_SUBSAT(xt, x7), MP3D_Q29_SEC_7PI_16, 29);
    }
    for (i = 0; i < 7; i++, y += 4*18)
    {
        MP3D_V_STORE4(&y[0*18], t[0][i]);
        MP3D_V_STORE4(&y[1*18], MP3D_V_ADDSAT(MP3D_V_ADDSAT(t[2][i], t[3][i]),
                                              t[3][i + 1]));
        MP3D_V_STORE4(&y[2*18], MP3D_V_ADDSAT(t[1][i], t[1][i + 1]));
        MP3D_V_STORE4(&y[3*18],
                      MP3D_V_ADDSAT(MP3D_V_ADDSAT(t[2][i + 1], t[3][i]),
                                    t[3][i + 1]));
    }
    MP3D_V_STORE4(&y[0*18], t[0][7]);
    MP3D_V_STORE4(&y[1*18], MP3D_V_ADDSAT(t[2][7], t[3][7]));
    MP3D_V_STORE4(&y[2*18], t[1][7]);
    MP3D_V_STORE4(&y[3*18], t[3][7]);
}
#endif /* MP3D_HAVE_INT_SIMD */

static void mp3d_DCT_II(int32_t *grbuf, int n) FL_NO_EXCEPT
{
    int i, k = 0;
#if MP3D_HAVE_INT_SIMD
    if (MP3D_SIMD_AVAILABLE())
    {
        for (; k + 4 <= n; k += 4)
        {
            mp3d_dct2_bands4(grbuf, k);
        }
    }
#endif
    for (; k < n; k++)
    {
        int32_t t[4][8], *x, *y = grbuf + k;

        for (x = t[0], i = 0; i < 8; i++, x++)
        {
            const int32_t x0 = y[i*18];
            const int32_t x1 = y[(15 - i)*18];
            const int32_t x2 = y[(16 + i)*18];
            const int32_t x3 = y[(31 - i)*18];
            const int32_t t0 = mp3d_add_sat(x0, x3);
            const int32_t t1 = mp3d_add_sat(x1, x2);
            const int32_t t2 = mp3d_mulshift(mp3d_sub_sat(x1, x2), g_sec_q27[3*i + 0], 27);
            const int32_t t3 = mp3d_mulshift(mp3d_sub_sat(x0, x3), g_sec_q27[3*i + 1], 27);
            x[0]  = mp3d_add_sat(t0, t1);
            x[8]  = mp3d_mulshift(mp3d_sub_sat(t0, t1), g_sec_q27[3*i + 2], 27);
            x[16] = mp3d_add_sat(t3, t2);
            x[24] = mp3d_mulshift(mp3d_sub_sat(t3, t2), g_sec_q27[3*i + 2], 27);
        }
        for (x = t[0], i = 0; i < 4; i++, x += 8)
        {
            int32_t x0 = x[0], x1 = x[1], x2 = x[2], x3 = x[3];
            int32_t x4 = x[4], x5 = x[5], x6 = x[6], x7 = x[7], xt;
            xt = mp3d_sub_sat(x0, x7); x0 = mp3d_add_sat(x0, x7);
            x7 = mp3d_sub_sat(x1, x6); x1 = mp3d_add_sat(x1, x6);
            x6 = mp3d_sub_sat(x2, x5); x2 = mp3d_add_sat(x2, x5);
            x5 = mp3d_sub_sat(x3, x4); x3 = mp3d_add_sat(x3, x4);
            x4 = mp3d_sub_sat(x0, x3); x0 = mp3d_add_sat(x0, x3);
            x3 = mp3d_sub_sat(x1, x2); x1 = mp3d_add_sat(x1, x2);
            x[0] = mp3d_add_sat(x0, x1);
            x[4] = mp3d_mulshift(mp3d_sub_sat(x0, x1), MP3D_Q31_COS_PI_4, 31);
            x5 = mp3d_add_sat(x5, x6);
            x6 = mp3d_mulshift(mp3d_add_sat(x6, x7), MP3D_Q31_COS_PI_4, 31);
            x7 = mp3d_add_sat(x7, xt);
            x3 = mp3d_mulshift(mp3d_add_sat(x3, x4), MP3D_Q31_COS_PI_4, 31);
            /* rotate by PI/8 */
            x5 = mp3d_sub_sat(x5, mp3d_mulshift(x7, MP3D_Q31_TAN_PI_16, 31));
            x7 = mp3d_add_sat(x7, mp3d_mulshift(x5, MP3D_Q31_SIN_PI_8, 31));
            x5 = mp3d_sub_sat(x5, mp3d_mulshift(x7, MP3D_Q31_TAN_PI_16, 31));
            x0 = mp3d_sub_sat(xt, x6); xt = mp3d_add_sat(xt, x6);
            x[1] = mp3d_mulshift(mp3d_add_sat(xt, x7), MP3D_Q29_SEC_PI_16, 29);
            x[2] = mp3d_mulshift(mp3d_add_sat(x4, x3), MP3D_Q29_SEC_PI_8, 29);
            x[3] = mp3d_mulshift(mp3d_sub_sat(x0, x5), MP3D_Q29_SEC_3PI_16, 29);
            x[5] = mp3d_mulshift(mp3d_add_sat(x0, x5), MP3D_Q29_SEC_5PI_16, 29);
            x[6] = mp3d_mulshift(mp3d_sub_sat(x4, x3), MP3D_Q29_SEC_3PI_8, 29);
            x[7] = mp3d_mulshift(mp3d_sub_sat(xt, x7), MP3D_Q29_SEC_7PI_16, 29);
        }
        for (i = 0; i < 7; i++, y += 4*18)
        {
            y[0*18] = t[0][i];
            y[1*18] = mp3d_add_sat(mp3d_add_sat(t[2][i], t[3][i]), t[3][i + 1]);
            y[2*18] = mp3d_add_sat(t[1][i], t[1][i + 1]);
            y[3*18] = mp3d_add_sat(mp3d_add_sat(t[2][i + 1], t[3][i]), t[3][i + 1]);
        }
        y[0*18] = t[0][7];
        y[1*18] = mp3d_add_sat(t[2][7], t[3][7]);
        y[2*18] = t[1][7];
        y[3*18] = t[3][7];
    }
}

/* Q(MINIMP3_FRAC_BITS) accumulator to int16, reproducing the float build's
   rounding exactly: add a half, truncate toward zero, then step away from zero
   for negatives ("away from zero, to be compliant"). Matching it matters --
   the fixed-vs-float gate is measured in LSBs, and a different rounding rule
   alone would put a one-LSB difference on roughly every sample. */
#define MP3D_PCM_HALF   ((int64_t)1 << (MINIMP3_FRAC_BITS - 1))
#define MP3D_PCM_UPPER  (((int64_t)65533) << (MINIMP3_FRAC_BITS - 1))
#define MP3D_PCM_LOWER  (-(((int64_t)65535) << (MINIMP3_FRAC_BITS - 1)))

static mp3d_sample_t mp3d_scale_pcm(int64_t sample) FL_NO_EXCEPT
{
    int64_t t, s;
    if (sample >= MP3D_PCM_UPPER) return (int16_t) 32767;
    if (sample <= MP3D_PCM_LOWER) return (int16_t)-32768;
    t = sample + MP3D_PCM_HALF;
    s = t >= 0 ? (t >> MINIMP3_FRAC_BITS) : -((-t) >> MINIMP3_FRAC_BITS);
    s -= (s < 0);
    return (int16_t)s;
}

static void mp3d_synth_pair(mp3d_sample_t *pcm, int nch, const int32_t *z) FL_NO_EXCEPT
{
    int64_t a;
    a  = (int64_t)(z[14*64] - z[    0]) * 29;
    a += (int64_t)(z[ 1*64] + z[13*64]) * 213;
    a += (int64_t)(z[12*64] - z[ 2*64]) * 459;
    a += (int64_t)(z[ 3*64] + z[11*64]) * 2037;
    a += (int64_t)(z[10*64] - z[ 4*64]) * 5153;
    a += (int64_t)(z[ 5*64] + z[ 9*64]) * 6574;
    a += (int64_t)(z[ 8*64] - z[ 6*64]) * 37489;
    a += (int64_t) z[ 7*64]             * 75038;
    pcm[0] = mp3d_scale_pcm(a);

    z += 2;
    a  = (int64_t)z[14*64] * 104;
    a += (int64_t)z[12*64] * 1567;
    a += (int64_t)z[10*64] * 9727;
    a += (int64_t)z[ 8*64] * 64019;
    a += (int64_t)z[ 6*64] * -9975;
    a += (int64_t)z[ 4*64] * -45;
    a += (int64_t)z[ 2*64] * 146;
    a += (int64_t)z[ 0*64] * -5;
    pcm[16*nch] = mp3d_scale_pcm(a);
}

/* The polyphase back-end is where the pipeline's Q26 samples are scaled back
   up to int16, and it is the one place a 64-bit accumulator is genuinely
   required: the window coefficients reach 75038, so a single product already
   needs 48 bits before sixteen of them are summed. */
#if MP3D_HAVE_INT_SIMD
/* One iteration's eight window taps.

   Factored into its own function so the x86 build can put the SSE4.1 target
   attribute on exactly this code and reach it through a run-time check,
   without forcing the whole file to be compiled for a baseline the project
   does not require. The tap order and the arithmetic are identical to the
   scalar S0/S1/S2 chain: taps 1,3,5,7 accumulate `a` with the operands
   swapped, which is what S2 does, and the rest follow S0/S1. Starting the
   accumulators at zero makes S0's assignment and S1's accumulation the same
   operation. */
MP3D_SIMD_TARGET static void mp3d_synth_taps(const int32_t *zlin,
                                             const int32_t *w, int i,
                                             int64_t *a, int64_t *b) FL_NO_EXCEPT
{
    mp3d_i64x2 alo = MP3D_V_ZERO64(), ahi = MP3D_V_ZERO64();
    mp3d_i64x2 blo = MP3D_V_ZERO64(), bhi = MP3D_V_ZERO64();
    int k;

    for (k = 0; k < 8; k++)
    {
        const int32_t w0 = *w++;
        const int32_t w1 = *w++;
        const mp3d_i32x4 vz = MP3D_V_PREP(MP3D_V_LOAD4(&zlin[4*i - k*64]));
        const mp3d_i32x4 vy =
            MP3D_V_PREP(MP3D_V_LOAD4(&zlin[4*i - (15 - k)*64]));
        const mp3d_i32x4 s0 = MP3D_V_SPLAT(w0);
        const mp3d_i32x4 s1 = MP3D_V_SPLAT(w1);

        blo = MP3D_V_ADD64(blo, MP3D_V_ADD64(MP3D_V_MUL_LO(vz, s1),
                                             MP3D_V_MUL_LO(vy, s0)));
        bhi = MP3D_V_ADD64(bhi, MP3D_V_ADD64(MP3D_V_MUL_HI(vz, s1),
                                             MP3D_V_MUL_HI(vy, s0)));
        if (k & 1)
        {
            alo = MP3D_V_ADD64(alo, MP3D_V_SUB64(MP3D_V_MUL_LO(vy, s1),
                                                 MP3D_V_MUL_LO(vz, s0)));
            ahi = MP3D_V_ADD64(ahi, MP3D_V_SUB64(MP3D_V_MUL_HI(vy, s1),
                                                 MP3D_V_MUL_HI(vz, s0)));
        }
        else
        {
            alo = MP3D_V_ADD64(alo, MP3D_V_SUB64(MP3D_V_MUL_LO(vz, s0),
                                                 MP3D_V_MUL_LO(vy, s1)));
            ahi = MP3D_V_ADD64(ahi, MP3D_V_SUB64(MP3D_V_MUL_HI(vz, s0),
                                                 MP3D_V_MUL_HI(vy, s1)));
        }
    }

    a[0] = MP3D_V_GET64(alo, 0); a[1] = MP3D_V_GET64(alo, 1);
    a[2] = MP3D_V_GET64(ahi, 0); a[3] = MP3D_V_GET64(ahi, 1);
    b[0] = MP3D_V_GET64(blo, 0); b[1] = MP3D_V_GET64(blo, 1);
    b[2] = MP3D_V_GET64(bhi, 0); b[3] = MP3D_V_GET64(bhi, 1);
}
#endif /* MP3D_HAVE_INT_SIMD */

static void mp3d_synth(int32_t *xl, mp3d_sample_t *dstl, int nch, int32_t *lins) FL_NO_EXCEPT
{
    int i;
    int32_t *xr = xl + 576*(nch - 1);
    mp3d_sample_t *dstr = dstl + (nch - 1);

    static const int32_t g_win[] = {
        -1,26,-31,208,218,401,-519,2063,2000,4788,-5517,7134,5959,35640,-39336,74992,
        -1,24,-35,202,222,347,-581,2080,1952,4425,-5879,7640,5288,33791,-41176,74856,
        -1,21,-38,196,225,294,-645,2087,1893,4063,-6237,8092,4561,31947,-43006,74630,
        -1,19,-41,190,227,244,-711,2085,1822,3705,-6589,8492,3776,30112,-44821,74313,
        -1,17,-45,183,228,197,-779,2075,1739,3351,-6935,8840,2935,28289,-46617,73908,
        -1,16,-49,176,228,153,-848,2057,1644,3004,-7271,9139,2037,26482,-48390,73415,
        -2,14,-53,169,227,111,-919,2032,1535,2663,-7597,9389,1082,24694,-50137,72835,
        -2,13,-58,161,224,72,-991,2001,1414,2330,-7910,9592,70,22929,-51853,72169,
        -2,11,-63,154,221,36,-1064,1962,1280,2006,-8209,9750,-998,21189,-53534,71420,
        -2,10,-68,147,215,2,-1137,1919,1131,1692,-8491,9863,-2122,19478,-55178,70590,
        -3,9,-73,139,208,-29,-1210,1870,970,1388,-8755,9935,-3300,17799,-56778,69679,
        -3,8,-79,132,200,-57,-1283,1817,794,1095,-8998,9966,-4533,16155,-58333,68692,
        -4,7,-85,125,189,-83,-1356,1759,605,814,-9219,9959,-5818,14548,-59838,67629,
        -4,7,-91,117,177,-106,-1428,1698,402,545,-9416,9916,-7154,12980,-61289,66494,
        -5,6,-97,111,163,-127,-1498,1634,185,288,-9585,9838,-8540,11455,-62684,65290
    };
    int32_t *zlin = lins + 15*64;
    const int32_t *w = g_win;
#if MP3D_HAVE_INT_SIMD
    const int use_simd = MP3D_SIMD_AVAILABLE();
#endif

    zlin[4*15]     = xl[18*16];
    zlin[4*15 + 1] = xr[18*16];
    zlin[4*15 + 2] = xl[0];
    zlin[4*15 + 3] = xr[0];

    zlin[4*31]     = xl[1 + 18*16];
    zlin[4*31 + 1] = xr[1 + 18*16];
    zlin[4*31 + 2] = xl[1];
    zlin[4*31 + 3] = xr[1];

    mp3d_synth_pair(dstr, nch, lins + 4*15 + 1);
    mp3d_synth_pair(dstr + 32*nch, nch, lins + 4*15 + 64 + 1);
    mp3d_synth_pair(dstl, nch, lins + 4*15);
    mp3d_synth_pair(dstl + 32*nch, nch, lins + 4*15 + 64);

    for (i = 14; i >= 0; i--)
    {
#define LOAD(k) int32_t w0 = *w++; int32_t w1 = *w++; const int32_t *vz = &zlin[4*i - k*64]; const int32_t *vy = &zlin[4*i - (15 - k)*64];
#define S0(k) { int j; LOAD(k); for (j = 0; j < 4; j++) b[j]  = (int64_t)vz[j]*w1 + (int64_t)vy[j]*w0, a[j]  = (int64_t)vz[j]*w0 - (int64_t)vy[j]*w1; }
#define S1(k) { int j; LOAD(k); for (j = 0; j < 4; j++) b[j] += (int64_t)vz[j]*w1 + (int64_t)vy[j]*w0, a[j] += (int64_t)vz[j]*w0 - (int64_t)vy[j]*w1; }
#define S2(k) { int j; LOAD(k); for (j = 0; j < 4; j++) b[j] += (int64_t)vz[j]*w1 + (int64_t)vy[j]*w0, a[j] += (int64_t)vy[j]*w1 - (int64_t)vz[j]*w0; }
        int64_t a[4], b[4];

        zlin[4*i]     = xl[18*(31 - i)];
        zlin[4*i + 1] = xr[18*(31 - i)];
        zlin[4*i + 2] = xl[1 + 18*(31 - i)];
        zlin[4*i + 3] = xr[1 + 18*(31 - i)];
        zlin[4*(i + 16)]   = xl[1 + 18*(1 + i)];
        zlin[4*(i + 16) + 1] = xr[1 + 18*(1 + i)];
        zlin[4*(i - 16) + 2] = xl[18*(1 + i)];
        zlin[4*(i - 16) + 3] = xr[18*(1 + i)];

#if MP3D_HAVE_INT_SIMD
        if (use_simd)
        {
            mp3d_synth_taps(zlin, w, i, a, b);
            w += 16; /* the scalar chain below advances w as a side effect */
        }
        else
#endif
        {
        S0(0) S2(1) S1(2) S2(3) S1(4) S2(5) S1(6) S2(7)
        }

        dstr[(15 - i)*nch] = mp3d_scale_pcm(a[1]);
        dstr[(17 + i)*nch] = mp3d_scale_pcm(b[1]);
        dstl[(15 - i)*nch] = mp3d_scale_pcm(a[0]);
        dstl[(17 + i)*nch] = mp3d_scale_pcm(b[0]);
        dstr[(47 - i)*nch] = mp3d_scale_pcm(a[3]);
        dstr[(49 + i)*nch] = mp3d_scale_pcm(b[3]);
        dstl[(47 - i)*nch] = mp3d_scale_pcm(a[2]);
        dstl[(49 + i)*nch] = mp3d_scale_pcm(b[2]);
    }
}

static void mp3d_synth_granule(int32_t *qmf_state, int32_t *grbuf, int nbands,
                               int nch, mp3d_sample_t *pcm) FL_NO_EXCEPT
{
    int i;
    int32_t *lins = qmf_state;
    for (i = 0; i < nch; i++)
    {
        mp3d_DCT_II(grbuf + 576*i, nbands);
        MP3D_STAGE(MINIMP3_STAGE_DCT2, i, grbuf + 576*i, 18*nbands);
    }

    for (i = 0; i < nbands; i += 2)
    {
        mp3d_synth(grbuf + i, pcm + 32*nch*i, nch, lins + i*64);
    }
#ifndef MINIMP3_NONSTANDARD_BUT_LOGICAL
    if (nch == 1)
    {
        for (i = 0; i < 15*64; i += 2)
        {
            qmf_state[i] = lins[nbands*64 + i];
        }
    } else
#endif /* MINIMP3_NONSTANDARD_BUT_LOGICAL */
    {
        memmove(qmf_state, lins + nbands*64, sizeof(int32_t)*15*64);
    }
}
#else
static void mp3d_DCT_II(float *grbuf, int n) FL_NO_EXCEPT
{
    static const float g_sec[24] = {
        10.19000816f,0.50060302f,0.50241929f,3.40760851f,0.50547093f,0.52249861f,2.05778098f,0.51544732f,0.56694406f,1.48416460f,0.53104258f,0.64682180f,1.16943991f,0.55310392f,0.78815460f,0.97256821f,0.58293498f,1.06067765f,0.83934963f,0.62250412f,1.72244716f,0.74453628f,0.67480832f,5.10114861f
    };
    int i, k = 0;
#if HAVE_SIMD
    if (have_simd()) for (; k < n; k += 4)
    {
        f4 t[4][8], *x;
        float *y = grbuf + k;

        for (x = t[0], i = 0; i < 8; i++, x++)
        {
            f4 x0 = VLD(&y[i*18]);
            f4 x1 = VLD(&y[(15 - i)*18]);
            f4 x2 = VLD(&y[(16 + i)*18]);
            f4 x3 = VLD(&y[(31 - i)*18]);
            f4 t0 = VADD(x0, x3);
            f4 t1 = VADD(x1, x2);
            f4 t2 = VMUL_S(VSUB(x1, x2), g_sec[3*i + 0]);
            f4 t3 = VMUL_S(VSUB(x0, x3), g_sec[3*i + 1]);
            x[0] = VADD(t0, t1);
            x[8] = VMUL_S(VSUB(t0, t1), g_sec[3*i + 2]);
            x[16] = VADD(t3, t2);
            x[24] = VMUL_S(VSUB(t3, t2), g_sec[3*i + 2]);
        }
        for (x = t[0], i = 0; i < 4; i++, x += 8)
        {
            f4 x0 = x[0], x1 = x[1], x2 = x[2], x3 = x[3], x4 = x[4], x5 = x[5], x6 = x[6], x7 = x[7], xt;
            xt = VSUB(x0, x7); x0 = VADD(x0, x7);
            x7 = VSUB(x1, x6); x1 = VADD(x1, x6);
            x6 = VSUB(x2, x5); x2 = VADD(x2, x5);
            x5 = VSUB(x3, x4); x3 = VADD(x3, x4);
            x4 = VSUB(x0, x3); x0 = VADD(x0, x3);
            x3 = VSUB(x1, x2); x1 = VADD(x1, x2);
            x[0] = VADD(x0, x1);
            x[4] = VMUL_S(VSUB(x0, x1), 0.70710677f);
            x5 = VADD(x5, x6);
            x6 = VMUL_S(VADD(x6, x7), 0.70710677f);
            x7 = VADD(x7, xt);
            x3 = VMUL_S(VADD(x3, x4), 0.70710677f);
            x5 = VSUB(x5, VMUL_S(x7, 0.198912367f)); /* rotate by PI/8 */
            x7 = VADD(x7, VMUL_S(x5, 0.382683432f));
            x5 = VSUB(x5, VMUL_S(x7, 0.198912367f));
            x0 = VSUB(xt, x6); xt = VADD(xt, x6);
            x[1] = VMUL_S(VADD(xt, x7), 0.50979561f);
            x[2] = VMUL_S(VADD(x4, x3), 0.54119611f);
            x[3] = VMUL_S(VSUB(x0, x5), 0.60134488f);
            x[5] = VMUL_S(VADD(x0, x5), 0.89997619f);
            x[6] = VMUL_S(VSUB(x4, x3), 1.30656302f);
            x[7] = VMUL_S(VSUB(xt, x7), 2.56291556f);
        }

        if (k > n - 3)
        {
#if HAVE_SSE
#define VSAVE2(i, v) _mm_storel_pi((__m64 *)(void*)&y[i*18], v)
#else /* HAVE_SSE */
#define VSAVE2(i, v) vst1_f32((float32_t *)&y[i*18],  vget_low_f32(v))
#endif /* HAVE_SSE */
            for (i = 0; i < 7; i++, y += 4*18)
            {
                f4 s = VADD(t[3][i], t[3][i + 1]);
                VSAVE2(0, t[0][i]);
                VSAVE2(1, VADD(t[2][i], s));
                VSAVE2(2, VADD(t[1][i], t[1][i + 1]));
                VSAVE2(3, VADD(t[2][1 + i], s));
            }
            VSAVE2(0, t[0][7]);
            VSAVE2(1, VADD(t[2][7], t[3][7]));
            VSAVE2(2, t[1][7]);
            VSAVE2(3, t[3][7]);
        } else
        {
#define VSAVE4(i, v) VSTORE(&y[i*18], v)
            for (i = 0; i < 7; i++, y += 4*18)
            {
                f4 s = VADD(t[3][i], t[3][i + 1]);
                VSAVE4(0, t[0][i]);
                VSAVE4(1, VADD(t[2][i], s));
                VSAVE4(2, VADD(t[1][i], t[1][i + 1]));
                VSAVE4(3, VADD(t[2][1 + i], s));
            }
            VSAVE4(0, t[0][7]);
            VSAVE4(1, VADD(t[2][7], t[3][7]));
            VSAVE4(2, t[1][7]);
            VSAVE4(3, t[3][7]);
        }
    } else
#endif /* HAVE_SIMD */
#ifdef MINIMP3_ONLY_SIMD
    {} /* for HAVE_SIMD=1, MINIMP3_ONLY_SIMD=1 case we do not need non-intrinsic "else" branch */
#else /* MINIMP3_ONLY_SIMD */
    for (; k < n; k++)
    {
        float t[4][8], *x, *y = grbuf + k;

        for (x = t[0], i = 0; i < 8; i++, x++)
        {
            float x0 = y[i*18];
            float x1 = y[(15 - i)*18];
            float x2 = y[(16 + i)*18];
            float x3 = y[(31 - i)*18];
            float t0 = x0 + x3;
            float t1 = x1 + x2;
            float t2 = (x1 - x2)*g_sec[3*i + 0];
            float t3 = (x0 - x3)*g_sec[3*i + 1];
            x[0] = t0 + t1;
            x[8] = (t0 - t1)*g_sec[3*i + 2];
            x[16] = t3 + t2;
            x[24] = (t3 - t2)*g_sec[3*i + 2];
        }
        for (x = t[0], i = 0; i < 4; i++, x += 8)
        {
            float x0 = x[0], x1 = x[1], x2 = x[2], x3 = x[3], x4 = x[4], x5 = x[5], x6 = x[6], x7 = x[7], xt;
            xt = x0 - x7; x0 += x7;
            x7 = x1 - x6; x1 += x6;
            x6 = x2 - x5; x2 += x5;
            x5 = x3 - x4; x3 += x4;
            x4 = x0 - x3; x0 += x3;
            x3 = x1 - x2; x1 += x2;
            x[0] = x0 + x1;
            x[4] = (x0 - x1)*0.70710677f;
            x5 =  x5 + x6;
            x6 = (x6 + x7)*0.70710677f;
            x7 =  x7 + xt;
            x3 = (x3 + x4)*0.70710677f;
            x5 -= x7*0.198912367f;  /* rotate by PI/8 */
            x7 += x5*0.382683432f;
            x5 -= x7*0.198912367f;
            x0 = xt - x6; xt += x6;
            x[1] = (xt + x7)*0.50979561f;
            x[2] = (x4 + x3)*0.54119611f;
            x[3] = (x0 - x5)*0.60134488f;
            x[5] = (x0 + x5)*0.89997619f;
            x[6] = (x4 - x3)*1.30656302f;
            x[7] = (xt - x7)*2.56291556f;

        }
        for (i = 0; i < 7; i++, y += 4*18)
        {
            y[0*18] = t[0][i];
            y[1*18] = t[2][i] + t[3][i] + t[3][i + 1];
            y[2*18] = t[1][i] + t[1][i + 1];
            y[3*18] = t[2][i + 1] + t[3][i] + t[3][i + 1];
        }
        y[0*18] = t[0][7];
        y[1*18] = t[2][7] + t[3][7];
        y[2*18] = t[1][7];
        y[3*18] = t[3][7];
    }
#endif /* MINIMP3_ONLY_SIMD */
}

#ifndef MINIMP3_FLOAT_OUTPUT
static int16_t mp3d_scale_pcm(float sample) FL_NO_EXCEPT
{
#if HAVE_ARMV6
    int32_t s32 = (int32_t)(sample + .5f);
    s32 -= (s32 < 0);
    int16_t s = (int16_t)minimp3_clip_int16_arm(s32);
#else
    if (sample >=  32766.5) return (int16_t) 32767;
    if (sample <= -32767.5) return (int16_t)-32768;
    int16_t s = (int16_t)(sample + .5f);
    s -= (s < 0);   /* away from zero, to be compliant */
#endif
    return s;
}
#else /* MINIMP3_FLOAT_OUTPUT */
static float mp3d_scale_pcm(float sample)
{
    return sample*(1.f/32768.f);
}
#endif /* MINIMP3_FLOAT_OUTPUT */

static void mp3d_synth_pair(mp3d_sample_t *pcm, int nch, const float *z) FL_NO_EXCEPT
{
    float a;
    a  = (z[14*64] - z[    0]) * 29;
    a += (z[ 1*64] + z[13*64]) * 213;
    a += (z[12*64] - z[ 2*64]) * 459;
    a += (z[ 3*64] + z[11*64]) * 2037;
    a += (z[10*64] - z[ 4*64]) * 5153;
    a += (z[ 5*64] + z[ 9*64]) * 6574;
    a += (z[ 8*64] - z[ 6*64]) * 37489;
    a +=  z[ 7*64]             * 75038;
    pcm[0] = mp3d_scale_pcm(a);

    z += 2;
    a  = z[14*64] * 104;
    a += z[12*64] * 1567;
    a += z[10*64] * 9727;
    a += z[ 8*64] * 64019;
    a += z[ 6*64] * -9975;
    a += z[ 4*64] * -45;
    a += z[ 2*64] * 146;
    a += z[ 0*64] * -5;
    pcm[16*nch] = mp3d_scale_pcm(a);
}

static void mp3d_synth(float *xl, mp3d_sample_t *dstl, int nch, float *lins) FL_NO_EXCEPT
{
    int i;
    float *xr = xl + 576*(nch - 1);
    mp3d_sample_t *dstr = dstl + (nch - 1);

    static const float g_win[] = {
        -1,26,-31,208,218,401,-519,2063,2000,4788,-5517,7134,5959,35640,-39336,74992,
        -1,24,-35,202,222,347,-581,2080,1952,4425,-5879,7640,5288,33791,-41176,74856,
        -1,21,-38,196,225,294,-645,2087,1893,4063,-6237,8092,4561,31947,-43006,74630,
        -1,19,-41,190,227,244,-711,2085,1822,3705,-6589,8492,3776,30112,-44821,74313,
        -1,17,-45,183,228,197,-779,2075,1739,3351,-6935,8840,2935,28289,-46617,73908,
        -1,16,-49,176,228,153,-848,2057,1644,3004,-7271,9139,2037,26482,-48390,73415,
        -2,14,-53,169,227,111,-919,2032,1535,2663,-7597,9389,1082,24694,-50137,72835,
        -2,13,-58,161,224,72,-991,2001,1414,2330,-7910,9592,70,22929,-51853,72169,
        -2,11,-63,154,221,36,-1064,1962,1280,2006,-8209,9750,-998,21189,-53534,71420,
        -2,10,-68,147,215,2,-1137,1919,1131,1692,-8491,9863,-2122,19478,-55178,70590,
        -3,9,-73,139,208,-29,-1210,1870,970,1388,-8755,9935,-3300,17799,-56778,69679,
        -3,8,-79,132,200,-57,-1283,1817,794,1095,-8998,9966,-4533,16155,-58333,68692,
        -4,7,-85,125,189,-83,-1356,1759,605,814,-9219,9959,-5818,14548,-59838,67629,
        -4,7,-91,117,177,-106,-1428,1698,402,545,-9416,9916,-7154,12980,-61289,66494,
        -5,6,-97,111,163,-127,-1498,1634,185,288,-9585,9838,-8540,11455,-62684,65290
    };
    float *zlin = lins + 15*64;
    const float *w = g_win;

    zlin[4*15]     = xl[18*16];
    zlin[4*15 + 1] = xr[18*16];
    zlin[4*15 + 2] = xl[0];
    zlin[4*15 + 3] = xr[0];

    zlin[4*31]     = xl[1 + 18*16];
    zlin[4*31 + 1] = xr[1 + 18*16];
    zlin[4*31 + 2] = xl[1];
    zlin[4*31 + 3] = xr[1];

    mp3d_synth_pair(dstr, nch, lins + 4*15 + 1);
    mp3d_synth_pair(dstr + 32*nch, nch, lins + 4*15 + 64 + 1);
    mp3d_synth_pair(dstl, nch, lins + 4*15);
    mp3d_synth_pair(dstl + 32*nch, nch, lins + 4*15 + 64);

#if HAVE_SIMD
    if (have_simd()) for (i = 14; i >= 0; i--)
    {
#define VLOAD(k) f4 w0 = VSET(*w++); f4 w1 = VSET(*w++); f4 vz = VLD(&zlin[4*i - 64*k]); f4 vy = VLD(&zlin[4*i - 64*(15 - k)]);
#define V0(k) { VLOAD(k) b =         VADD(VMUL(vz, w1), VMUL(vy, w0)) ; a =         VSUB(VMUL(vz, w0), VMUL(vy, w1));  }
#define V1(k) { VLOAD(k) b = VADD(b, VADD(VMUL(vz, w1), VMUL(vy, w0))); a = VADD(a, VSUB(VMUL(vz, w0), VMUL(vy, w1))); }
#define V2(k) { VLOAD(k) b = VADD(b, VADD(VMUL(vz, w1), VMUL(vy, w0))); a = VADD(a, VSUB(VMUL(vy, w1), VMUL(vz, w0))); }
        f4 a, b;
        zlin[4*i]     = xl[18*(31 - i)];
        zlin[4*i + 1] = xr[18*(31 - i)];
        zlin[4*i + 2] = xl[1 + 18*(31 - i)];
        zlin[4*i + 3] = xr[1 + 18*(31 - i)];
        zlin[4*i + 64] = xl[1 + 18*(1 + i)];
        zlin[4*i + 64 + 1] = xr[1 + 18*(1 + i)];
        zlin[4*i - 64 + 2] = xl[18*(1 + i)];
        zlin[4*i - 64 + 3] = xr[18*(1 + i)];

        V0(0) V2(1) V1(2) V2(3) V1(4) V2(5) V1(6) V2(7)

        {
#ifndef MINIMP3_FLOAT_OUTPUT
#if HAVE_SSE
            static const f4 g_max = { 32767.0f, 32767.0f, 32767.0f, 32767.0f };
            static const f4 g_min = { -32768.0f, -32768.0f, -32768.0f, -32768.0f };
            __m128i pcm8 = _mm_packs_epi32(_mm_cvtps_epi32(_mm_max_ps(_mm_min_ps(a, g_max), g_min)),
                                           _mm_cvtps_epi32(_mm_max_ps(_mm_min_ps(b, g_max), g_min)));
            dstr[(15 - i)*nch] = _mm_extract_epi16(pcm8, 1);
            dstr[(17 + i)*nch] = _mm_extract_epi16(pcm8, 5);
            dstl[(15 - i)*nch] = _mm_extract_epi16(pcm8, 0);
            dstl[(17 + i)*nch] = _mm_extract_epi16(pcm8, 4);
            dstr[(47 - i)*nch] = _mm_extract_epi16(pcm8, 3);
            dstr[(49 + i)*nch] = _mm_extract_epi16(pcm8, 7);
            dstl[(47 - i)*nch] = _mm_extract_epi16(pcm8, 2);
            dstl[(49 + i)*nch] = _mm_extract_epi16(pcm8, 6);
#else /* HAVE_SSE */
            int16x4_t pcma, pcmb;
            a = VADD(a, VSET(0.5f));
            b = VADD(b, VSET(0.5f));
            pcma = vqmovn_s32(vqaddq_s32(vcvtq_s32_f32(a), vreinterpretq_s32_u32(vcltq_f32(a, VSET(0)))));
            pcmb = vqmovn_s32(vqaddq_s32(vcvtq_s32_f32(b), vreinterpretq_s32_u32(vcltq_f32(b, VSET(0)))));
            vst1_lane_s16(dstr + (15 - i)*nch, pcma, 1);
            vst1_lane_s16(dstr + (17 + i)*nch, pcmb, 1);
            vst1_lane_s16(dstl + (15 - i)*nch, pcma, 0);
            vst1_lane_s16(dstl + (17 + i)*nch, pcmb, 0);
            vst1_lane_s16(dstr + (47 - i)*nch, pcma, 3);
            vst1_lane_s16(dstr + (49 + i)*nch, pcmb, 3);
            vst1_lane_s16(dstl + (47 - i)*nch, pcma, 2);
            vst1_lane_s16(dstl + (49 + i)*nch, pcmb, 2);
#endif /* HAVE_SSE */

#else /* MINIMP3_FLOAT_OUTPUT */

            static const f4 g_scale = { 1.0f/32768.0f, 1.0f/32768.0f, 1.0f/32768.0f, 1.0f/32768.0f };
            a = VMUL(a, g_scale);
            b = VMUL(b, g_scale);
#if HAVE_SSE
            _mm_store_ss(dstr + (15 - i)*nch, _mm_shuffle_ps(a, a, _MM_SHUFFLE(1, 1, 1, 1)));
            _mm_store_ss(dstr + (17 + i)*nch, _mm_shuffle_ps(b, b, _MM_SHUFFLE(1, 1, 1, 1)));
            _mm_store_ss(dstl + (15 - i)*nch, _mm_shuffle_ps(a, a, _MM_SHUFFLE(0, 0, 0, 0)));
            _mm_store_ss(dstl + (17 + i)*nch, _mm_shuffle_ps(b, b, _MM_SHUFFLE(0, 0, 0, 0)));
            _mm_store_ss(dstr + (47 - i)*nch, _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 3, 3, 3)));
            _mm_store_ss(dstr + (49 + i)*nch, _mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 3, 3, 3)));
            _mm_store_ss(dstl + (47 - i)*nch, _mm_shuffle_ps(a, a, _MM_SHUFFLE(2, 2, 2, 2)));
            _mm_store_ss(dstl + (49 + i)*nch, _mm_shuffle_ps(b, b, _MM_SHUFFLE(2, 2, 2, 2)));
#else /* HAVE_SSE */
            vst1q_lane_f32(dstr + (15 - i)*nch, a, 1);
            vst1q_lane_f32(dstr + (17 + i)*nch, b, 1);
            vst1q_lane_f32(dstl + (15 - i)*nch, a, 0);
            vst1q_lane_f32(dstl + (17 + i)*nch, b, 0);
            vst1q_lane_f32(dstr + (47 - i)*nch, a, 3);
            vst1q_lane_f32(dstr + (49 + i)*nch, b, 3);
            vst1q_lane_f32(dstl + (47 - i)*nch, a, 2);
            vst1q_lane_f32(dstl + (49 + i)*nch, b, 2);
#endif /* HAVE_SSE */
#endif /* MINIMP3_FLOAT_OUTPUT */
        }
    } else
#endif /* HAVE_SIMD */
#ifdef MINIMP3_ONLY_SIMD
    {} /* for HAVE_SIMD=1, MINIMP3_ONLY_SIMD=1 case we do not need non-intrinsic "else" branch */
#else /* MINIMP3_ONLY_SIMD */
    for (i = 14; i >= 0; i--)
    {
#define LOAD(k) float w0 = *w++; float w1 = *w++; float *vz = &zlin[4*i - k*64]; float *vy = &zlin[4*i - (15 - k)*64];
#define S0(k) { int j; LOAD(k); for (j = 0; j < 4; j++) b[j]  = vz[j]*w1 + vy[j]*w0, a[j]  = vz[j]*w0 - vy[j]*w1; }
#define S1(k) { int j; LOAD(k); for (j = 0; j < 4; j++) b[j] += vz[j]*w1 + vy[j]*w0, a[j] += vz[j]*w0 - vy[j]*w1; }
#define S2(k) { int j; LOAD(k); for (j = 0; j < 4; j++) b[j] += vz[j]*w1 + vy[j]*w0, a[j] += vy[j]*w1 - vz[j]*w0; }
        float a[4], b[4];

        zlin[4*i]     = xl[18*(31 - i)];
        zlin[4*i + 1] = xr[18*(31 - i)];
        zlin[4*i + 2] = xl[1 + 18*(31 - i)];
        zlin[4*i + 3] = xr[1 + 18*(31 - i)];
        zlin[4*(i + 16)]   = xl[1 + 18*(1 + i)];
        zlin[4*(i + 16) + 1] = xr[1 + 18*(1 + i)];
        zlin[4*(i - 16) + 2] = xl[18*(1 + i)];
        zlin[4*(i - 16) + 3] = xr[18*(1 + i)];

        S0(0) S2(1) S1(2) S2(3) S1(4) S2(5) S1(6) S2(7)

        dstr[(15 - i)*nch] = mp3d_scale_pcm(a[1]);
        dstr[(17 + i)*nch] = mp3d_scale_pcm(b[1]);
        dstl[(15 - i)*nch] = mp3d_scale_pcm(a[0]);
        dstl[(17 + i)*nch] = mp3d_scale_pcm(b[0]);
        dstr[(47 - i)*nch] = mp3d_scale_pcm(a[3]);
        dstr[(49 + i)*nch] = mp3d_scale_pcm(b[3]);
        dstl[(47 - i)*nch] = mp3d_scale_pcm(a[2]);
        dstl[(49 + i)*nch] = mp3d_scale_pcm(b[2]);
    }
#endif /* MINIMP3_ONLY_SIMD */
}

static void mp3d_synth_granule(float *qmf_state, float *grbuf, int nbands,
                               int nch, mp3d_sample_t *pcm) FL_NO_EXCEPT
{
    int i;
    float *lins = qmf_state;
    for (i = 0; i < nch; i++)
    {
        mp3d_DCT_II(grbuf + 576*i, nbands);
        MP3D_STAGE(MINIMP3_STAGE_DCT2, i, grbuf + 576*i, 18*nbands);
    }

    for (i = 0; i < nbands; i += 2)
    {
        mp3d_synth(grbuf + i, pcm + 32*nch*i, nch, lins + i*64);
    }
#ifndef MINIMP3_NONSTANDARD_BUT_LOGICAL
    if (nch == 1)
    {
        for (i = 0; i < 15*64; i += 2)
        {
            qmf_state[i] = lins[nbands*64 + i];
        }
    } else
#endif /* MINIMP3_NONSTANDARD_BUT_LOGICAL */
    {
        memmove(qmf_state, lins + nbands*64, sizeof(float)*15*64);
    }
}

#endif /* MINIMP3_HAVE_FIXED_POINT */

static int mp3d_match_frame(const uint8_t *hdr, int mp3_bytes, int frame_bytes) FL_NO_EXCEPT
{
    int i, nmatch;
    for (i = 0, nmatch = 0; nmatch < MAX_FRAME_SYNC_MATCHES; nmatch++)
    {
        i += hdr_frame_bytes(hdr + i, frame_bytes) + hdr_padding(hdr + i);
        if (i + HDR_SIZE > mp3_bytes)
            return nmatch > 0;
        if (!hdr_compare(hdr, hdr + i))
            return 0;
    }
    return 1;
}

static int mp3d_find_frame(const uint8_t *mp3, int mp3_bytes, int *free_format_bytes, int *ptr_frame_bytes) FL_NO_EXCEPT
{
    int i, k;
    for (i = 0; i < mp3_bytes - HDR_SIZE; i++, mp3++)
    {
        if (hdr_valid(mp3))
        {
            int frame_bytes = hdr_frame_bytes(mp3, *free_format_bytes);
            int frame_and_padding = frame_bytes + hdr_padding(mp3);

            for (k = HDR_SIZE; !frame_bytes && k < MAX_FREE_FORMAT_FRAME_SIZE && i + 2*k < mp3_bytes - HDR_SIZE; k++)
            {
                if (hdr_compare(mp3, mp3 + k))
                {
                    int fb = k - hdr_padding(mp3);
                    int nextfb = fb + hdr_padding(mp3 + k);
                    if (i + k + nextfb + HDR_SIZE > mp3_bytes || !hdr_compare(mp3, mp3 + k + nextfb))
                        continue;
                    frame_and_padding = k;
                    frame_bytes = fb;
                    *free_format_bytes = fb;
                }
            }
            if ((frame_bytes && i + frame_and_padding <= mp3_bytes &&
                mp3d_match_frame(mp3, mp3_bytes - i, frame_bytes)) ||
                (!i && frame_and_padding == mp3_bytes))
            {
                *ptr_frame_bytes = frame_and_padding;
                return i;
            }
            *free_format_bytes = 0;
        }
    }
    *ptr_frame_bytes = 0;
    return mp3_bytes;
}

int mp3dec_is_fixed_point(void) FL_NO_EXCEPT
{
    return MINIMP3_HAVE_FIXED_POINT;
}

int mp3dec_dsp_is_integer(void) FL_NO_EXCEPT
{
    return MINIMP3_DSP_INTEGER;
}

int mp3dec_dsp_uses_simd(void) FL_NO_EXCEPT
{
    return MP3D_SIMD_KERNELS_LIVE;
}

void mp3dec_init(mp3dec_t *dec) FL_NO_EXCEPT
{
    dec->header[0] = 0;
}

int mp3dec_decode_frame_r(mp3dec_t *dec, mp3dec_scratch_t *scratch_storage,
                          const uint8_t *mp3, int mp3_bytes,
                          mp3d_sample_t *pcm,
                          mp3dec_frame_info_t *info) FL_NO_EXCEPT
{
    int i = 0, igr, frame_size = 0, success = 1;
    const uint8_t *hdr;
    bs_t bs_frame[1];
    mp3dec_scratch_internal_t *scratch =
        (mp3dec_scratch_internal_t *)(void *)scratch_storage->buffer;

    if (mp3_bytes > 4 && dec->header[0] == 0xff && hdr_compare(dec->header, mp3))
    {
        frame_size = hdr_frame_bytes(mp3, dec->free_format_bytes) + hdr_padding(mp3);
        if (frame_size != mp3_bytes && (frame_size + HDR_SIZE > mp3_bytes || !hdr_compare(mp3, mp3 + frame_size)))
        {
            frame_size = 0;
        }
    }
    if (!frame_size)
    {
        memset(dec, 0, sizeof(mp3dec_t));
        i = mp3d_find_frame(mp3, mp3_bytes, &dec->free_format_bytes, &frame_size);
        if (!frame_size || i + frame_size > mp3_bytes)
        {
            info->frame_bytes = i;
            return 0;
        }
    }

    hdr = mp3 + i;
    memcpy(dec->header, hdr, HDR_SIZE);
    info->frame_bytes = i + frame_size;
    info->frame_offset = i;
    info->channels = HDR_IS_MONO(hdr) ? 1 : 2;
    info->hz = hdr_sample_rate_hz(hdr);
    info->layer = 4 - HDR_GET_LAYER(hdr);
    info->bitrate_kbps = hdr_bitrate_kbps(hdr);

    if (!pcm)
    {
        return hdr_frame_samples(hdr);
    }

    bs_init(bs_frame, hdr + HDR_SIZE, frame_size - HDR_SIZE);
    if (HDR_IS_CRC(hdr))
    {
        get_bits(bs_frame, 16);
    }

    if (info->layer == 3)
    {
        int main_data_begin = L3_read_side_info(bs_frame, scratch->gr_info, hdr);
        if (main_data_begin < 0 || bs_frame->pos > bs_frame->limit)
        {
            mp3dec_init(dec);
            return 0;
        }
        success = L3_restore_reservoir(dec, bs_frame, scratch, main_data_begin);
        if (success)
        {
            for (igr = 0; igr < (HDR_TEST_MPEG1(hdr) ? 2 : 1); igr++, pcm += 576*info->channels)
            {
                memset(scratch->grbuf[0], 0, 576*2*sizeof(mp3d_dsp_t));
                L3_decode(dec, scratch, scratch->gr_info + igr*info->channels, info->channels);
                mp3d_synth_granule(dec->qmf_state, scratch->grbuf[0], 18, info->channels, pcm);
            }
        }
        L3_save_reservoir(dec, scratch);
    } else
    {
#ifdef MINIMP3_ONLY_MP3
        return 0;
#else /* MINIMP3_ONLY_MP3 */
        L12_scale_info sci[1];
        L12_read_scale_info(hdr, bs_frame, sci);

        memset(scratch->grbuf[0], 0, 576*2*sizeof(mp3d_dsp_t));
        for (i = 0, igr = 0; igr < 3; igr++)
        {
            if (12 == (i += L12_dequantize_granule(scratch->grbuf[0] + i, bs_frame, sci, info->layer | 1)))
            {
                i = 0;
#if MINIMP3_HAVE_FIXED_POINT
                L12_apply_scf_384(sci, sci->scf_mant + igr, sci->scf_exp + igr, scratch->grbuf[0]);
#else
                L12_apply_scf_384(sci, sci->scf + igr, scratch->grbuf[0]);
#endif
                mp3d_synth_granule(dec->qmf_state, scratch->grbuf[0], 12, info->channels, pcm);
                memset(scratch->grbuf[0], 0, 576*2*sizeof(mp3d_dsp_t));
                pcm += 384*info->channels;
            }
            if (bs_frame->pos > bs_frame->limit)
            {
                mp3dec_init(dec);
                return 0;
            }
        }
#endif /* MINIMP3_ONLY_MP3 */
    }
    return success*hdr_frame_samples(dec->header);
}

#ifdef MINIMP3_FLOAT_OUTPUT
void mp3dec_f32_to_s16(const float *in, int16_t *out, int num_samples) FL_NO_EXCEPT
{
    int i = 0;
#if HAVE_SIMD
    int aligned_count = num_samples & ~7;
    for(; i < aligned_count; i += 8)
    {
        static const f4 g_scale = { 32768.0f, 32768.0f, 32768.0f, 32768.0f };
        f4 a = VMUL(VLD(&in[i  ]), g_scale);
        f4 b = VMUL(VLD(&in[i+4]), g_scale);
#if HAVE_SSE
        static const f4 g_max = { 32767.0f, 32767.0f, 32767.0f, 32767.0f };
        static const f4 g_min = { -32768.0f, -32768.0f, -32768.0f, -32768.0f };
        __m128i pcm8 = _mm_packs_epi32(_mm_cvtps_epi32(_mm_max_ps(_mm_min_ps(a, g_max), g_min)),
                                       _mm_cvtps_epi32(_mm_max_ps(_mm_min_ps(b, g_max), g_min)));
        out[i  ] = _mm_extract_epi16(pcm8, 0);
        out[i+1] = _mm_extract_epi16(pcm8, 1);
        out[i+2] = _mm_extract_epi16(pcm8, 2);
        out[i+3] = _mm_extract_epi16(pcm8, 3);
        out[i+4] = _mm_extract_epi16(pcm8, 4);
        out[i+5] = _mm_extract_epi16(pcm8, 5);
        out[i+6] = _mm_extract_epi16(pcm8, 6);
        out[i+7] = _mm_extract_epi16(pcm8, 7);
#else /* HAVE_SSE */
        int16x4_t pcma, pcmb;
        a = VADD(a, VSET(0.5f));
        b = VADD(b, VSET(0.5f));
        pcma = vqmovn_s32(vqaddq_s32(vcvtq_s32_f32(a), vreinterpretq_s32_u32(vcltq_f32(a, VSET(0)))));
        pcmb = vqmovn_s32(vqaddq_s32(vcvtq_s32_f32(b), vreinterpretq_s32_u32(vcltq_f32(b, VSET(0)))));
        vst1_lane_s16(out+i  , pcma, 0);
        vst1_lane_s16(out+i+1, pcma, 1);
        vst1_lane_s16(out+i+2, pcma, 2);
        vst1_lane_s16(out+i+3, pcma, 3);
        vst1_lane_s16(out+i+4, pcmb, 0);
        vst1_lane_s16(out+i+5, pcmb, 1);
        vst1_lane_s16(out+i+6, pcmb, 2);
        vst1_lane_s16(out+i+7, pcmb, 3);
#endif /* HAVE_SSE */
    }
#endif /* HAVE_SIMD */
    for(; i < num_samples; i++)
    {
        float sample = in[i] * 32768.0f;
        if (sample >=  32766.5)
            out[i] = (int16_t) 32767;
        else if (sample <= -32767.5)
            out[i] = (int16_t)-32768;
        else
        {
            int16_t s = (int16_t)(sample + .5f);
            s -= (s < 0);   /* away from zero, to be compliant */
            out[i] = s;
        }
    }
}
#endif /* MINIMP3_FLOAT_OUTPUT */
#ifdef __cplusplus
} /* namespace MINIMP3_NAMESPACE */
} /* namespace fl */
#endif

/* FastLED: release every macro the implementation owns so the header can be
   included a second time in the same translation unit under a different
   MINIMP3_NAMESPACE. Without this the fixed variant would inherit the float
   variant's SIMD configuration (HAVE_SIMD / MINIMP3_ONLY_SIMD in particular,
   which would compile the scalar integer kernels out entirely) and every
   redefinition that differs between the two would be a hard error. */
#undef MP3D_STAGE
#undef MP3D_SCF_ARGS
#undef MP3D_HUFF_SCF_ARGS
#undef MP3D_HUFF_ONE_VARS
#undef MP3D_HUFF_NEXT_SCF
#undef MP3D_HUFF_ESC
#undef MP3D_HUFF_TAB
#undef MP3D_HUFF_ONE
#undef MP3D_SAT_MAX
#undef MP3D_SAT_MIN
#undef MP3D_ONE
#undef MP3D_PCM_HALF
#undef MP3D_PCM_UPPER
#undef MP3D_PCM_LOWER
/* Only release MINIMP3_NO_SIMD if this header is what set it; a caller that
   asked for the scalar build must keep getting it on a re-include. */
#undef MP3D_FLOAT_SIMD_OFF
#undef MP3D_V_ZERO64
#undef MP3D_V_LOAD4
#undef MP3D_V_SPLAT
#undef MP3D_V_MUL_LO
#undef MP3D_V_MUL_HI
#undef MP3D_V_ADD64
#undef MP3D_V_SUB64
#undef MP3D_V_GET64
#undef MP3D_V_PREP
#undef MP3D_V_SIGN
#undef MP3D_V_ADDSAT
#undef MP3D_V_SUBSAT
#undef MP3D_V_MULSHIFT
#undef MP3D_V_STORE4
#undef MP3D_HAVE_INT_SIMD
#undef MP3D_INT_SIMD_SSE
#undef MP3D_INT_SIMD_NEON
#undef MP3D_SIMD_KERNELS_LIVE
#undef BITS_DEQUANTIZER_OUT
#undef BSPOS
#undef CHECK_BITS
#undef DEQ_COUNT1
#undef DQ
#undef FLUSH_BITS
#undef HAVE_ARMV6
#undef HAVE_SIMD
#undef HAVE_SSE
#undef HDR_GET_BITRATE
#undef HDR_GET_LAYER
#undef HDR_GET_MY_SAMPLE_RATE
#undef HDR_GET_SAMPLE_RATE
#undef HDR_GET_STEREO_MODE
#undef HDR_GET_STEREO_MODE_EXT
#undef HDR_IS_CRC
#undef HDR_IS_FRAME_576
#undef HDR_IS_FREE_FORMAT
#undef HDR_IS_LAYER_1
#undef HDR_IS_MONO
#undef HDR_IS_MS_STEREO
#undef HDR_SIZE
#undef HDR_TEST_I_STEREO
#undef HDR_TEST_MPEG1
#undef HDR_TEST_MS_STEREO
#undef HDR_TEST_NOT_MPEG25
#undef HDR_TEST_PADDING
#undef LOAD
#undef MAX_BITRESERVOIR_BYTES
#undef MAX_FRAME_SYNC_MATCHES
#undef MAX_FREE_FORMAT_FRAME_SIZE
#undef MAX_L3_FRAME_PAYLOAD_BYTES
#undef MAX_SCF
#undef MAX_SCFI
#undef minimp3_cpuid
#undef MINIMP3_MAX
#undef MINIMP3_MIN
#undef MINIMP3_ONLY_SIMD
#undef MODE_JOINT_STEREO
#undef MODE_MONO
#undef PEEK_BITS
#undef RELOAD_SCALEFACTOR
#undef S0
#undef S1
#undef S2
#undef SHORT_BLOCK_TYPE
#undef STOP_BLOCK_TYPE
#undef V0
#undef V1
#undef V2
#undef VADD
#undef VLD
#undef VLOAD
#undef VMAC
#undef VMSB
#undef VMUL
#undef VMUL_S
#undef VREV
#undef VSAVE2
#undef VSAVE4
#undef VSET
#undef VSTORE
#undef VSUB

#endif /* MINIMP3_IMPLEMENTATION && !_MINIMP3_IMPLEMENTATION_GUARD */
