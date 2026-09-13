/// @file AutoResearchMathExp.h
/// @brief On-device accuracy and speed of fl::exp against the toolchain libm (#4288).
///
/// fl::exp is a range-reduced polynomial with no libm dependency. This bench
/// answers two questions on the real core: how far it is from the
/// toolchain's expf/exp in ulp, and how many nanoseconds a call costs next
/// to libm's. Host x86 numbers are not representative (glibc's expf uses
/// FMA and tables); RISC-V and Cortex-M are what this is for.
///
/// libm is used only as the reference, from AutoResearchMathExp.cpp, so this
/// header pulls in no math library.
#pragma once

#include "fl/math/math.h"
#include "fl/stl/int.h"
#include "fl/system/sketch_macros.h"

namespace autoresearch {
namespace math_exp {

struct Result {
    bool success;
    fl::u32 iterations;       // calls per timed loop
    fl::u32 accuracy_points;  // inputs compared against libm
    float worst_ulp_float;    // max |fl::exp - expf| in ulp of expf
    double worst_ulp_double;  // max |fl::exp - exp| in ulp of exp
    float worst_x_float;
    double worst_x_double;
    fl::u32 fl_expf_us;       // fl::exp, iterations calls
    fl::u32 libm_expf_us;     // ::expf, same inputs
    fl::u32 fl_exp_us;        // fl::exp (double)
    fl::u32 libm_exp_us;      // ::exp
    float sink;               // keeps the loops honest
    int large_memory;         // FL_PLATFORM_HAS_LARGE_MEMORY as built
};

/// Runs the accuracy sweep and the timed loops. iterations = calls per loop.
Result run(fl::u32 iterations);

}  // namespace math_exp
}  // namespace autoresearch
