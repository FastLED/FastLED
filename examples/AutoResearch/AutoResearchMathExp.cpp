/// @file AutoResearchMathExp.cpp
/// @brief fl::exp vs libm on device (#4288). See AutoResearchMathExp.h.
#include "fl/system/sketch_macros.h"
#if !defined(FASTLED_AUTORESEARCH_LOW_MEMORY) && !FL_PLATFORM_HAS_LARGE_MEMORY
#define FASTLED_AUTORESEARCH_LOW_MEMORY 1
#endif
#if !(defined(FASTLED_AUTORESEARCH_LOW_MEMORY) && FASTLED_AUTORESEARCH_LOW_MEMORY)

#include "AutoResearchMathExp.h"
#include "fl/math/math.h"
#include "fl/stl/bit_cast.h"
#include "fl/stl/int.h"
#include <Arduino.h>  // micros()
#include <math.h>     // ok include - libm is the reference this bench measures against

namespace autoresearch {
namespace math_exp {
namespace {

float ulp_of(float v) {
    const fl::u32 bits = fl::bit_cast<fl::u32>(v < 0 ? -v : v);
    return fl::bit_cast<float>(bits + 1u) - fl::bit_cast<float>(bits);
}

double ulp_of(double v) {
    const fl::u64 bits = fl::bit_cast<fl::u64>(v < 0 ? -v : v);
    return fl::bit_cast<double>(bits + 1ull) - fl::bit_cast<double>(bits);
}

typedef float (*ExpFn)(float);

fl::u32 time_us(ExpFn fn, const float* in, fl::u32 iterations, float* sink) {
    float acc = 0.0f;
    const fl::u32 t0 = micros();
    for (fl::u32 i = 0; i < iterations; ++i) {
        acc += fn(in[i & 255u]);
    }
    const fl::u32 t1 = micros();
    *sink += acc;
    return t1 - t0;
}

float call_fl_expf(float x) { return fl::expf(x); }
float call_libm_expf(float x) { return ::expf(x); }
float call_fl_exp(float x) { return static_cast<float>(fl::exp(static_cast<double>(x))); }
float call_libm_exp(float x) { return static_cast<float>(::exp(static_cast<double>(x))); }

}  // namespace

Result run(fl::u32 iterations) {
    Result r = {};
    r.iterations = iterations;
    r.large_memory = FL_PLATFORM_HAS_LARGE_MEMORY ? 1 : 0;

    // Accuracy: the finite float range in 0.05 steps and the double range in
    // 0.5 steps, against libm.
    fl::u32 points = 0;
    for (float x = -103.9f; x < 88.7f; x += 0.05f) {
        const float want = ::expf(x);
        const float got = fl::expf(x);
        if (want <= 0.0f || want != want || want > 3.0e38f) {
            continue;
        }
        const float d = (got > want ? got - want : want - got) / ulp_of(want);
        if (d > r.worst_ulp_float) {
            r.worst_ulp_float = d;
            r.worst_x_float = x;
        }
        ++points;
    }
    for (double x = -745.0; x < 709.7; x += 0.5) {
        const double want = ::exp(x);
        const double got = fl::exp(x);
        if (want <= 0.0 || want != want) {
            continue;
        }
        const double d = (got > want ? got - want : want - got) / ulp_of(want);
        if (d > r.worst_ulp_double) {
            r.worst_ulp_double = d;
            r.worst_x_double = x;
        }
        ++points;
    }
    r.accuracy_points = points;

    // Speed: inputs over |x| <= 10, the constant-Q / Gaussian-window range.
    static float inputs[256];
    for (int i = 0; i < 256; ++i) {
        inputs[i] = -10.0f + 20.0f * static_cast<float>(i) / 255.0f;
    }
    float sink = 0.0f;
    r.fl_expf_us = time_us(&call_fl_expf, inputs, iterations, &sink);
    r.libm_expf_us = time_us(&call_libm_expf, inputs, iterations, &sink);
    r.fl_exp_us = time_us(&call_fl_exp, inputs, iterations, &sink);
    r.libm_exp_us = time_us(&call_libm_exp, inputs, iterations, &sink);
    r.sink = sink;

    r.success = r.worst_ulp_float <= 2.0f && r.worst_ulp_double <= 2.0 && points > 0;
    return r;
}

}  // namespace math_exp
}  // namespace autoresearch

#endif  // !FASTLED_AUTORESEARCH_LOW_MEMORY
