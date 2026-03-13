// Performance comparison: five_bit_hd_gamma_bitshift baseline vs optimized
// Benchmarks the span-based CRGBA5 output variant which is the hot path
// for APA102/DOTSTAR LED strips.
// ok standalone

#include "FastLED.h"
#include "fl/gfx/five_bit_hd_gamma.h"
#include "fl/stl/int.h"
#include "fl/stl/cstring.h"
#include "fl/stl/stdio.h"
#include "profile_result.h"

using namespace fl;

// Benchmark configuration
static const int WARMUP_CALLS = 100;
static const int NUM_LEDS = 256; // Typical LED strip length
static const int PROFILE_ITERATIONS = 10000;

// Volatile to prevent dead-code elimination
volatile u8 g_sink = 0;

// Test data buffers
static CRGB g_input[NUM_LEDS];
static CRGBA5 g_output[NUM_LEDS];

static void init_test_data() {
    // Fill with realistic LED color data - mix of patterns
    for (int i = 0; i < NUM_LEDS; i++) {
        g_input[i] = CRGB(
            static_cast<u8>((i * 7 + 31) & 0xFF),
            static_cast<u8>((i * 13 + 97) & 0xFF),
            static_cast<u8>((i * 23 + 53) & 0xFF));
    }
}

__attribute__((noinline)) void benchmark_baseline(int iterations) {
    CRGB color_scale(255, 200, 180); // Typical color temperature correction
    u8 brightness = 128;            // Mid brightness

    fl::span<const CRGB> input(g_input, NUM_LEDS);
    fl::span<CRGBA5> output(g_output, NUM_LEDS);

    u8 local_sink = 0;

    for (int iter = 0; iter < iterations; iter++) {
        // Vary brightness slightly to prevent over-optimization
        u8 b = static_cast<u8>(brightness + (iter & 3));

        asm volatile("" : : : "memory");

        five_bit_hd_gamma_bitshift(input, color_scale, b, output);

        // Sink one value to prevent dead code elimination
        local_sink ^= output[iter & (NUM_LEDS - 1)].brightness_5bit;
        local_sink ^= output[iter & (NUM_LEDS - 1)].color.r;

        asm volatile("" : "+r"(local_sink) : : "memory");
    }

    g_sink = local_sink;
}

int main(int argc, char *argv[]) {
    bool json_output = (argc > 1 && fl::strcmp(argv[1], "baseline") == 0);

    init_test_data();

    // Warmup
    benchmark_baseline(WARMUP_CALLS);

    // Benchmark
    u32 t0 = ::micros();
    benchmark_baseline(PROFILE_ITERATIONS);
    u32 t1 = ::micros();
    u32 elapsed_us = t1 - t0;

    // Total pixel operations = iterations * NUM_LEDS
    int total_pixel_ops = PROFILE_ITERATIONS * NUM_LEDS;

    if (json_output) {
        ProfileResultBuilder::print_result("baseline", "five_bit_hd_gamma",
                                           total_pixel_ops, elapsed_us);
    } else {
        i64 elapsed_ns = static_cast<i64>(elapsed_us) * 1000LL;
        double ns_per_pixel =
            static_cast<double>(elapsed_ns) / total_pixel_ops;

        fl::printf("\n=== five_bit_hd_gamma Performance ===\n\n");
        fl::printf("Config: %d LEDs x %d iterations = %d pixel ops\n",
                   NUM_LEDS, PROFILE_ITERATIONS, total_pixel_ops);
        fl::printf("Time:        %lu us\n",
                   static_cast<unsigned long>(elapsed_us));
        fl::printf("Per pixel:   %.2f ns\n", ns_per_pixel);
        fl::printf("Throughput:  %.2f Mpixels/sec\n",
                   total_pixel_ops / static_cast<double>(elapsed_us));
        fl::printf("=====================================\n");
    }

    return 0;
}
