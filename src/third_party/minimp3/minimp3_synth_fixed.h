#pragma once

/* The fixed-point synthesis back-end: DCT-32, the polyphase filterbank and the
   integer SIMD kernels they dispatch to. This is 57% of a decode by host
   instruction count and holds the one stage where this decoder still loses to
   the Helix reference on RISC-V, so it is where optimisation work happens.
   It is split out of minimp3.h so that work has a single file to edit.

   This is not a standalone header. It is textually included from minimp3.h at
   one point, inside `#if MINIMP3_HAVE_FIXED_POINT`, and relies on everything
   minimp3.h has already defined above that point: mp3d_dsp_t and the Q-format
   typedefs, the coefficient tables, the MP3D_LEAF / MP3D_HOT / MP3D_KERNEL
   inline policy, the arithmetic helpers (mp3d_mulshift, MP3D_WRAP_ADD,
   mp3d_narrow_q30) and the MP3D_HAVE_INT_SIMD detection. Do not include it
   anywhere else; there is no include-order in which it compiles alone.

   To measure a change here:

       bash mp3measure

   which reports host Callgrind instruction counts against the last commit, an
   ESP32-C6 autoresearch run with the Helix ratio, the riscv32 .text delta and
   the PSNR tripwire. Quote the device number: on this decoder host and device
   have disagreed in both direction and magnitude. See
   agents/docs/mp3-decoder-performance.md. */

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
/* Rounded, saturating narrow of two int64x2 accumulators -- the vector form of
   mp3d_narrow_q30 generalised over the shift. Factored out of mp3d_v_mulshift
   so a kernel that builds its own accumulators can share it (#4109): the
   twiddle loop sums two products before narrowing once, and rounding each
   product separately would differ from the scalar path in the low bit. */
static int32x4_t mp3d_v_narrow(int64x2_t lo, int64x2_t hi,
                               int shift) FL_NO_EXCEPT
{
    const int64x2_t round = vdupq_n_s64((int64_t)1 << (shift - 1));
    const int64x2_t sh = vdupq_n_s64(-shift);
    lo = vshlq_s64(vaddq_s64(lo, round), sh);
    hi = vshlq_s64(vaddq_s64(hi, round), sh);
    return vmaxq_s32(vcombine_s32(vqmovn_s64(lo), vqmovn_s64(hi)),
                     vdupq_n_s32(MP3D_SAT_MIN));
}

static int32x4_t mp3d_v_mulshift(int32x4_t v, int32_t coef,
                                 int shift) FL_NO_EXCEPT
{
    const int32x2_t c = vdup_n_s32(coef);
    return mp3d_v_narrow(vmull_s32(vget_low_s32(v), c),
                         vmull_s32(vget_high_s32(v), c), shift);
}
#define MP3D_V_MULSHIFT(v, coef, bits) mp3d_v_mulshift((v), (coef), (bits))
/* Vector-by-vector 32x32->64. MP3D_V_MUL_LO/HI take a splat as their second
   operand; the twiddle kernel needs both factors to vary per lane. */
