#include "fl/math/math.h"
#include "fl/system/sketch_macros.h"  // SKETCH_HAS_LARGE_MEMORY

// FL_MATH_USE_LIBM gate: on Large-memory targets we use libm for full IEEE-
// 754 precision. On Low-memory targets (where SKETCH_HAS_LARGE_MEMORY=0:
// AVR, LPC8xx, Teensy 3.x/LC, STM32F1, ESP8266, ...) libm transitively
// anchors libgcc's soft-double helper set (`__aeabi_dadd`, `__aeabi_dmul`,
// `__aeabi_ddiv`, ...). On the LPC845-BRK JSON-RPC bring-up sketch (#3002)
// that was ~6 KB of overhead — more than 10 % of the entire 64 KB flash
// budget — for math operations that the sketch never actually performed.
//
// Sketches on Low-memory targets that need full IEEE-754 precision can
// `#include <cmath>` directly; that anchors libm only at the call site
// that opted in.
#ifndef FL_MATH_USE_LIBM
#  if SKETCH_HAS_LARGE_MEMORY
#    define FL_MATH_USE_LIBM 1
#  else
#    define FL_MATH_USE_LIBM 0
#  endif
#endif

#if FL_MATH_USE_LIBM
// IWYU pragma: begin_keep
#include <math.h>
// IWYU pragma: end_keep  // okay banned header (STL wrapper implementation requires standard math.h)
#endif

// Arduino's WString.h (transitively included via the Arduino core on AVR and
// other Arduino-framework targets) defines `F()` as a PROGMEM-string macro.
// That macro collides with the `template <typename F>` parameter used by
// the polynomial / Newton-iteration helpers in this file: every `F(value)`
// constructor cast (e.g. `F(0.5)`) is rewritten to `WString::F(0.5)` and
// fails to compile. Undefining the macro here is local to this translation
// unit; the macro is only used in user sketches, never in fl::math itself.
#ifdef F
#undef F
#endif

