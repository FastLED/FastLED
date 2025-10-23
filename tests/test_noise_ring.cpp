#include "test.h"
#include "noise.h"
#include "crgb.h"
#include "fl/math.h"
#include <cmath>

using namespace fl;

namespace {
    // Helper function to calculate average color difference between two frames
    float calcAverageColorDifference(const CRGB* frame1, const CRGB* frame2, int num_leds) {
        float total_diff = 0.0f;
        for (int i = 0; i < num_leds; i++) {
            // Calculate delta for each channel
            int r_diff = (int)frame1[i].r - (int)frame2[i].r;
            int g_diff = (int)frame1[i].g - (int)frame2[i].g;
            int b_diff = (int)frame1[i].b - (int)frame2[i].b;

            // Use Euclidean distance for color difference
            float pixel_diff = std::sqrt(r_diff*r_diff + g_diff*g_diff + b_diff*b_diff);
            total_diff += pixel_diff;
        }
        return total_diff / num_leds;
    }
}

TEST_CASE("noiseRingHSV8 temporal smoothness - small time delta") {
    const int NUM_LEDS = 128;
    const float ANGLE_STEP = 2.0f * M_PI / NUM_LEDS;

    CRGB frame_t0[NUM_LEDS];
    CRGB frame_t1[NUM_LEDS];

    uint32_t time_base = 5000;
    float radius = 1.0f;

    // Generate frame at time t0
    for (int i = 0; i < NUM_LEDS; i++) {
        float angle = i * ANGLE_STEP;
        CHSV hsv = noiseRingHSV8(angle, time_base, radius);
        frame_t0[i] = hsv;
    }

    // Generate frame at t0 + 1ms (small time delta)
    // Noise should change very smoothly - small average color difference
    for (int i = 0; i < NUM_LEDS; i++) {
        float angle = i * ANGLE_STEP;
        CHSV hsv = noiseRingHSV8(angle, time_base + 1, radius);
        frame_t1[i] = hsv;
    }

    float avg_diff_1ms = calcAverageColorDifference(frame_t0, frame_t1, NUM_LEDS);

    FL_WARN("=== noiseRingHSV8 Temporal Smoothness Test (Δt=1ms) ===");
    FL_WARN("Average color pixel difference: " << avg_diff_1ms);
    FL_WARN("Threshold for smooth animation: < 5.0");

    // At 1ms, the noise should change only minimally
    // This tests that the noise function provides smooth animation
    // as time progresses in small increments
    CHECK_LT(avg_diff_1ms, 5.0f);
}

TEST_CASE("noiseRingHSV8 temporal evolution - large time delta") {
    const int NUM_LEDS = 128;
    const float ANGLE_STEP = 2.0f * M_PI / NUM_LEDS;

    CRGB frame_t0[NUM_LEDS];
    CRGB frame_t1[NUM_LEDS];

    uint32_t time_base = 1000;
    float radius = 1.0f;

    // Generate frame at time t0
    for (int i = 0; i < NUM_LEDS; i++) {
        float angle = i * ANGLE_STEP;
        CHSV hsv = noiseRingHSV8(angle, time_base, radius);
        frame_t0[i] = hsv;
    }

    // Generate frame at t0 + 1 second (large time delta)
    // Noise should change significantly - large average color difference
    for (int i = 0; i < NUM_LEDS; i++) {
        float angle = i * ANGLE_STEP;
        CHSV hsv = noiseRingHSV8(angle, time_base + 1000, radius);
        frame_t1[i] = hsv;
    }

    float avg_diff_1sec = calcAverageColorDifference(frame_t0, frame_t1, NUM_LEDS);

    FL_WARN("=== noiseRingHSV8 Temporal Evolution Test (Δt=1s) ===");
    FL_WARN("Average color pixel difference: " << avg_diff_1sec);
    FL_WARN("Threshold for significant evolution: > 30.0");

    // At 1 second, the noise should have evolved significantly
    // This tests that the pattern changes meaningfully over time
    CHECK_GT(avg_diff_1sec, 30.0f);
}

