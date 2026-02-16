// Performance comparison: sincos32 (scalar) vs sincos32_simd (SIMD)
// ok standalone
#include "FastLED.h"
#include "fl/align.h"
#include "fl/int.h"
#include "fl/json.h"
#include "fl/simd.h"
#include "fl/sin32.h"
#include "fl/stl/cstring.h"
#include "fl/stl/stdio.h"
#include "profile_result.h"

using namespace fl;

// Benchmark configuration
static const int WARMUP_CALLS = 1000;
static const int PROFILE_CALLS = 100000; // Process same number of angles for fair comparison

// Global volatile to prevent constant propagation
volatile u32 g_angle_offset = 0;
volatile i32 g_accumulator = 0;

// Benchmark scalar sincos32 (1 angle per call)
__attribute__((noinline))
void benchmark_sincos32_scalar(int calls) {
    // Pre-generate test angles (16 different angles to prevent pattern optimization)
    u32 test_angles[16];
    for (int i = 0; i < 16; i++) {
        test_angles[i] = i * 1048576; // 0, 22.5, 45, ... degrees
    }

    i32 local_accumulator = 0;

    for (int i = 0; i < calls; i++) {
        // Vary angles using volatile offset
        int angle_idx = (i + g_angle_offset) & 15;

        asm volatile("" : : : "memory");
        u32 angle = test_angles[angle_idx];

        SinCos32 result = sincos32(angle);

        // Accumulate to prevent dead code elimination
        local_accumulator ^= result.sin_val;
        local_accumulator ^= result.cos_val;

        asm volatile("" : "+r"(local_accumulator) : : "memory");
    }

    g_accumulator = local_accumulator;
}

// Benchmark SIMD sincos32_simd (4 angles per call)
__attribute__((noinline))
void benchmark_sincos32_simd(int calls) {
    // Pre-generate test angle sets (16 different sets)
    FL_ALIGNAS(16) u32 angle_sets[16][4];
    for (int set = 0; set < 16; set++) {
        angle_sets[set][0] = set * 1048576;
        angle_sets[set][1] = set * 1048576 + 4194304;
        angle_sets[set][2] = set * 1048576 + 8388608;
        angle_sets[set][3] = set * 1048576 + 12582912;
    }

    i32 local_accumulator = 0;

    for (int i = 0; i < calls; i++) {
        int set_idx = (i + g_angle_offset) & 15;

        asm volatile("" : : : "memory");
        simd::simd_u32x4 angles = simd::load_u32_4(angle_sets[set_idx]);

        SinCos32_simd result = sincos32_simd(angles);

        FL_ALIGNAS(16) u32 sin_vals[4];
        FL_ALIGNAS(16) u32 cos_vals[4];
        simd::store_u32_4(sin_vals, result.sin_vals);
        simd::store_u32_4(cos_vals, result.cos_vals);

        local_accumulator ^= static_cast<i32>(sin_vals[0]);
        local_accumulator ^= static_cast<i32>(cos_vals[0]);

        asm volatile("" : "+r"(local_accumulator) : : "memory");
    }

    g_accumulator = local_accumulator;
}

int main(int argc, char *argv[]) {
    bool json_output = (argc > 1 && fl::strcmp(argv[1], "baseline") == 0);

    // Warmup both variants
    benchmark_sincos32_scalar(WARMUP_CALLS);
    benchmark_sincos32_simd(WARMUP_CALLS / 4); // 4 angles per call

    // Benchmark scalar version
    u32 t0_scalar = ::micros();
    benchmark_sincos32_scalar(PROFILE_CALLS);
    u32 t1_scalar = ::micros();
    u32 elapsed_scalar_us = t1_scalar - t0_scalar;

    // Benchmark SIMD version (process same total number of angles)
    u32 t0_simd = ::micros();
    benchmark_sincos32_simd(PROFILE_CALLS / 4); // 4 angles per SIMD call
    u32 t1_simd = ::micros();
    u32 elapsed_simd_us = t1_simd - t0_simd;

    if (json_output) {
        // For comparison tests, we output multiple results
        // The runner will collect both
        ProfileResultBuilder::print_result("scalar", "sincos32_compare",
                                          PROFILE_CALLS, elapsed_scalar_us);
        ProfileResultBuilder::print_result("simd", "sincos32_compare",
                                          PROFILE_CALLS, elapsed_simd_us);
    } else {
        // Calculate statistics for human-readable output
        i64 elapsed_scalar_ns = static_cast<i64>(elapsed_scalar_us) * 1000LL;
        i64 elapsed_simd_ns = static_cast<i64>(elapsed_simd_us) * 1000LL;
        double ns_per_call_scalar = static_cast<double>(elapsed_scalar_ns) / PROFILE_CALLS;
        double ns_per_call_simd = static_cast<double>(elapsed_simd_ns) / PROFILE_CALLS;
        double speedup = ns_per_call_scalar / ns_per_call_simd;
        printf("\n=== sincos32 Performance Comparison ===\n\n");
        printf("Scalar (sincos32):\n");
        printf("  Calls:       %d\n", PROFILE_CALLS);
        printf("  Time:        %lu us\n", static_cast<unsigned long>(elapsed_scalar_us));
        printf("  Per call:    %.2f ns\n", ns_per_call_scalar);
        printf("  Throughput:  %.2f Mcalls/sec\n\n", PROFILE_CALLS / static_cast<double>(elapsed_scalar_us));

        printf("SIMD (sincos32_simd):\n");
        printf("  Calls:       %d (processing %d angles total)\n", PROFILE_CALLS / 4, PROFILE_CALLS);
        printf("  Time:        %lu us\n", static_cast<unsigned long>(elapsed_simd_us));
        printf("  Per angle:   %.2f ns\n", ns_per_call_simd);
        printf("  Throughput:  %.2f Mcalls/sec\n\n", PROFILE_CALLS / static_cast<double>(elapsed_simd_us));

        printf("Speedup:       %.2fx faster\n", speedup);
        printf("=======================================\n");
    }

    return 0;
}