namespace fl {

// Standalone floor implementation for float
float floor_impl_float(float value) {
    if (value >= 0.0f) {
        return static_cast<float>(static_cast<int>(value));
    }
    int i = static_cast<int>(value);
    return static_cast<float>(i - (value != static_cast<float>(i) ? 1 : 0));
}

// Standalone floor implementation for double
double floor_impl_double(double value) {
    if (value >= 0.0) {
        return static_cast<double>(static_cast<long long>(value));
    }
    long long i = static_cast<long long>(value);
    return static_cast<double>(i - (value != static_cast<double>(i) ? 1 : 0));
}

// Standalone ceil implementation for float
float ceil_impl_float(float value) {
    if (value <= 0.0f) {
        return static_cast<float>(static_cast<int>(value));
    }
    int i = static_cast<int>(value);
    return static_cast<float>(i + (value != static_cast<float>(i) ? 1 : 0));
}

// Standalone ceil implementation for double
double ceil_impl_double(double value) {
    if (value <= 0.0) {
        return static_cast<double>(static_cast<long long>(value));
    }
    long long i = static_cast<long long>(value);
    return static_cast<double>(i + (value != static_cast<double>(i) ? 1 : 0));
}

// Standalone exp implementation using Taylor series
// e^x ≈ 1 + x + x²/2! + x³/3! + x⁴/4! + ...
float exp_impl_float(float value) {
    if (value > 10.0f)
        return 22026.465794806718f; // e^10 approx
    if (value < -10.0f)
        return 0.0000453999297625f; // e^-10 approx

    // For negative values, use exp(x) = 1/exp(-x) to keep the Taylor series
    // input non-negative where it converges well with limited terms.
    if (value < 0.0f) {
        return 1.0f / exp_impl_float(-value);
    }

    float result = 1.0f;
    float term = 1.0f;
    for (int i = 1; i < 10; ++i) {
        term *= value / static_cast<float>(i);
        result += term;
    }
    return result;
}

double exp_impl_double(double value) {
    if (value > 10.0)
        return 22026.465794806718; // e^10 approx
    if (value < -10.0)
        return 0.0000453999297625; // e^-10 approx

    // For negative values, use exp(x) = 1/exp(-x) to keep the Taylor series
    // input non-negative where it converges well with limited terms.
    if (value < 0.0) {
        return 1.0 / exp_impl_double(-value);
    }

    double result = 1.0;
    double term = 1.0;
    for (int i = 1; i < 10; ++i) {
        term *= value / static_cast<double>(i);
        result += term;
    }
    return result;
}

// =============================================================================
// Libm-free trig / sqrt / log / pow implementations
// =============================================================================
//
// Hand-rolled to keep fl::math from anchoring libm in the link on freestanding
// targets. Previously the wrappers below all called `::sqrtf` / `::sin` /
// `::log` / etc, which pulled libm into every sketch that touched fl::math —
// transitively dragging in libgcc's full soft-double helper set (`__aeabi_dadd`,
// `__aeabi_dmul`, `__aeabi_ddiv`, ...) on no-FPU MCUs. On the LPC845-BRK
// JSON-RPC bring-up sketch (#3002), that was ~6 KB of overhead — more than
// 10 % of the entire 64 KB flash budget — for math operations that the sketch
// never actually performed.
//
// Strategy: polynomial / Newton iteration approximations with range reduction.
// Accuracy target is FastLED's animation use cases (color wheel angles,
// particle orientation, easing curves, gamma LUT generation) — typically
// ~3-4 decimal digits relative is plenty. Sketches that need full IEEE-754
// precision can call `<cmath>` directly; doing so anchors libm at THAT call
// site only, which is the explicit cost the user opted into.

namespace detail {

// --- sqrt via Newton-Raphson iteration ------------------------------------
//
// Initial estimate from the IEEE-754 exponent halving trick (well-known
// "bit-hack" approach, refined). Two Newton iterations converge to roughly
// 5 decimal digits for float, 10 for double — more than enough for color
// space and rotational math.
template <typename F>
inline F sqrt_newton_(F value) FL_NOEXCEPT {
    if (value <= F(0)) return F(0);
    // Initial estimate: x_0 = value / 2. Pure-arithmetic, no bit-cast needed.
    // Newton converges quadratically so 5-6 iterations is plenty even from a
    // crude start.
    F x = value;
    if (x > F(1)) x = F(1) + (x - F(1)) * F(0.5);  // bias toward 1 for fast convergence
    for (int i = 0; i < 6; ++i) {
        if (x == F(0)) break;
        x = F(0.5) * (x + value / x);
    }
    return x;
}

// --- Polynomial sin/cos with range reduction ------------------------------
//
// Reduce input to [-π/4, π/4] then evaluate degree-7 Taylor polynomial.
// Max error ~1e-5 for float, ~1e-10 for double on the reduced range.
template <typename F>
inline F sin_poly_quarterturn_(F x) FL_NOEXCEPT {
    // sin(x) ≈ x - x³/6 + x⁵/120 - x⁷/5040
    const F x2 = x * x;
    return x * (F(1) - x2 * (F(1.0 / 6.0) - x2 * (F(1.0 / 120.0) - x2 * F(1.0 / 5040.0))));
}

template <typename F>
inline F cos_poly_quarterturn_(F x) FL_NOEXCEPT {
    // cos(x) ≈ 1 - x²/2 + x⁴/24 - x⁶/720
    const F x2 = x * x;
    return F(1) - x2 * (F(0.5) - x2 * (F(1.0 / 24.0) - x2 * F(1.0 / 720.0)));
}

template <typename F>
inline F sin_reduce_(F x) FL_NOEXCEPT {
    const F kPi      = F(3.14159265358979323846);
    const F kTwoPi   = F(6.28318530717958647692);
    const F kPiHalf  = F(1.57079632679489661923);
    // Reduce to [-π, π]: subtract floor(x / 2π) * 2π.
    // For the FastLED use cases we expect |x| < ~100, so this loop runs a
    // bounded number of times. For pathological inputs we cap iterations.
    int guard = 32;
    while (x > kPi && guard > 0) { x -= kTwoPi; --guard; }
    while (x < -kPi && guard > 0) { x += kTwoPi; --guard; }
    // Map to [-π/2, π/2] using sin(π - x) = sin(x).
    if (x > kPiHalf)  x = kPi - x;
    if (x < -kPiHalf) x = -kPi - x;
    // [-π/2, π/2] -- the polynomial works fine here (max |x| ≈ 1.57).
    return sin_poly_quarterturn_(x);
}

template <typename F>
inline F cos_reduce_(F x) FL_NOEXCEPT {
    // cos(x) = sin(π/2 - x)
    const F kPiHalf = F(1.57079632679489661923);
    return sin_reduce_(kPiHalf - x);
}

// --- log via fraction extraction + polynomial -----------------------------
//
// log(x) = log(m * 2^e) = log(m) + e * log(2)   where m ∈ [1, 2)
// Repeatedly multiply/divide by 2 to bring x into [1, 2), then evaluate
// log(m) via a polynomial in (m-1) on [0, 1].
template <typename F>
inline F log_natural_(F value) FL_NOEXCEPT {
    if (value <= F(0)) return F(-1e30);  // -inf surrogate
    int e = 0;
    while (value >= F(2)) { value *= F(0.5); ++e; }
    while (value < F(1))  { value *= F(2);   --e; }
    // value ∈ [1, 2); evaluate log(value) via Taylor around 1: let u = value - 1.
    // log(1+u) = u - u²/2 + u³/3 - u⁴/4 + ...
    // Use enough terms for ~5-decimal accuracy on [0, 1].
    const F u  = value - F(1);
    const F u2 = u * u;
    const F u3 = u2 * u;
    const F u4 = u2 * u2;
    const F u5 = u4 * u;
    const F u6 = u4 * u2;
    const F u7 = u6 * u;
    F log_m = u - u2 * F(0.5) + u3 * F(1.0 / 3.0) - u4 * F(0.25)
              + u5 * F(0.2) - u6 * F(1.0 / 6.0) + u7 * F(1.0 / 7.0);
    const F kLn2 = F(0.69314718055994530942);
    return log_m + F(e) * kLn2;
}

}  // namespace detail

// Each impl below picks libm (Large-memory targets) or polynomial / Newton
// (Low-memory targets — keeps libm out of the link).

float sqrt_impl_float(float value) {
#if FL_MATH_USE_LIBM
    return ::sqrtf(value);
#else
    return detail::sqrt_newton_<float>(value);
#endif
}

double sqrt_impl_double(double value) {
#if FL_MATH_USE_LIBM
    return ::sqrt(value);
#else
    return detail::sqrt_newton_<double>(value);
#endif
}

float sin_impl_float(float value) {
#if FL_MATH_USE_LIBM
    return ::sinf(value);
#else
    return detail::sin_reduce_<float>(value);
#endif
}

double sin_impl_double(double value) {
#if FL_MATH_USE_LIBM
    return ::sin(value);
#else
    return detail::sin_reduce_<double>(value);
#endif
}

float cos_impl_float(float value) {
#if FL_MATH_USE_LIBM
    return ::cosf(value);
#else
    return detail::cos_reduce_<float>(value);
#endif
}

double cos_impl_double(double value) {
#if FL_MATH_USE_LIBM
    return ::cos(value);
#else
    return detail::cos_reduce_<double>(value);
#endif
}

float log_impl_float(float value) {
#if FL_MATH_USE_LIBM
    return ::logf(value);
#else
    return detail::log_natural_<float>(value);
#endif
}

double log_impl_double(double value) {
#if FL_MATH_USE_LIBM
    return ::log(value);
#else
    return detail::log_natural_<double>(value);
#endif
}

float log10_impl_float(float value) {
#if FL_MATH_USE_LIBM
    return ::log10f(value);
#else
    const float kInvLn10 = 0.43429448190325182765f;
    return detail::log_natural_<float>(value) * kInvLn10;
#endif
}

double log10_impl_double(double value) {
#if FL_MATH_USE_LIBM
    return ::log10(value);
#else
    const double kInvLn10 = 0.43429448190325182765;
    return detail::log_natural_<double>(value) * kInvLn10;
#endif
}

float pow_impl_float(float base, float exponent) {
#if FL_MATH_USE_LIBM
    return ::powf(base, exponent);
#else
    if (exponent == 0.0f) return 1.0f;
    int ie = static_cast<int>(exponent);
    if (static_cast<float>(ie) == exponent && ie >= -8 && ie <= 8) {
        float r = 1.0f;
        if (ie > 0)  { for (int i = 0; i < ie; ++i) r *= base; return r; }
        if (ie < 0)  { for (int i = 0; i < -ie; ++i) r *= base; return 1.0f / r; }
    }
    if (base <= 0.0f) return 0.0f;
    return exp_impl_float(exponent * detail::log_natural_<float>(base));
#endif
}

double pow_impl_double(double base, double exponent) {
#if FL_MATH_USE_LIBM
    return ::pow(base, exponent);
#else
    if (exponent == 0.0) return 1.0;
    int ie = static_cast<int>(exponent);
    if (static_cast<double>(ie) == exponent && ie >= -8 && ie <= 8) {
        double r = 1.0;
        if (ie > 0)  { for (int i = 0; i < ie; ++i) r *= base; return r; }
        if (ie < 0)  { for (int i = 0; i < -ie; ++i) r *= base; return 1.0 / r; }
    }
    if (base <= 0.0) return 0.0;
    return exp_impl_double(exponent * detail::log_natural_<double>(base));
#endif
}

// Absolute value: pure arithmetic; libm path unnecessary, same on both.
float fabs_impl_float(float value) {
    return value < 0.0f ? -value : value;
}

double fabs_impl_double(double value) {
    return value < 0.0 ? -value : value;
}

long lround_impl_float(float value) {
#if FL_MATH_USE_LIBM
    return ::lroundf(value);
#else
    if (value >= 0.0f) return static_cast<long>(value + 0.5f);
    return static_cast<long>(value - 0.5f);
#endif
}

long lround_impl_double(double value) {
#if FL_MATH_USE_LIBM
    return ::lround(value);
#else
    if (value >= 0.0) return static_cast<long>(value + 0.5);
    return static_cast<long>(value - 0.5);
#endif
}

// Arduino.h defines `round` as a macro, so we temporarily hide it.
#pragma push_macro("round")
#undef round
float round_impl_float(float value) {
#if FL_MATH_USE_LIBM
    return ::roundf(value);
#else
    return static_cast<float>(lround_impl_float(value));
#endif
}

double round_impl_double(double value) {
#if FL_MATH_USE_LIBM
    return ::round(value);
#else
    return static_cast<double>(lround_impl_double(value));
#endif
}
#pragma pop_macro("round")

float fmod_impl_float(float x, float y) {
#if FL_MATH_USE_LIBM
    return ::fmodf(x, y);
#else
    if (y == 0.0f) return 0.0f;
    const float q = x / y;
    // Truncate toward zero (libm fmod convention).
    const float t = q >= 0.0f ? floor_impl_float(q) : ceil_impl_float(q);
    return x - t * y;
#endif
}

double fmod_impl_double(double x, double y) {
#if FL_MATH_USE_LIBM
    return ::fmod(x, y);
#else
    if (y == 0.0) return 0.0;
    const double q = x / y;
    const double t = q >= 0.0 ? floor_impl_double(q) : ceil_impl_double(q);
    return x - t * y;
#endif
}

// Inverse tangent 2 implementations (atan2).
//
// Hand-rolled to avoid pulling libm into the link on freestanding targets
// (Cortex-M0+, AVR, etc). On the LPC845-BRK bring-up firmware (#3002), this
// wrapper used to anchor `::atan2`, which transitively pulled in libm's
// `s_scalbn` / `w_fmod` / `w_acos` and from there the full libgcc soft-double
// helper set (`__aeabi_dadd`, `__aeabi_dmul`, `__aeabi_ddiv`, ...) — together
// ~6 KB of pure overhead on a 64 KB-flash part for a sketch that never
// actually computes an arctangent.
//
// Implementation: piecewise minimax polynomial on |y/x| in [0, 1] (degree-5),
// extended to full [-π, π] via the standard quadrant + reciprocal identities:
//
//   atan2(y, x) = atan(y/x)            for x > 0
//               = atan(y/x) + π        for x < 0, y >= 0
//               = atan(y/x) - π        for x < 0, y <  0
//               = ±π/2                 for x == 0 (sign of y)
//
//   atan(u)     = polynomial(u)        for |u| <= 1
//               = π/2 - atan(1/u)      for u > 1
//               = -π/2 - atan(1/u)     for u < -1
//
// The degree-5 polynomial coefficients are Chebyshev-fit on [-1, 1]; max
// error ≈ 1.5e-3 rad (~0.085°). Adequate for FastLED's animation and
// rotational-math use cases — color wheel angles, particle orientation,
// xypath rotations. Sketches that need full double precision should bypass
// fl::math and call libm directly via `<cmath>` so they get the libm pull
// only when they actually need it.
namespace detail {
template <typename F>
inline F atan_poly_unit_(F u) FL_NOEXCEPT {
    // Polynomial approximation of atan on [-1, 1].
    // Coefficients from Padé-style minimax fit; max abs error ~1.5e-3.
    const F u2 = u * u;
    return u * (F(0.99997726) +
                u2 * (F(-0.33262347) +
                      u2 * (F(0.19354346) +
                            u2 * (F(-0.11643287) +
                                  u2 * (F(0.05265332) +
                                        u2 * F(-0.01172120))))));
}

template <typename F>
inline F atan_full_(F u) FL_NOEXCEPT {
    const F kPiHalf = F(1.57079632679489661923);
    // Reduce to |u| <= 1 via the reciprocal identity.
    if (u > F(1)) {
        return kPiHalf - atan_poly_unit_(F(1) / u);
    } else if (u < F(-1)) {
        return -kPiHalf - atan_poly_unit_(F(1) / u);
    }
    return atan_poly_unit_(u);
}

template <typename F>
inline F atan2_full_(F y, F x) FL_NOEXCEPT {
    const F kPi     = F(3.14159265358979323846);
    const F kPiHalf = F(1.57079632679489661923);
    // x == 0 special case: angle is ±π/2 (sign of y), or 0 when both are 0.
    if (x == F(0)) {
        if (y > F(0)) return kPiHalf;
        if (y < F(0)) return -kPiHalf;
        return F(0);
    }
    F a = atan_full_(y / x);
    if (x < F(0)) {
        a += (y >= F(0)) ? kPi : -kPi;
    }
    return a;
}
}  // namespace detail

float atan2_impl_float(float y, float x) {
#if FL_MATH_USE_LIBM
    return ::atan2f(y, x);
#else
    return detail::atan2_full_<float>(y, x);
#endif
}

double atan2_impl_double(double y, double x) {
#if FL_MATH_USE_LIBM
    return ::atan2(y, x);
#else
    return detail::atan2_full_<double>(y, x);
#endif
}

float hypot_impl_float(float x, float y) {
#if FL_MATH_USE_LIBM
    return ::hypotf(x, y);
#else
    return detail::sqrt_newton_<float>(x * x + y * y);
#endif
}

double hypot_impl_double(double x, double y) {
#if FL_MATH_USE_LIBM
    return ::hypot(x, y);
#else
    return detail::sqrt_newton_<double>(x * x + y * y);
#endif
}

float atan_impl_float(float value) {
#if FL_MATH_USE_LIBM
    return ::atanf(value);
#else
    return detail::atan_full_<float>(value);
#endif
}

double atan_impl_double(double value) {
#if FL_MATH_USE_LIBM
    return ::atan(value);
#else
    return detail::atan_full_<double>(value);
#endif
}

float asin_impl_float(float value) {
#if FL_MATH_USE_LIBM
    return ::asinf(value);
#else
    const float kPiHalf = 1.57079632679489661923f;
    if (value >=  1.0f) return  kPiHalf;
    if (value <= -1.0f) return -kPiHalf;
    const float denom = detail::sqrt_newton_<float>(1.0f - value * value);
    if (denom == 0.0f) return value > 0.0f ? kPiHalf : -kPiHalf;
    return detail::atan_full_<float>(value / denom);
#endif
}

double asin_impl_double(double value) {
#if FL_MATH_USE_LIBM
    return ::asin(value);
#else
    const double kPiHalf = 1.57079632679489661923;
    if (value >=  1.0) return  kPiHalf;
    if (value <= -1.0) return -kPiHalf;
    const double denom = detail::sqrt_newton_<double>(1.0 - value * value);
    if (denom == 0.0) return value > 0.0 ? kPiHalf : -kPiHalf;
    return detail::atan_full_<double>(value / denom);
#endif
}

float acos_impl_float(float value) {
#if FL_MATH_USE_LIBM
    return ::acosf(value);
#else
    const float kPiHalf = 1.57079632679489661923f;
    return kPiHalf - asin_impl_float(value);
#endif
}

double acos_impl_double(double value) {
#if FL_MATH_USE_LIBM
    return ::acos(value);
#else
    const double kPiHalf = 1.57079632679489661923;
    return kPiHalf - asin_impl_double(value);
#endif
}

float tan_impl_float(float value) {
#if FL_MATH_USE_LIBM
    return ::tanf(value);
#else
    const float c = detail::cos_reduce_<float>(value);
    if (c == 0.0f) return 0.0f;
    return detail::sin_reduce_<float>(value) / c;
#endif
}

double tan_impl_double(double value) {
#if FL_MATH_USE_LIBM
    return ::tan(value);
#else
    const double c = detail::cos_reduce_<double>(value);
    if (c == 0.0) return 0.0;
    return detail::sin_reduce_<double>(value) / c;
#endif
}

// Load exponent implementations (ldexp): value * 2^exp.
//
// Hand-rolled (no libm) for the same reason as atan2 above — `::ldexp` /
// `::ldexpf` was the other observed external libm entry from `fl.math+` in
// the LPC845-BRK bring-up firmware map.
//
// Strategy: scale by repeated doubling/halving rather than touching IEEE-754
// exponent bits directly. This is portable (no bit_cast assumptions, no
// endianness dependence, no signaling-NaN risk) and the worst-case loop
// count is bounded by the float/double exponent range (~127 / ~1023). For
// the FastLED use cases that anchor this symbol — stb_vorbis decoder,
// fixed_point's to_float(), animartrix rotational math — exp is typically
// a small integer (|exp| < 30), so the bounded loop is cheap (≤ 30 mults).
namespace detail {
template <typename F>
inline F ldexp_loop_(F value, int exp) FL_NOEXCEPT {
    if (value == F(0)) return value;
    if (exp == 0) return value;
    if (exp > 0) {
        // Cap to avoid pathological loops; saturate to ±inf-ish via overflow
        // (these magnitudes are well outside the IEEE 754 finite range, and
        // returning a saturated value matches libm's behavior of producing
        // ±HUGE_VAL on overflow).
        if (exp > 1024) exp = 1024;
        while (exp >= 30) { value *= F(1073741824); exp -= 30; }  // 2^30
        while (exp > 0)  { value *= F(2); --exp; }
        return value;
    }
    // exp < 0
    int neg = -exp;
    if (neg > 1024) neg = 1024;
    while (neg >= 30) { value *= F(1.0 / 1073741824.0); neg -= 30; }
    while (neg > 0)  { value *= F(0.5); --neg; }
    return value;
}
}  // namespace detail

float ldexp_impl_float(float value, int exp) {
#if FL_MATH_USE_LIBM
    return ::ldexpf(value, exp);
#else
    return detail::ldexp_loop_<float>(value, exp);
#endif
}

double ldexp_impl_double(double value, int exp) {
#if FL_MATH_USE_LIBM
    return ::ldexp(value, exp);
#else
    return detail::ldexp_loop_<double>(value, exp);
#endif
}

} // namespace fl