#define MP3D_V_MULV_LO(a, b)   vmull_s32(vget_low_s32(a), vget_low_s32(b))
#define MP3D_V_MULV_HI(a, b)   vmull_s32(vget_high_s32(a), vget_high_s32(b))
#define MP3D_V_REV4(v)                                                         \
    vcombine_s32(vrev64_s32(vget_high_s32(v)), vrev64_s32(vget_low_s32(v)))
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
MP3D_SIMD_TARGET static __m128i mp3d_v_narrow(__m128i a01, __m128i a23,
                                             int bits) FL_NO_EXCEPT
{
    const __m128i round = _mm_set1_epi64x((int64_t)1 << (bits - 1));
    __m128i p01 = _mm_add_epi64(a01, round);
    __m128i p23 = _mm_add_epi64(a23, round);
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
MP3D_SIMD_TARGET static __m128i mp3d_v_mulshift(__m128i v, int32_t coef,
                                                int bits) FL_NO_EXCEPT
{
    const __m128i c = _mm_set1_epi32(coef);
    const __m128i s = _mm_shuffle_epi32(v, _MM_SHUFFLE(3, 1, 2, 0));
    return mp3d_v_narrow(_mm_mul_epi32(s, c),
                         _mm_mul_epi32(_mm_srli_si128(s, 4), c), bits);
}
/* Vector-by-vector: both operands need the even-lane shuffle here, unlike
   MP3D_V_MUL_LO/HI whose second operand is a splat. */
#define MP3D_V_MULV_LO(a, b)                                                   \
    _mm_mul_epi32(MP3D_V_PREP(a), MP3D_V_PREP(b))
#define MP3D_V_MULV_HI(a, b)                                                   \
    _mm_mul_epi32(_mm_srli_si128(MP3D_V_PREP(a), 4),                           \
                  _mm_srli_si128(MP3D_V_PREP(b), 4))
#define MP3D_V_REV4(v)         _mm_shuffle_epi32((v), _MM_SHUFFLE(0, 1, 2, 3))
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
/* The closing twiddle-and-window loop of L3_imdct36 (#4109).

   Only this part of the IMDCT is addressable. The 9-point DCT-III above it has
   a butterfly that does not map onto four lanes, and vectorising across bands
   would need stride-18 gathers -- the IMDCT works *within* a band, the opposite
   of the DCT-32 below, where four consecutive bands are four consecutive int32
   and no gather is needed.

   Nine iterations is two vectors plus a scalar tail. Each output sums two
   int64 products and narrows once, exactly as the scalar path does. The
   reversed grbuf[17 - i] store is a lane reverse, the shape upstream's float
   kernel uses VREV for. */
MP3D_SIMD_TARGET static void mp3d_imdct36_twiddle_simd(
    int32_t *grbuf, int32_t *overlap, const int32_t *window,
    const int32_t *co, const int32_t *si) FL_NO_EXCEPT
{
    int i;
    for (i = 0; i <= 4; i += 4)
    {
        const mp3d_i32x4 cov = MP3D_V_LOAD4(&co[i]);
        const mp3d_i32x4 siv = MP3D_V_LOAD4(&si[i]);
        const mp3d_i32x4 tlo = MP3D_V_LOAD4(&g_twid9_q30[0 + i]);
        const mp3d_i32x4 thi = MP3D_V_LOAD4(&g_twid9_q30[9 + i]);
        const mp3d_i32x4 ovl = MP3D_V_LOAD4(&overlap[i]);
        const mp3d_i32x4 wlo = MP3D_V_LOAD4(&window[0 + i]);
        const mp3d_i32x4 whi = MP3D_V_LOAD4(&window[9 + i]);
        const mp3d_i32x4 sum = mp3d_v_narrow(
            MP3D_V_ADD64(MP3D_V_MULV_LO(cov, thi), MP3D_V_MULV_LO(siv, tlo)),
            MP3D_V_ADD64(MP3D_V_MULV_HI(cov, thi), MP3D_V_MULV_HI(siv, tlo)),
            30);
        const mp3d_i32x4 nov = mp3d_v_narrow(
            MP3D_V_SUB64(MP3D_V_MULV_LO(cov, tlo), MP3D_V_MULV_LO(siv, thi)),
            MP3D_V_SUB64(MP3D_V_MULV_HI(cov, tlo), MP3D_V_MULV_HI(siv, thi)),
            30);
        const mp3d_i32x4 head = mp3d_v_narrow(
            MP3D_V_SUB64(MP3D_V_MULV_LO(ovl, wlo), MP3D_V_MULV_LO(sum, whi)),
            MP3D_V_SUB64(MP3D_V_MULV_HI(ovl, wlo), MP3D_V_MULV_HI(sum, whi)),
            30);
        const mp3d_i32x4 tail = mp3d_v_narrow(
            MP3D_V_ADD64(MP3D_V_MULV_LO(ovl, whi), MP3D_V_MULV_LO(sum, wlo)),
            MP3D_V_ADD64(MP3D_V_MULV_HI(ovl, whi), MP3D_V_MULV_HI(sum, wlo)),
            30);

        /* overlap[i] was read into ovl above, before this overwrites it. */
        MP3D_V_STORE4(&overlap[i], nov);
        MP3D_V_STORE4(&grbuf[i], head);
        /* lanes 0..3 belong at 17-i down to 14-i: reversed, based at 14-i. */
        MP3D_V_STORE4(&grbuf[14 - i], MP3D_V_REV4(tail));
    }
    {
        const int32_t ovl = overlap[8];
        const int32_t sum = mp3d_narrow_q30(
            (int64_t)co[8]*g_twid9_q30[9 + 8] +
            (int64_t)si[8]*g_twid9_q30[0 + 8]);
        overlap[8] = mp3d_narrow_q30(
            (int64_t)co[8]*g_twid9_q30[0 + 8] -
            (int64_t)si[8]*g_twid9_q30[9 + 8]);
        grbuf[8] = mp3d_narrow_q30(
            (int64_t)ovl*window[0 + 8] - (int64_t)sum*window[9 + 8]);
        grbuf[9] = mp3d_narrow_q30(
            (int64_t)ovl*window[9 + 8] + (int64_t)sum*window[0 + 8]);
    }
}

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

/* Dispatches to the vector kernel when the host has the instructions and runs
   the scalar loop otherwise. Both compute each output as two int64 products
   narrowed once; rounding per product would differ between the two paths in
   the low bit, and #4055's gate is exact equality. */
/* -O3 for the same reason as mp3d_DCT_II above; the two were measured
   together. */
MP3D_KERNEL static void mp3d_imdct36_twiddle(int32_t *grbuf, int32_t *overlap,
                                 const int32_t *window, const int32_t *co,
                                 const int32_t *si) FL_NO_EXCEPT
{
    int i;
#if MP3D_SIMD_KERNELS_LIVE
    if (MP3D_SIMD_AVAILABLE())
    {
        mp3d_imdct36_twiddle_simd(grbuf, overlap, window, co, si);
        return;
    }
#endif
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

/* value * Coef, where Coef is a compile-time Q`Bits` coefficient -- the same
   arithmetic as mp3d_mulshift<Bits>(value, Coef), computed from the high half
   of the product instead of the low one.

   This is the one thing that made the DCT-32 the only stage where this decoder
   still lost to Helix on riscv32. Both do the same 80 multiplies per 32-point
   DCT; Helix spends two instructions on each (`mulh`, then a left shift to
   undo the coefficient's Q format) and this decoder spent nine:

       mul  mulh  lui  add  sltu  add  srli  slli  or

   The seven after `mul` are all there to round at bit `Bits` rather than at
   bit 32, which needs the low half of the product, the carry out of adding
   2^(Bits-1) to it, and a 64-bit funnel shift. Helix does not round at all --
   it truncates, which is where its 20 dB of accuracy went.

   The identity below rounds at bit 32 instead, which `mulh` does for free, and
   still lands on exactly the same bits. Write Coef = W*2^Bits + F with
   0 <= F < 2^Bits; then

       floor((v*Coef + 2^(Bits-1)) / 2^Bits)
         = v*W + floor((v*F + 2^(Bits-1)) / 2^Bits)
         = v*W + floor((v*(F << (32-Bits)) + 2^31) / 2^32)

   and the second term is `mulh(v, F32) + (lo(v*F32) >> 31)`, because adding
   2^31 to an unsigned low half carries exactly when its top bit is set. Both
   W and F32 are compile-time constants.

   F32 is kept *signed*. F << (32-Bits) is a Q32 fraction and exceeds int32
   whenever the coefficient's fractional part is at least a half, which is most
   of them; taking that bit pattern as a negative int32 subtracts 2^32 from it,
   so the missing 2^32/2^32 = 1 goes back on the integer side as W+1. That
   keeps the multiply a plain `mulh` -- gcc lowers a genuinely unsigned
   constant operand into srai/mul/mul/mulhu/add, five instructions, which
   throws the win away.

   Cost on riscv32 -Os, excluding the lui/addi that materialises the
   coefficient (both forms pay that, and both hoist it out of the k loop):

     | coefficient                  | before | after |
     |------------------------------|--------|-------|
     | tan(pi/16) Q31, W = 0        |    8   |   4   |
     | cos(pi/4)  Q31, W = 1        |    8   |   5   |
     | 1/(2cos(7pi/16)) Q29, W = 3  |    8   |   7   |
     | secants Q27, W = 0 or 1      |    8   |  4..5 |

   Bit-exactness is not an argument here, it is a proof: the identity was
   checked against mp3d_mulshift for all 2^32 int32 inputs against each of the
   33 coefficients this file instantiates, with zero mismatches. The device
   checksum and the PSNR are unchanged, and they are the tripwire if a
   transcribed constant is ever wrong.

   Not applied to the SIMD kernels: MP3D_V_MULSHIFT saturates on the narrow,
   which this form cannot do without giving back what it saved, and the vector
   targets have no scarcity of multiply throughput to fix. */
template <int Bits, int32_t Coef>
MP3D_LEAF int32_t mp3d_mulshift_k(int32_t value) FL_NO_EXCEPT
{
    /* Every DCT-32 coefficient is a positive secant or cosine. The split
       relies on that: `Coef >> Bits` is implementation-defined for a negative
       value, and W would have to round the other way. */
    static_assert(Coef > 0, "mp3d_mulshift_k: coefficient must be positive");
    const int32_t frac = (int32_t)((uint32_t)Coef << (32 - Bits));
    const int32_t whole = (int32_t)(Coef >> Bits) + (frac < 0 ? 1 : 0);
    const int64_t product = (int64_t)value * (int64_t)frac;
    const uint32_t rounded = (uint32_t)(uint64_t)(product >> 32) +
                             ((uint32_t)(uint64_t)product >> 31);
    return (int32_t)((uint32_t)value * (uint32_t)whole + rounded);
}

/* Forced to -O3 for the device, not for the host.

   Every ESP-IDF build in this project compiles at -Os
   (CONFIG_COMPILER_OPTIMIZATION_SIZE=y), and on these three kernels that is
   the wrong trade: they are straight-line butterfly code with no loop that
   -Os is protecting anyone from. Raising just them, on top of the polyphase
   rework below, is 41,998 -> 41,634 us on an ESP32-C6 -- 0.87%, about three
   times the flash-to-flash drift of the Helix reference measured alongside
   it -- for 1,310 bytes of .text (14,564 -> 15,874, +9.0%). Output is
   bit-identical: the ESP32-C6's combined FNV-1a over the decoded PCM is
   0xc6b632ab either way.

   ci/codec_cpu/callgrind.py cannot see this at all, because the host harness
   already builds at -O2. It is one of the few changes in this file that has
   to be decided on hardware.

   Deliberately not applied to mp3d_synth: there -O3 unrolls the four-lane
   chain to 3,007 instructions and 798 memory operations against the 998 and
   188 the hand-written pair form below produces, and .text goes to 20,748
   bytes. The unroll is the thing that helps, and doing it by hand is both
   smaller and faster than asking for it. */
MP3D_KERNEL static void mp3d_DCT_II(int32_t *grbuf, int n) FL_NO_EXCEPT
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

        /* Pass 1, spelled out rather than written as a loop over i.

           gcc unrolls it either way -- it was 443 riscv32 instructions inside
           the k loop before this, with no backward branch -- so the unroll is
           not what this buys. What it buys is `i` being a template argument,
           which makes g_sec_q27[3*i + k] a constant expression and lets each
           of the four multiplies use mp3d_mulshift_k. Written as a loop the
           secant is an int32 the compiler happens to know the value of, which
           is not the same thing: a template argument can pick the integer and
           fractional parts apart at compile time, and a value cannot.

           g_sec_q27 stays the single definition of these numbers -- the
           coefficients are read from it here, not transcribed -- which is why
           it is `constexpr`. The SIMD kernel still indexes it at run time. */
#define MP3D_DCT_PASS1(I)                                                      \
        do {                                                                   \
            const int32_t x0 = y[(I)*18];                                      \
            const int32_t x1 = y[(15 - (I))*18];                               \
            const int32_t x2 = y[(16 + (I))*18];                               \
            const int32_t x3 = y[(31 - (I))*18];                               \
            const int32_t t0 = mp3d_add_sat(x0, x3);                           \
            const int32_t t1 = mp3d_add_sat(x1, x2);                           \
            const int32_t t2 = mp3d_mulshift_k<27, g_sec_q27[3*(I) + 0]>(      \
                mp3d_sub_sat(x1, x2));                                         \
            const int32_t t3 = mp3d_mulshift_k<27, g_sec_q27[3*(I) + 1]>(      \
                mp3d_sub_sat(x0, x3));                                         \
            t[0][I] = mp3d_add_sat(t0, t1);                                    \
            t[1][I] = mp3d_mulshift_k<27, g_sec_q27[3*(I) + 2]>(               \
                mp3d_sub_sat(t0, t1));                                         \
            t[2][I] = mp3d_add_sat(t3, t2);                                    \
            t[3][I] = mp3d_mulshift_k<27, g_sec_q27[3*(I) + 2]>(               \
                mp3d_sub_sat(t3, t2));                                         \
        } while (0)
        MP3D_DCT_PASS1(0);
        MP3D_DCT_PASS1(1);
        MP3D_DCT_PASS1(2);
        MP3D_DCT_PASS1(3);
        MP3D_DCT_PASS1(4);
        MP3D_DCT_PASS1(5);
        MP3D_DCT_PASS1(6);
        MP3D_DCT_PASS1(7);
#undef MP3D_DCT_PASS1
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
            x[4] = mp3d_mulshift_k<31, MP3D_Q31_COS_PI_4>(mp3d_sub_sat(x0, x1));
            x5 = mp3d_add_sat(x5, x6);
            x6 = mp3d_mulshift_k<31, MP3D_Q31_COS_PI_4>(mp3d_add_sat(x6, x7));
            x7 = mp3d_add_sat(x7, xt);
            x3 = mp3d_mulshift_k<31, MP3D_Q31_COS_PI_4>(mp3d_add_sat(x3, x4));
            /* rotate by PI/8 */
            x5 = mp3d_sub_sat(x5, mp3d_mulshift_k<31, MP3D_Q31_TAN_PI_16>(x7));
            x7 = mp3d_add_sat(x7, mp3d_mulshift_k<31, MP3D_Q31_SIN_PI_8>(x5));
            x5 = mp3d_sub_sat(x5, mp3d_mulshift_k<31, MP3D_Q31_TAN_PI_16>(x7));
            x0 = mp3d_sub_sat(xt, x6); xt = mp3d_add_sat(xt, x6);
            x[1] = mp3d_mulshift_k<29, MP3D_Q29_SEC_PI_16>(mp3d_add_sat(xt, x7));
            x[2] = mp3d_mulshift_k<29, MP3D_Q29_SEC_PI_8>(mp3d_add_sat(x4, x3));
            x[3] = mp3d_mulshift_k<29, MP3D_Q29_SEC_3PI_16>(mp3d_sub_sat(x0, x5));
            x[5] = mp3d_mulshift_k<29, MP3D_Q29_SEC_5PI_16>(mp3d_add_sat(x0, x5));
            x[6] = mp3d_mulshift_k<29, MP3D_Q29_SEC_3PI_8>(mp3d_sub_sat(x4, x3));
            x[7] = mp3d_mulshift_k<29, MP3D_Q29_SEC_7PI_16>(mp3d_sub_sat(xt, x7));
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
   alone would put a one-LSB difference on roughly every sample.

   The clamp happens after the shift rather than before it, which is what keeps
   all but one operation here in 32 bits. This is an exact rewrite, not an
   approximation, and the equivalence is worth spelling out because the
   thresholds look like they moved:

     - The old upper test fired at sample >= 32766.5 LSB. At exactly that point
       t is 32767 LSB, so the shift yields s = 32767 and the new `s > 32767`
       clamp returns 32767 -- and above it, more. Below it t < 32767 LSB, so
       s <= 32766 and the clamp cannot fire. Same partition, same answers.
     - The old lower test fired at sample <= -32767.5 LSB, where -t is at least
       32767 LSB; truncation gives s <= -32767 and the `s -= (s < 0)` step puts
       it at -32768 or beyond, which the new clamp returns. Above it -t is under
       32767 LSB, so s >= -32767 after the step and the clamp cannot fire.

   The shift result always fits in int32 long before the clamp needs to
   consider it: the polyphase accumulator is bounded by 2^31 * 178833 < 2^48.5
   (the largest window row, summed in absolute value), so `t >> 26` is at most
   about 2^22.5.

   What this buys is two 64-bit comparisons against 64-bit constants, which a
   32-bit target pays for in a compare/branch pair per half. Measured out of
   line on riscv32 -Os the function drops from 42 instructions to 29, and the
   host callgrind total from 283.0M to 276.2M. It is a small win on its own --
   0.8% of Layer III on the C6, against the 50% the force-inlining above is
   worth -- but it is free. Nothing about the arithmetic
   changes -- the CPU audit checksum is byte-identical either way, and the
   ESP32-C6 reports the same combined FNV-1a over the decoded PCM. */
#define MP3D_PCM_HALF   ((int64_t)1 << (MINIMP3_FRAC_BITS - 1))

MP3D_LEAF mp3d_sample_t mp3d_scale_pcm(int64_t sample) FL_NO_EXCEPT
{
    const int64_t t = sample + MP3D_PCM_HALF;
    int32_t s = (int32_t)(t >= 0 ? (t >> MINIMP3_FRAC_BITS)
                                 : -((-t) >> MINIMP3_FRAC_BITS));
    s -= (s < 0);
    if (s > 32767) return (int16_t) 32767;
    if (s < -32768) return (int16_t)-32768;
    return (int16_t)s;
}

static void mp3d_synth_pair(mp3d_sample_t *pcm, int nch, const int32_t *z) FL_NO_EXCEPT
{
    int64_t a;
    /* The sums and differences are computed with defined wraparound, then
       widened (FastLED#4133).

       `(int64_t)(x - y)` evaluates `x - y` in int32 first and only then widens,
       which overflows once the polyphase inputs get large -- undefined
       behaviour, not merely a wrong number. Ordinary audio never gets there; a
       malformed intensity-stereo stream does, and UBSan caught it on
       l3-nonstandard-big-iscf.

       Widening both operands first was the obvious fix and it is the wrong one
       here: an int64 add or subtract costs two instructions plus carry on a
       32-bit target, and the codegen ledger measured it at +34% on the Xtensa
       polyphase inner loop -- 251 instructions against 187 -- which is the
       hottest kernel on the most constrained platform FastLED targets.

       Unsigned arithmetic wraps by definition in C, so MP3D_WRAP_SUB and
       MP3D_WRAP_ADD emit exactly the same single 32-bit instruction the
       undefined version did, and produce exactly the same bits. No well-formed
       stream reaches the wrap; a malformed one now gets a defined, reproducible
       answer instead of whatever the optimiser felt entitled to assume. */
    a  = (int64_t)MP3D_WRAP_SUB(z[14*64], z[    0]) * 29;
    a += (int64_t)MP3D_WRAP_ADD(z[ 1*64], z[13*64]) * 213;
    a += (int64_t)MP3D_WRAP_SUB(z[12*64], z[ 2*64]) * 459;
    a += (int64_t)MP3D_WRAP_ADD(z[ 3*64], z[11*64]) * 2037;
    a += (int64_t)MP3D_WRAP_SUB(z[10*64], z[ 4*64]) * 5153;
    a += (int64_t)MP3D_WRAP_ADD(z[ 5*64], z[ 9*64]) * 6574;
    a += (int64_t)MP3D_WRAP_SUB(z[ 8*64], z[ 6*64]) * 37489;
    a += (int64_t) z[ 7*64]                         * 75038;
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

/* Kept out of line deliberately, but for much less than the host says.

   gcc inlines this into mp3d_synth_granule's `for (i = 0; i < nbands; i += 2)`
   loop at both -O2 and -Os. On an x86-64 host that is a large pessimisation:
   the polyphase keeps its 64-bit accumulators live across an eight-tap chain,
   and folded into the granule's frame alongside the DCT-32 inlined just above
   it the allocator spills them. ci/codec_cpu/callgrind.py on the 892-frame
   corpus, scalar, -O2: 236,721,305 Ir inlined against 225,422,041 out of
   line, 4.8% of the whole decode for a keyword, and the ratio to the retired
   Helix backend goes 1.062x -> 1.011x.

   On the ESP32-C6 -- which is the one that counts -- the same keyword alone
   moved 46,505 -> 46,402 us. Each of those numbers is stable to 0.00% across
   repeats, but the *unchanged* Helix reference drifts about 0.9% between
   flashes, so 0.2% is indistinguishable from zero: on device this buys
   nothing measurable. Not a contradiction -- -Os allocates the inlined
   granule differently than -O2 does, and the host spill this removes was
   never being paid on the device.

   It is kept because it costs six bytes of .text (the granule shrinks by
   what mp3d_synth takes on) and because it makes the polyphase separately
   attributable in a profile, not because it is worth the 4.8% the host
   reports. Anyone reading the host number as a device prediction should read
   the next comment down instead: the restructuring below it is a 3.2% host
   *regression* and it is what actually took the device from 1.33x Helix to
   1.20x. */
MP3D_HOT void mp3d_synth(int32_t *xl, mp3d_sample_t *dstl, int nch, int32_t *lins) FL_NO_EXCEPT
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
    /* Lane stride through the four-lane accumulator.

       The four lanes are (left, right) x (subband i, subband i+1). In mono
       `xr == xl` and `dstr == dstl`, so lanes 1 and 3 recompute lanes 0 and 2
       and their stores are immediately overwritten by the lane 0/2 stores that
       follow them -- half the polyphase filterbank, thrown away. Helix avoids
       it by shipping a separate PolyphaseMono; here it is the same function
       with a two-lane tap chain, which is worth 34% of a mono decode on an
       ESP32-C6 (136,546 -> 89,699 us over 60 frames of 16 kHz MPEG-2 Layer III,
       bit-identical output).

       Nothing else reads the odd lanes in mono. The granule's mono carry loop
       (`for (i = 0; i < 15*64; i += 2)`) already saves only even indices, so
       the odd lanes of the history are stale in mono either way -- today they
       are read and discarded, which is exactly the work this removes.

       The lane step is a literal in each of two copies of the tap chain rather
       than a variable in one copy. One copy with `j += jstep` was tried first,
       on the reasoning that riscv32 -Os does not unroll the four-lane loop
       anyway (mp3d_synth_granule has exactly 32 `mulh` -- 8 taps x 4 products,
       not 8 x 4 x 4), so the step ought to have been free. Measured on the C6
       it was not: the stereo Layer III path went 46,294 -> 47,665 us, +3.0%,
       against a helix reference that moved 0.01% between the same two runs.
       Two chains with literal steps cost .text and nothing else. */
#if MP3D_HAVE_INT_SIMD
    const int use_simd = MP3D_SIMD_AVAILABLE();
#endif

    zlin[4*15]     = xl[18*16];
    zlin[4*15 + 2] = xl[0];

    zlin[4*31]     = xl[1 + 18*16];
    zlin[4*31 + 2] = xl[1];

    if (nch == 2)
    {
        zlin[4*15 + 1] = xr[18*16];
        zlin[4*15 + 3] = xr[0];
        zlin[4*31 + 1] = xr[1 + 18*16];
        zlin[4*31 + 3] = xr[1];

        mp3d_synth_pair(dstr, nch, lins + 4*15 + 1);
        mp3d_synth_pair(dstr + 32*nch, nch, lins + 4*15 + 64 + 1);
    }
    mp3d_synth_pair(dstl, nch, lins + 4*15);
    mp3d_synth_pair(dstl + 32*nch, nch, lins + 4*15 + 64);

    for (i = 14; i >= 0; i--)
    {
/* One tap of one lane pair. S0/S1/S2 are the original chain unchanged: S0
   opens the accumulators, S1 adds and S2 adds with the `a` operands swapped.
   The arithmetic, the operand order and the accumulation order are identical
   to the four-lane form these replaced, which is why the PCM checksum does
   not move.

   Two things changed, and only the second one is arithmetic-free by accident.

   The chain used to carry all four lanes -- eight int64 accumulators, held in
   `int64_t a[4], b[4]` -- across the whole eight-tap run. On riscv32 -Os that
   array is a stack slot, and the tap body paid eight memory operations per
   tap per lane to reload and rewrite it: of its 35 instructions, four loads
   and four stores were accumulator traffic and nothing else.

   Halving the live set is necessary but not sufficient. `int64_t a[2], b[2]`
   with the lane loop left rolled spills exactly as badly, because at -Os gcc
   does not unroll a two-iteration loop and a variable index into a local array
   has to be memory. The lanes are therefore written out as named scalars,
   which is what actually lets the accumulators live in registers.

   One lane at a time was tried too. It removes the spill just as completely
   but re-reads the window pair and recomputes the two zlin base addresses
   four times per tap instead of twice, which costs 3.2% on an x86-64 host
   that had registers to spare and was never paying the spill. The pair keeps
   both ends. */
#define LOAD(k) const int32_t w0 = w[2*(k)]; const int32_t w1 = w[2*(k) + 1]; const int32_t *vz = &zlin[4*i + g - (k)*64]; const int32_t *vy = &zlin[4*i + g - (15 - (k))*64];
#define S0(k, st) { LOAD(k);                                                   \
        b0  = (int64_t)vz[0]*w1 + (int64_t)vy[0]*w0;                           \
        a0  = (int64_t)vz[0]*w0 - (int64_t)vy[0]*w1;                           \
        if (st == 1) {                                                         \
        b1  = (int64_t)vz[1]*w1 + (int64_t)vy[1]*w0;                           \
        a1  = (int64_t)vz[1]*w0 - (int64_t)vy[1]*w1; } }
#define S1(k, st) { LOAD(k);                                                   \
        b0 += (int64_t)vz[0]*w1 + (int64_t)vy[0]*w0;                           \
        a0 += (int64_t)vz[0]*w0 - (int64_t)vy[0]*w1;                           \
        if (st == 1) {                                                         \
        b1 += (int64_t)vz[1]*w1 + (int64_t)vy[1]*w0;                           \
        a1 += (int64_t)vz[1]*w0 - (int64_t)vy[1]*w1; } }
#define S2(k, st) { LOAD(k);                                                   \
        b0 += (int64_t)vz[0]*w1 + (int64_t)vy[0]*w0;                           \
        a0 += (int64_t)vy[0]*w1 - (int64_t)vz[0]*w0;                           \
        if (st == 1) {                                                         \
        b1 += (int64_t)vz[1]*w1 + (int64_t)vy[1]*w0;                           \
        a1 += (int64_t)vy[1]*w1 - (int64_t)vz[1]*w0; } }
/* Lane pair g writes (a, b) to (15 - i, 17 + i) for g == 0 and to the same
   pair plus 32 samples for g == 2; within the pair, lane 0 is the left
   channel and lane 1 is dstr, which is dstl + (nch - 1). Both are address
   arithmetic on the pair index rather than four copies of the chain.

   In mono the odd lane recomputes the even one into the same address, so the
   step is 2 and lane 1 never runs at all -- a1 and b1 are dead, not merely
   redundant, and the zeroing below is what tells the compiler so rather than
   a value anything reads. */
#define MP3D_SYNTH_CHAIN(st)                                                   \
        {                                                                      \
            int g;                                                             \
            for (g = 0; g < 4; g += 2)                                         \
            {                                                                  \
                mp3d_sample_t *d = dstl + g*16*nch;                            \
                int64_t a0, b0, a1 = 0, b1 = 0;                                \
                S0(0, st) S2(1, st) S1(2, st) S2(3, st)                        \
                S1(4, st) S2(5, st) S1(6, st) S2(7, st)                        \
                if (st == 1)                                                   \
                {                                                              \
                    d[(15 - i)*nch + 1] = mp3d_scale_pcm(a1);                  \
                    d[(17 + i)*nch + 1] = mp3d_scale_pcm(b1);                  \
                }                                                              \
                d[(15 - i)*nch] = mp3d_scale_pcm(a0);                          \
                d[(17 + i)*nch] = mp3d_scale_pcm(b0);                          \
            }                                                                  \
        }

        zlin[4*i]     = xl[18*(31 - i)];
        zlin[4*i + 2] = xl[1 + 18*(31 - i)];
        zlin[4*(i + 16)]   = xl[1 + 18*(1 + i)];
        zlin[4*(i - 16) + 2] = xl[18*(1 + i)];
        if (nch == 2)
        {
            zlin[4*i + 1] = xr[18*(31 - i)];
            zlin[4*i + 3] = xr[1 + 18*(31 - i)];
            zlin[4*(i + 16) + 1] = xr[1 + 18*(1 + i)];
            zlin[4*(i - 16) + 3] = xr[18*(1 + i)];
        }

#if MP3D_HAVE_INT_SIMD
        if (use_simd)
        {
            /* The vector kernel still produces all four lanes at once, so it
               keeps the array form and the store block that goes with it. */
            int64_t a[4], b[4];
            mp3d_synth_taps(zlin, w, i, a, b);
            if (nch == 2)
            {
                dstr[(15 - i)*nch] = mp3d_scale_pcm(a[1]);
                dstr[(17 + i)*nch] = mp3d_scale_pcm(b[1]);
                dstr[(47 - i)*nch] = mp3d_scale_pcm(a[3]);
                dstr[(49 + i)*nch] = mp3d_scale_pcm(b[3]);
            }
            dstl[(15 - i)*nch] = mp3d_scale_pcm(a[0]);
            dstl[(17 + i)*nch] = mp3d_scale_pcm(b[0]);
            dstl[(47 - i)*nch] = mp3d_scale_pcm(a[2]);
            dstl[(49 + i)*nch] = mp3d_scale_pcm(b[2]);
        }
        else
#endif
        /* In mono the odd lanes recompute the even ones into the same
           addresses, so the step is 2 and lanes 1 and 3 never run. */
        if (nch == 1)
        {
            MP3D_SYNTH_CHAIN(2)
        }
        else
        {
            MP3D_SYNTH_CHAIN(1)
        }
        w += 16;
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
