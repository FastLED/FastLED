// Generate assembly dumps for sin32 analysis
#include "fl/sin32.h"
#include "fl/simd.h"

using namespace fl;

// Force noinline to see full function in assembly
__attribute__((noinline))
SinCos32 test_scalar(u32 angle) {
    return sincos32(angle);
}

// Force noinline to see full function in assembly
__attribute__((noinline))
SinCos32_simd test_simd(simd::simd_u32x4 angles) {
    return sincos32_simd(angles);
}

// Dummy main to force compilation
int main() {
    SinCos32 s = test_scalar(12345);

    u32 angles_arr[4] = {1000, 2000, 3000, 4000};
    simd::simd_u32x4 angles = simd::load_u32_4(angles_arr);
    SinCos32_simd result = test_simd(angles);

    return s.sin_val + result.sin_vals[0];
}
