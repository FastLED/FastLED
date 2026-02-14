// ok standalone
#include "FastLED.h"
#include "fl/align.h"
#include "fl/int.h"
#include "fl/simd.h"
#include "fl/sin32.h"
#include "fl/stl/cstdint.h"
#include "fl/stl/cstring.h"
#include "fl/stl/stdio.h"
#include "platforms/shared/simd_noop.hpp"
#include "tests/profile/profile_result.h"

using namespace fl;

static const int WARMUP_CALLS = 1000;
static const int PROFILE_CALLS = 100000;

// Global volatile to prevent constant propagation across calls
volatile u32 g_angle_offset = 0;

// Volatile accumulator to force computation (prevents dead code elimination)
volatile i32 g_accumulator = 0;

// Function pointer type for sincos32_simd
typedef SinCos32_simd (*sincos_func_t)(simd::simd_u32x4);

// Volatile function pointer prevents inlining
volatile sincos_func_t g_sincos_func = &sincos32_simd;

// Force noinline for this function
__attribute__((noinline))
void benchmark_sincos32_simd(int calls) {
    // Pre-generate test angles (16 different angle sets to prevent pattern optimization)
    FL_ALIGNAS(16) u32 angle_sets[16][4];
    for (int set = 0; set < 16; set++) {
        angle_sets[set][0] = set * 1048576;          // 0, 22.5, 45, ... degrees
        angle_sets[set][1] = set * 1048576 + 4194304; // +90 degrees
        angle_sets[set][2] = set * 1048576 + 8388608; // +180 degrees
        angle_sets[set][3] = set * 1048576 + 12582912; // +270 degrees
    }

    i32 local_accumulator = 0;

    for (int i = 0; i < calls; i++) {
        // Vary angles using volatile offset to defeat constant propagation
        int set_idx = (i + g_angle_offset) & 15; // Cycle through 16 sets

        // Force load from memory (prevent compiler from caching angles)
        asm volatile("" : : : "memory");

        simd::simd_u32x4 angles = simd::load_u32_4(angle_sets[set_idx]);

        // Force call through volatile function pointer to prevent inlining
        SinCos32_simd result;
        asm volatile("" : "=m"(angles) : "m"(angles));
        result = g_sincos_func(angles);  // Cannot be inlined
        asm volatile("" : "=m"(result) : "m"(result));

        // Extract scalar values to force computation
        FL_ALIGNAS(16) u32 sin_vals[4];
        FL_ALIGNAS(16) u32 cos_vals[4];
        simd::store_u32_4(sin_vals, result.sin_vals);
        simd::store_u32_4(cos_vals, result.cos_vals);

        // Accumulate results to prevent dead code elimination
        // Use XOR to prevent overflow and keep operation cheap
        local_accumulator ^= static_cast<i32>(sin_vals[0]);
        local_accumulator ^= static_cast<i32>(cos_vals[0]);

        // Memory barrier to prevent hoisting/sinking
        asm volatile("" : "+r"(local_accumulator) : : "memory");
    }

    // Write to volatile global to ensure compiler doesn't optimize away the entire loop
    g_accumulator = local_accumulator;
}

int main(int argc, char *argv[]) {
    bool json_output = (argc > 1 && fl::strcmp(argv[1], "baseline") == 0);

    // Warmup
    benchmark_sincos32_simd(WARMUP_CALLS);

    // Profile
    u32 t0 = ::micros();
    benchmark_sincos32_simd(PROFILE_CALLS);
    u32 t1 = ::micros();

    u32 elapsed_us = t1 - t0;

    if (json_output) {
        ProfileResultBuilder::print_result("sincos32_simd", "sincos32_simd",
                                          PROFILE_CALLS, elapsed_us);
    } else {
        printf("sincos32_simd: %d calls in %lu us (%.1f ns/call, %.1f Mcalls/sec)\n",
               PROFILE_CALLS, static_cast<unsigned long>(elapsed_us),
               static_cast<double>(elapsed_us * 1000) / PROFILE_CALLS,
               PROFILE_CALLS / static_cast<double>(elapsed_us));
    }

    return 0;
}
