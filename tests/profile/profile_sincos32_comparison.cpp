// ok standalone
// Comparative profiling: sincos32 (scalar) vs sincos32_simd (SIMD)
// Build with profile mode and measure performance difference
//
// Usage:
//   ./profile_sincos32_comparison             # Profile both variants
//   ./profile_sincos32_comparison scalar      # Profile scalar only
//   ./profile_sincos32_comparison simd        # Profile SIMD only
//   ./profile_sincos32_comparison baseline    # JSON output (SIMD variant for framework)

#include "FastLED.h"
#include "fl/align.h"
#include "fl/int.h"
#include "fl/json.h"
#include "fl/simd.h"
#include "fl/sin32.h"
#include "fl/stl/cstdint.h"
#include "fl/stl/cstring.h"
#include "fl/stl/stdio.h"
#include "fl/stl/string.h"
#include "platforms/shared/simd_noop.hpp"

using namespace fl;

static const int WARMUP_CALLS = 1000;
static const int PROFILE_CALLS = 100000;

// Global volatile to prevent constant propagation across calls
volatile u32 g_angle_offset = 0;

// Volatile accumulator to force computation (prevents dead code elimination)
volatile i32 g_accumulator = 0;

// Function pointer types
typedef SinCos32 (*sincos_scalar_func_t)(u32);
typedef SinCos32_simd (*sincos_simd_func_t)(simd::simd_u32x4);

// Volatile function pointers prevent inlining
volatile sincos_scalar_func_t g_sincos_scalar_func = &sincos32;
volatile sincos_simd_func_t g_sincos_simd_func = &sincos32_simd;

// ========================
// Scalar sincos32 benchmark
// ========================
__attribute__((noinline))
void benchmark_sincos32_scalar(int calls) {
    // Pre-generate test angles (16 different angles to prevent pattern optimization)
    u32 test_angles[16];
    for (int i = 0; i < 16; i++) {
        test_angles[i] = i * 1048576;  // 0, 22.5, 45, ... degrees
    }

    i32 local_accumulator = 0;

    for (int i = 0; i < calls; i++) {
        // Vary angles using volatile offset to defeat constant propagation
        int angle_idx = (i + g_angle_offset) & 15; // Cycle through 16 angles

        // Force load from memory (prevent compiler from caching angles)
        asm volatile("" : : : "memory");

        u32 angle = test_angles[angle_idx];

        // Force call through volatile function pointer to prevent inlining
        SinCos32 result;
        asm volatile("" : "=m"(angle) : "m"(angle));
        result = g_sincos_scalar_func(angle);  // Cannot be inlined
        asm volatile("" : "=m"(result) : "m"(result));

        // Accumulate results to prevent dead code elimination
        // Use XOR to prevent overflow and keep operation cheap
        local_accumulator ^= result.sin_val;
        local_accumulator ^= result.cos_val;

        // Memory barrier to prevent hoisting/sinking
        asm volatile("" : "+r"(local_accumulator) : : "memory");
    }

    // Write to volatile global to ensure compiler doesn't optimize away the entire loop
    g_accumulator = local_accumulator;
}

// ========================
// SIMD sincos32_simd benchmark
// ========================
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
        result = g_sincos_simd_func(angles);  // Cannot be inlined
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
    bool do_scalar = true;
    bool do_simd = true;
    bool json_output = false;  // Profile framework JSON output mode

    if (argc > 1) {
        // Check for "baseline" argument (profile framework mode)
        if (fl::strcmp(argv[1], "baseline") == 0) {
            json_output = true;
            // Run SIMD variant for baseline measurements
            do_scalar = false;
            do_simd = true;
        } else {
            // Disable all by default if specific variant requested
            do_scalar = false;
            do_simd = false;

            if (fl::strcmp(argv[1], "scalar") == 0) {
                do_scalar = true;
            } else if (fl::strcmp(argv[1], "simd") == 0) {
                do_simd = true;
            } else {
                // Unknown argument - enable all
                do_scalar = true;
                do_simd = true;
            }
        }
    }

    u32 scalar_elapsed_us = 0;
    u32 simd_elapsed_us = 0;

    // ========================
    // Scalar sincos32 path
    // ========================
    if (do_scalar) {
        // Warmup
        benchmark_sincos32_scalar(WARMUP_CALLS);

        // Profile
        u32 t0 = ::micros();
        benchmark_sincos32_scalar(PROFILE_CALLS);
        u32 t1 = ::micros();

        scalar_elapsed_us = t1 - t0;

        if (!json_output) {
            printf("sincos32 (scalar):  %d calls in %lu us (%.1f ns/call, %.1f Mcalls/sec)\n",
                   PROFILE_CALLS, static_cast<unsigned long>(scalar_elapsed_us),
                   static_cast<double>(scalar_elapsed_us * 1000) / PROFILE_CALLS,
                   PROFILE_CALLS / static_cast<double>(scalar_elapsed_us));
        }
    }

    // ========================
    // SIMD sincos32_simd path
    // ========================
    if (do_simd) {
        // Warmup
        benchmark_sincos32_simd(WARMUP_CALLS);

        // Profile
        u32 t0 = ::micros();
        benchmark_sincos32_simd(PROFILE_CALLS);
        u32 t1 = ::micros();

        simd_elapsed_us = t1 - t0;

        if (json_output) {
            i64 elapsed_ns = static_cast<i64>(simd_elapsed_us) * 1000LL;
            double ns_per_call = static_cast<double>(elapsed_ns) / PROFILE_CALLS;
            double calls_per_sec = 1e9 / ns_per_call;

            Json result = Json::object();
            result.set("variant", "sincos32_simd");
            result.set("target", "sincos32_comparison");
            result.set("total_calls", PROFILE_CALLS);
            result.set("total_time_ns", elapsed_ns);
            result.set("ns_per_call", ns_per_call);
            result.set("calls_per_sec", calls_per_sec);

            printf("PROFILE_RESULT:%s\n", result.to_string().c_str());
        } else {
            printf("sincos32_simd:      %d calls in %lu us (%.1f ns/call, %.1f Mcalls/sec)\n",
                   PROFILE_CALLS, static_cast<unsigned long>(simd_elapsed_us),
                   static_cast<double>(simd_elapsed_us * 1000) / PROFILE_CALLS,
                   PROFILE_CALLS / static_cast<double>(simd_elapsed_us));
        }
    }

    // ========================
    // Performance comparison
    // ========================
    if (do_scalar && do_simd && !json_output) {
        double scalar_ns_per_angle = static_cast<double>(scalar_elapsed_us * 1000) / PROFILE_CALLS;
        double simd_ns_per_angle = static_cast<double>(simd_elapsed_us * 1000) / (PROFILE_CALLS * 4);
        double speedup = scalar_ns_per_angle / simd_ns_per_angle;

        printf("\n");
        printf("Performance Summary:\n");
        printf("  Scalar: %.1f ns/angle\n", scalar_ns_per_angle);
        printf("  SIMD:   %.1f ns/angle (processes 4 angles simultaneously)\n", simd_ns_per_angle);
        printf("  Speedup: %.2fx (SIMD is %.1f%% faster)\n", speedup, (speedup - 1.0) * 100.0);
        printf("\n");
        printf("Detailed breakdown:\n");
        printf("  Scalar: %.1f ns/call (1 angle per call)\n", scalar_ns_per_angle);
        printf("  SIMD:   %.1f ns/call (4 angles per call)\n", static_cast<double>(simd_elapsed_us * 1000) / PROFILE_CALLS);
    }

    return 0;
}