TEST_CASE("noiseRingHSV8 temporal response ratio") {
    const int NUM_LEDS = 128;
    const float ANGLE_STEP = 2.0f * M_PI / NUM_LEDS;

    CRGB frame_t0[NUM_LEDS];
    CRGB frame_t1_small[NUM_LEDS];
    CRGB frame_t1_large[NUM_LEDS];

    uint32_t time_base = 10000;
    float radius = 1.5f;

    // Generate frame at time t0
    for (int i = 0; i < NUM_LEDS; i++) {
        float angle = i * ANGLE_STEP;
        CHSV hsv = noiseRingHSV8(angle, time_base, radius);
        frame_t0[i] = hsv;
    }

    // Generate frame at t0 + 1ms
    for (int i = 0; i < NUM_LEDS; i++) {
        float angle = i * ANGLE_STEP;
        CHSV hsv = noiseRingHSV8(angle, time_base + 1, radius);
        frame_t1_small[i] = hsv;
    }

    // Generate frame at t0 + 1 second
    for (int i = 0; i < NUM_LEDS; i++) {
        float angle = i * ANGLE_STEP;
        CHSV hsv = noiseRingHSV8(angle, time_base + 1000, radius);
        frame_t1_large[i] = hsv;
    }

    float avg_diff_1ms = calcAverageColorDifference(frame_t0, frame_t1_small, NUM_LEDS);
    float avg_diff_1sec = calcAverageColorDifference(frame_t0, frame_t1_large, NUM_LEDS);
    float ratio = (avg_diff_1ms > 0.1f) ? (avg_diff_1sec / avg_diff_1ms) : avg_diff_1sec;

    FL_WARN("=== noiseRingHSV8 Temporal Response Ratio Test ===");
    FL_WARN("Δt=1ms: " << avg_diff_1ms);
    FL_WARN("Δt=1s: " << avg_diff_1sec);
    FL_WARN("Ratio (1s / 1ms): " << ratio);
    FL_WARN("Expected ratio: > 5.0x (1 second change >> 1 millisecond change)");

    // Small time changes should produce proportionally smaller differences
    // Large time changes should produce proportionally larger differences
    CHECK_GT(avg_diff_1sec, avg_diff_1ms * 5.0f);
}

TEST_CASE("noiseRingCRGB temporal test") {
    const int NUM_LEDS = 128;
    const float ANGLE_STEP = 2.0f * M_PI / NUM_LEDS;

    CRGB frame_t0[NUM_LEDS];
    CRGB frame_t1_small[NUM_LEDS];
    CRGB frame_t1_large[NUM_LEDS];

    uint32_t time_base = 20000;
    float radius = 2.0f;

    // Generate frame at time t0
    for (int i = 0; i < NUM_LEDS; i++) {
        float angle = i * ANGLE_STEP;
        frame_t0[i] = noiseRingCRGB(angle, time_base, radius);
    }

    // Generate at t0 + 1ms
    for (int i = 0; i < NUM_LEDS; i++) {
        float angle = i * ANGLE_STEP;
        frame_t1_small[i] = noiseRingCRGB(angle, time_base + 1, radius);
    }

    // Generate at t0 + 1 second
    for (int i = 0; i < NUM_LEDS; i++) {
        float angle = i * ANGLE_STEP;
        frame_t1_large[i] = noiseRingCRGB(angle, time_base + 1000, radius);
    }

    float avg_diff_1ms = calcAverageColorDifference(frame_t0, frame_t1_small, NUM_LEDS);
    float avg_diff_1sec = calcAverageColorDifference(frame_t0, frame_t1_large, NUM_LEDS);

    FL_WARN("=== noiseRingCRGB Temporal Test ===");
    FL_WARN("Δt=1ms average difference: " << avg_diff_1ms);
    FL_WARN("Δt=1s average difference: " << avg_diff_1sec);

    // Small delta should be small
    CHECK_LT(avg_diff_1ms, 5.0f);

    // Large delta should be large
    CHECK_GT(avg_diff_1sec, 25.0f);
}

