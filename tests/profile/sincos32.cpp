// Profiler for sincos32_simd
// Customized to benchmark SIMD 4-wide sincos

#include "FastLED.h"
#include "fl/sin32.h"
#include "fl/simd.h"
#include "fl/stl/cstring.h"
#include "fl/stl/stdio.h"

using namespace fl;

// Benchmark configuration
static const int WARMUP_ITERATIONS = 1000;
static const int PROFILE_ITERATIONS = 100000;

__attribute__((noinline))
void benchmarkBaseline() {
    // 4 different angles spread across quadrants
    volatile i32 sink = 0;
    for (int i = 0; i < PROFILE_ITERATIONS; i++) {
        u32 base = static_cast<u32>(i * 167);  // Vary angles each iteration
        FL_ALIGNAS(16) u32 angle_data[4] = {
            base,
            base + 4194304u,   // +pi/2
            base + 8388608u,   // +pi
            base + 12582912u   // +3pi/2
        };
        simd::simd_u32x4 angles = simd::load_u32_4(angle_data);
        SinCos32_simd result = sincos32_simd(angles);
        // Sink one value to prevent dead code elimination
        u32 tmp[4];
        simd::store_u32_4(tmp, result.sin_vals);
        sink = static_cast<i32>(tmp[0]);
    }
    (void)sink;
}

int main(int argc, char *argv[]) {
    bool do_baseline = true;

    if (argc > 1) {
        if (fl::strcmp(argv[1], "baseline") == 0) {
            do_baseline = true;
        } else if (fl::strcmp(argv[1], "optimized") == 0) {
            do_baseline = false;
            fl::printf("Error: Optimized variant not implemented yet\n");
            return 1;
        }
    }

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS / PROFILE_ITERATIONS; i++) {
        benchmarkBaseline();
    }

    // Benchmark baseline
    if (do_baseline) {
        fl::u32 t0 = ::micros();
        benchmarkBaseline();
        fl::u32 t1 = ::micros();
        fl::u32 elapsed_us = t1 - t0;
        fl::i64 elapsed_ns = static_cast<fl::i64>(elapsed_us) * 1000LL;
        double ns_per_call = static_cast<double>(elapsed_ns) / PROFILE_ITERATIONS;

        // Structured output for AI parsing
        fl::printf("PROFILE_RESULT:{\n");
        fl::printf("  \"variant\": \"baseline\",\n");
        fl::printf("  \"target\": \"sincos32\",\n");
        fl::printf("  \"total_calls\": %d,\n", PROFILE_ITERATIONS);
        fl::printf("  \"total_time_ns\": %lld,\n", static_cast<long long>(elapsed_ns));
        fl::printf("  \"ns_per_call\": %.2f,\n", ns_per_call);
        fl::printf("  \"calls_per_sec\": %.0f\n", 1e9 / ns_per_call);
        fl::printf("}\n");
    }

    return 0;
}