TEST_CASE("noiseRingHSV16 full ring coverage") {
    const int NUM_LEDS = 256;
    const float ANGLE_STEP = 2.0f * M_PI / NUM_LEDS;

    uint32_t time_sample = 12345;
    float radius = 1.0f;

    fl::HSV16 min_hsv(0xFFFF, 0xFFFF, 0xFFFF);
    fl::HSV16 max_hsv(0, 0, 0);
    uint32_t h_sum = 0, s_sum = 0, v_sum = 0;

    // Sample the full ring
    for (int i = 0; i < NUM_LEDS; i++) {
        float angle = i * ANGLE_STEP;
        fl::HSV16 hsv = noiseRingHSV16(angle, time_sample, radius);

        // Track ranges
        min_hsv.h = FL_MIN(min_hsv.h, hsv.h);
        max_hsv.h = FL_MAX(max_hsv.h, hsv.h);

        min_hsv.s = FL_MIN(min_hsv.s, hsv.s);
        max_hsv.s = FL_MAX(max_hsv.s, hsv.s);

        min_hsv.v = FL_MIN(min_hsv.v, hsv.v);
        max_hsv.v = FL_MAX(max_hsv.v, hsv.v);

        // Track sums for average
        h_sum += hsv.h;
        s_sum += hsv.s;
        v_sum += hsv.v;
    }

    uint32_t h_avg = h_sum / NUM_LEDS;
    uint32_t s_avg = s_sum / NUM_LEDS;
    uint32_t v_avg = v_sum / NUM_LEDS;

    FL_WARN("=== noiseRingHSV16 Full Ring Coverage ===");
    FL_WARN("Hue - min: " << min_hsv.h << ", max: " << max_hsv.h << ", avg: " << h_avg << ", span: " << (max_hsv.h - min_hsv.h));
    FL_WARN("Sat - min: " << min_hsv.s << ", max: " << max_hsv.s << ", avg: " << s_avg << ", span: " << (max_hsv.s - min_hsv.s));
    FL_WARN("Val - min: " << min_hsv.v << ", max: " << max_hsv.v << ", avg: " << v_avg << ", span: " << (max_hsv.v - min_hsv.v));

    // All components should have decent range variation
    CHECK_GT(max_hsv.h - min_hsv.h, 5000);  // Good hue variation
    CHECK_GT(max_hsv.s - min_hsv.s, 5000);  // Good saturation variation
    CHECK_GT(max_hsv.v - min_hsv.v, 5000);  // Good value variation

    // Averages should be reasonably distributed (not clipped)
    CHECK_GT(h_avg, 0x2000);
    CHECK_LT(h_avg, 0xD000);
}

TEST_CASE("noiseRingHSV8 radius level of detail") {
    const int NUM_LEDS = 64;
    const float ANGLE_STEP = 2.0f * M_PI / NUM_LEDS;

    uint32_t time_sample = 54321;

    CRGB frame_radius_0p5[NUM_LEDS];
    CRGB frame_radius_2p0[NUM_LEDS];

    // Generate at radius = 0.5 (fine detail)
    for (int i = 0; i < NUM_LEDS; i++) {
        float angle = i * ANGLE_STEP;
        CHSV hsv = noiseRingHSV8(angle, time_sample, 0.5f);
        frame_radius_0p5[i] = hsv;
    }

    // Generate at radius = 2.0 (coarse detail)
    for (int i = 0; i < NUM_LEDS; i++) {
        float angle = i * ANGLE_STEP;
        CHSV hsv = noiseRingHSV8(angle, time_sample, 2.0f);
        frame_radius_2p0[i] = hsv;
    }

    float avg_diff = calcAverageColorDifference(frame_radius_0p5, frame_radius_2p0, NUM_LEDS);

    FL_WARN("=== noiseRingHSV8 Radius Level of Detail Test ===");
    FL_WARN("Average color difference (radius 0.5 vs 2.0): " << avg_diff);
    FL_WARN("Different radius values should sample different detail levels");

    // Different radius values should produce noticeably different patterns
    CHECK_GT(avg_diff, 10.0f);
}
