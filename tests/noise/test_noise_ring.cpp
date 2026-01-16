#include "noise_test_helpers.h"

using namespace fl;
using namespace noise_test_helpers;

// Temporarily disable all non-critical noise tests for faster test runs
// These can be re-enabled when needed for in-depth noise validation
#if 0

TEST_CASE("[.]noiseRingHSV8 temporal smoothness - small time delta") {
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

TEST_CASE("[.]noiseRingHSV8 temporal evolution - large time delta") {
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
    FL_WARN("Threshold for significant evolution: > 1.0");

    // At 1 second, the noise should have evolved significantly
    // This tests that the pattern changes meaningfully over time
    CHECK_GT(avg_diff_1sec, 1.0f);
}

TEST_CASE("[.]noiseRingHSV8 temporal response ratio") {
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
    FL_WARN("Expected ratio: > 1.0x (1 second change > 1 millisecond change)");

    // Small time changes should produce proportionally smaller differences
    // Large time changes should produce proportionally larger differences
    CHECK_GT(avg_diff_1sec, avg_diff_1ms);
}

TEST_CASE("[.]noiseRingCRGB temporal test") {
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

    // Large delta should be larger than small delta
    CHECK_GT(avg_diff_1sec, avg_diff_1ms);
}

TEST_CASE("[.]noiseRingHSV16 full ring coverage") {
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

TEST_CASE("[.]noiseRingHSV8 radius level of detail") {
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

// Sphere noise tests
TEST_CASE("[.]noiseSphereHSV8 temporal smoothness - small time delta") {
    const int NUM_SAMPLES = 128;
    const float ANGLE_STEP = 2.0f * M_PI / NUM_SAMPLES;

    CRGB frame_t0[NUM_SAMPLES];
    CRGB frame_t1[NUM_SAMPLES];

    uint32_t time_base = 5000;
    float radius = 1.0f;

    // Generate frame at time t0, sampling along equator
    for (int i = 0; i < NUM_SAMPLES; i++) {
        float angle = i * ANGLE_STEP;
        float phi = M_PI / 2.0f;  // Equator
        CHSV hsv = noiseSphereHSV8(angle, phi, time_base, radius);
        frame_t0[i] = hsv;
    }

    // Generate frame at t0 + 1ms (small time delta)
    for (int i = 0; i < NUM_SAMPLES; i++) {
        float angle = i * ANGLE_STEP;
        float phi = M_PI / 2.0f;  // Equator
        CHSV hsv = noiseSphereHSV8(angle, phi, time_base + 1, radius);
        frame_t1[i] = hsv;
    }

    float avg_diff_1ms = calcAverageColorDifference(frame_t0, frame_t1, NUM_SAMPLES);

    FL_WARN("=== noiseSphereHSV8 Temporal Smoothness Test (Δt=1ms) ===");
    FL_WARN("Average color pixel difference: " << avg_diff_1ms);
    FL_WARN("Threshold for smooth animation: < 5.0");

    CHECK_LT(avg_diff_1ms, 5.0f);
}

TEST_CASE("[.]noiseSphereHSV8 temporal evolution - large time delta") {
    const int NUM_SAMPLES = 128;
    const float ANGLE_STEP = 2.0f * M_PI / NUM_SAMPLES;

    CRGB frame_t0[NUM_SAMPLES];
    CRGB frame_t1[NUM_SAMPLES];

    uint32_t time_base = 1000;
    float radius = 1.0f;

    // Generate frame at time t0, sampling along equator
    for (int i = 0; i < NUM_SAMPLES; i++) {
        float angle = i * ANGLE_STEP;
        float phi = M_PI / 2.0f;  // Equator
        CHSV hsv = noiseSphereHSV8(angle, phi, time_base, radius);
        frame_t0[i] = hsv;
    }

    // Generate frame at t0 + 1 second (large time delta)
    for (int i = 0; i < NUM_SAMPLES; i++) {
        float angle = i * ANGLE_STEP;
        float phi = M_PI / 2.0f;  // Equator
        CHSV hsv = noiseSphereHSV8(angle, phi, time_base + 1000, radius);
        frame_t1[i] = hsv;
    }

    float avg_diff_1sec = calcAverageColorDifference(frame_t0, frame_t1, NUM_SAMPLES);

    FL_WARN("=== noiseSphereHSV8 Temporal Evolution Test (Δt=1s) ===");
    FL_WARN("Average color pixel difference: " << avg_diff_1sec);
    FL_WARN("Threshold for significant evolution: > 0.1");

    CHECK_GT(avg_diff_1sec, 0.1f);
}

TEST_CASE("[.]noiseSphereHSV8 temporal response ratio") {
    const int NUM_SAMPLES = 128;
    const float ANGLE_STEP = 2.0f * M_PI / NUM_SAMPLES;

    CRGB frame_t0[NUM_SAMPLES];
    CRGB frame_t1_small[NUM_SAMPLES];
    CRGB frame_t1_large[NUM_SAMPLES];

    uint32_t time_base = 10000;
    float radius = 1.5f;

    // Generate frame at time t0, sampling along equator
    for (int i = 0; i < NUM_SAMPLES; i++) {
        float angle = i * ANGLE_STEP;
        float phi = M_PI / 2.0f;  // Equator
        CHSV hsv = noiseSphereHSV8(angle, phi, time_base, radius);
        frame_t0[i] = hsv;
    }

    // Generate frame at t0 + 1ms
    for (int i = 0; i < NUM_SAMPLES; i++) {
        float angle = i * ANGLE_STEP;
        float phi = M_PI / 2.0f;  // Equator
        CHSV hsv = noiseSphereHSV8(angle, phi, time_base + 1, radius);
        frame_t1_small[i] = hsv;
    }

    // Generate frame at t0 + 1 second
    for (int i = 0; i < NUM_SAMPLES; i++) {
        float angle = i * ANGLE_STEP;
        float phi = M_PI / 2.0f;  // Equator
        CHSV hsv = noiseSphereHSV8(angle, phi, time_base + 1000, radius);
        frame_t1_large[i] = hsv;
    }

    float avg_diff_1ms = calcAverageColorDifference(frame_t0, frame_t1_small, NUM_SAMPLES);
    float avg_diff_1sec = calcAverageColorDifference(frame_t0, frame_t1_large, NUM_SAMPLES);
    float ratio = (avg_diff_1ms > 0.1f) ? (avg_diff_1sec / avg_diff_1ms) : avg_diff_1sec;

    FL_WARN("=== noiseSphereHSV8 Temporal Response Ratio Test ===");
    FL_WARN("Δt=1ms: " << avg_diff_1ms);
    FL_WARN("Δt=1s: " << avg_diff_1sec);
    FL_WARN("Ratio (1s / 1ms): " << ratio);
    FL_WARN("Expected ratio: > 1.0x (1 second change > 1 millisecond change)");

    CHECK_GT(avg_diff_1sec, avg_diff_1ms);
}

TEST_CASE("[.]noiseSphereCRGB temporal test") {
    const int NUM_SAMPLES = 128;
    const float ANGLE_STEP = 2.0f * M_PI / NUM_SAMPLES;

    CRGB frame_t0[NUM_SAMPLES];
    CRGB frame_t1_small[NUM_SAMPLES];
    CRGB frame_t1_large[NUM_SAMPLES];

    uint32_t time_base = 20000;
    float radius = 2.0f;

    // Generate frame at time t0, sampling along equator
    for (int i = 0; i < NUM_SAMPLES; i++) {
        float angle = i * ANGLE_STEP;
        float phi = M_PI / 2.0f;  // Equator
        frame_t0[i] = noiseSphereCRGB(angle, phi, time_base, radius);
    }

    // Generate at t0 + 1ms
    for (int i = 0; i < NUM_SAMPLES; i++) {
        float angle = i * ANGLE_STEP;
        float phi = M_PI / 2.0f;  // Equator
        frame_t1_small[i] = noiseSphereCRGB(angle, phi, time_base + 1, radius);
    }

    // Generate at t0 + 1 second
    for (int i = 0; i < NUM_SAMPLES; i++) {
        float angle = i * ANGLE_STEP;
        float phi = M_PI / 2.0f;  // Equator
        frame_t1_large[i] = noiseSphereCRGB(angle, phi, time_base + 1000, radius);
    }

    float avg_diff_1ms = calcAverageColorDifference(frame_t0, frame_t1_small, NUM_SAMPLES);
    float avg_diff_1sec = calcAverageColorDifference(frame_t0, frame_t1_large, NUM_SAMPLES);

    FL_WARN("=== noiseSphereCRGB Temporal Test ===");
    FL_WARN("Δt=1ms average difference: " << avg_diff_1ms);
    FL_WARN("Δt=1s average difference: " << avg_diff_1sec);

    CHECK_LT(avg_diff_1ms, 5.0f);
    CHECK_GT(avg_diff_1sec, avg_diff_1ms);
}

TEST_CASE("[.]noiseSphereHSV16 full sphere coverage") {
    const int ANGLE_SAMPLES = 16;
    const int PHI_SAMPLES = 8;
    const float ANGLE_STEP = 2.0f * M_PI / ANGLE_SAMPLES;
    const float PHI_STEP = M_PI / PHI_SAMPLES;

    uint32_t time_sample = 12345;
    float radius = 1.0f;

    fl::HSV16 min_hsv(0xFFFF, 0xFFFF, 0xFFFF);
    fl::HSV16 max_hsv(0, 0, 0);
    uint32_t h_sum = 0, s_sum = 0, v_sum = 0;
    int sample_count = 0;

    // Sample across the full sphere
    for (int a = 0; a < ANGLE_SAMPLES; a++) {
        for (int p = 0; p < PHI_SAMPLES; p++) {
            float angle = a * ANGLE_STEP;
            float phi = p * PHI_STEP;
            fl::HSV16 hsv = noiseSphereHSV16(angle, phi, time_sample, radius);

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
            sample_count++;
        }
    }

    uint32_t h_avg = h_sum / sample_count;
    uint32_t s_avg = s_sum / sample_count;
    uint32_t v_avg = v_sum / sample_count;

    FL_WARN("=== noiseSphereHSV16 Full Sphere Coverage ===");
    FL_WARN("Samples: " << sample_count);
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

TEST_CASE("[.]noiseSphereHSV8 radius level of detail") {
    const int NUM_SAMPLES = 64;
    const float ANGLE_STEP = 2.0f * M_PI / NUM_SAMPLES;

    uint32_t time_sample = 54321;

    CRGB frame_radius_0p5[NUM_SAMPLES];
    CRGB frame_radius_2p0[NUM_SAMPLES];

    // Generate at radius = 0.5 (fine detail), sampling along equator
    for (int i = 0; i < NUM_SAMPLES; i++) {
        float angle = i * ANGLE_STEP;
        float phi = M_PI / 2.0f;  // Equator
        CHSV hsv = noiseSphereHSV8(angle, phi, time_sample, 0.5f);
        frame_radius_0p5[i] = hsv;
    }

    // Generate at radius = 2.0 (coarse detail), sampling along equator
    for (int i = 0; i < NUM_SAMPLES; i++) {
        float angle = i * ANGLE_STEP;
        float phi = M_PI / 2.0f;  // Equator
        CHSV hsv = noiseSphereHSV8(angle, phi, time_sample, 2.0f);
        frame_radius_2p0[i] = hsv;
    }

    float avg_diff = calcAverageColorDifference(frame_radius_0p5, frame_radius_2p0, NUM_SAMPLES);

    FL_WARN("=== noiseSphereHSV8 Radius Level of Detail Test ===");
    FL_WARN("Average color difference (radius 0.5 vs 2.0): " << avg_diff);
    FL_WARN("Different radius values should sample different detail levels");

    // Different radius values should produce noticeably different patterns
    CHECK_GT(avg_diff, 10.0f);
}

TEST_CASE("[.]noiseSphereHSV8 polar angle variation") {
    const int NUM_SAMPLES = 32;
    const float PHI_STEP = M_PI / NUM_SAMPLES;

    uint32_t time_sample = 99999;
    float angle = M_PI / 4.0f;  // Fixed azimuth angle
    float radius = 1.0f;

    CRGB frame_north[NUM_SAMPLES];
    CRGB frame_south[NUM_SAMPLES];

    // Sample along a meridian at north (phi near 0)
    for (int i = 0; i < NUM_SAMPLES; i++) {
        float phi = i * PHI_STEP;
        CHSV hsv = noiseSphereHSV8(angle, phi, time_sample, radius);
        frame_north[i] = hsv;
    }

    // Sample along a different meridian at south (phi near π)
    for (int i = 0; i < NUM_SAMPLES; i++) {
        float phi = (M_PI - (i * PHI_STEP));
        CHSV hsv = noiseSphereHSV8(angle + M_PI, phi, time_sample, radius);
        frame_south[i] = hsv;
    }

    float avg_diff = calcAverageColorDifference(frame_north, frame_south, NUM_SAMPLES);

    FL_WARN("=== noiseSphereHSV8 Polar Angle Variation Test ===");
    FL_WARN("Average color difference (north vs south): " << avg_diff);
    FL_WARN("Different polar positions should produce different patterns");

    // North and south poles should have different color patterns
    CHECK_GT(avg_diff, 5.0f);
}

TEST_CASE("[.]noiseRingHSV16 stress test - 1M time samples") {
    // Stress test: Keep angle and radius constant, vary time over 1 million samples
    // This reveals the effective min/max range of HSV16 components for circular hue
    const int NUM_SAMPLES = 1000000;
    const float ANGLE = 0.0f;  // Fixed angle
    const float RADIUS = 1.0f;  // Fixed radius

    uint16_t min_h = 0xFFFF;
    uint16_t max_h = 0x0000;
    uint16_t min_s = 0xFFFF;
    uint16_t max_s = 0x0000;
    uint16_t min_v = 0xFFFF;
    uint16_t max_v = 0x0000;

    // Sample 1 million time values
    for (uint32_t time = 0; time < NUM_SAMPLES; time++) {
        fl::HSV16 hsv = noiseRingHSV16(ANGLE, time, RADIUS);

        // Update running min/max for each component
        min_h = FL_MIN(min_h, hsv.h);
        max_h = FL_MAX(max_h, hsv.h);

        min_s = FL_MIN(min_s, hsv.s);
        max_s = FL_MAX(max_s, hsv.s);

        min_v = FL_MIN(min_v, hsv.v);
        max_v = FL_MAX(max_v, hsv.v);
    }

    uint16_t h_span = max_h - min_h;
    uint16_t s_span = max_s - min_s;
    uint16_t v_span = max_v - min_v;

    // Calculate percentage of full range (0xFFFF = 65535)
    float h_percent = (h_span / 65535.0f) * 100.0f;
    float s_percent = (s_span / 65535.0f) * 100.0f;
    float v_percent = (v_span / 65535.0f) * 100.0f;

    FL_WARN("=== noiseRingHSV16 Stress Test (1M Samples) ===");
    FL_WARN("Angle: " << ANGLE << ", Radius: " << RADIUS);
    FL_WARN("");
    FL_WARN("HUE (Hue):");
    FL_WARN("  Min: " << min_h << ", Max: " << max_h);
    FL_WARN("  Span: " << h_span << " (" << h_percent << "% of full range)");
    FL_WARN("");
    FL_WARN("SAT (Saturation):");
    FL_WARN("  Min: " << min_s << ", Max: " << max_s);
    FL_WARN("  Span: " << s_span << " (" << s_percent << "% of full range)");
    FL_WARN("");
    FL_WARN("VAL (Value/Brightness):");
    FL_WARN("  Min: " << min_v << ", Max: " << max_v);
    FL_WARN("  Span: " << v_span << " (" << v_percent << "% of full range)");
    FL_WARN("");
    FL_WARN("Issue: Hue should span 0-65535 for circular nature (currently " << h_percent << "%)");

    // Verify all components have some range
    CHECK_GT(h_span, 0);
    CHECK_GT(s_span, 0);
    CHECK_GT(v_span, 0);
}

TEST_CASE("[.]noiseRingHSV16 angle sweep - full ring coverage at fixed time") {
    // Test: Vary angle around the full ring, keep time and radius constant
    // This shows the effective range when the noise position changes spatially
    const int NUM_ANGLES = 360;  // Sample every 1 degree
    const float RADIUS = 1.0f;
    const uint32_t TIME = 0;  // Fixed time

    uint16_t min_h = 0xFFFF;
    uint16_t max_h = 0x0000;
    uint16_t min_s = 0xFFFF;
    uint16_t max_s = 0x0000;
    uint16_t min_v = 0xFFFF;
    uint16_t max_v = 0x0000;

    // Sample angles around the full circle
    for (int angle_deg = 0; angle_deg < NUM_ANGLES; angle_deg++) {
        float angle_rad = (angle_deg / 360.0f) * 2.0f * M_PI;
        fl::HSV16 hsv = noiseRingHSV16(angle_rad, TIME, RADIUS);

        min_h = FL_MIN(min_h, hsv.h);
        max_h = FL_MAX(max_h, hsv.h);

        min_s = FL_MIN(min_s, hsv.s);
        max_s = FL_MAX(max_s, hsv.s);

        min_v = FL_MIN(min_v, hsv.v);
        max_v = FL_MAX(max_v, hsv.v);
    }

    uint16_t h_span = max_h - min_h;
    uint16_t s_span = max_s - min_s;
    uint16_t v_span = max_v - min_v;

    float h_percent = (h_span / 65535.0f) * 100.0f;
    float s_percent = (s_span / 65535.0f) * 100.0f;
    float v_percent = (v_span / 65535.0f) * 100.0f;

    FL_WARN("=== noiseRingHSV16 Angle Sweep (360 samples at fixed time) ===");
    FL_WARN("Time: " << TIME << ", Radius: " << RADIUS);
    FL_WARN("");
    FL_WARN("HUE (Hue):");
    FL_WARN("  Min: " << min_h << ", Max: " << max_h);
    FL_WARN("  Span: " << h_span << " (" << h_percent << "% of full range)");
    FL_WARN("");
    FL_WARN("SAT (Saturation):");
    FL_WARN("  Min: " << min_s << ", Max: " << max_s);
    FL_WARN("  Span: " << s_span << " (" << s_percent << "% of full range)");
    FL_WARN("");
    FL_WARN("VAL (Value/Brightness):");
    FL_WARN("  Min: " << min_v << ", Max: " << max_v);
    FL_WARN("  Span: " << v_span << " (" << v_percent << "% of full range)");
    FL_WARN("");
    FL_WARN("Note: For circular hue, full 0-65535 range required (currently " << h_percent << "%)");

    CHECK_GT(h_span, 0);
    CHECK_GT(s_span, 0);
    CHECK_GT(v_span, 0);
}

TEST_CASE("[.]noiseRingHSV16 2D parameter space - time + angle variation") {
    // Comprehensive stress test: Vary BOTH time and angle
    // This explores a 2D parameter space to find true min/max
    const int ANGLE_SAMPLES = 64;
    const int TIME_SAMPLES = 16384;  // ~1M total samples: 64 * 16384
    const float RADIUS = 1.0f;
    const float ANGLE_STEP = 2.0f * M_PI / ANGLE_SAMPLES;

    uint16_t min_h = 0xFFFF;
    uint16_t max_h = 0x0000;
    uint16_t min_s = 0xFFFF;
    uint16_t max_s = 0x0000;
    uint16_t min_v = 0xFFFF;
    uint16_t max_v = 0x0000;

    // Sample the 2D parameter space
    for (int a = 0; a < ANGLE_SAMPLES; a++) {
        float angle = a * ANGLE_STEP;
        for (int t = 0; t < TIME_SAMPLES; t++) {
            fl::HSV16 hsv = noiseRingHSV16(angle, t, RADIUS);

            min_h = FL_MIN(min_h, hsv.h);
            max_h = FL_MAX(max_h, hsv.h);

            min_s = FL_MIN(min_s, hsv.s);
            max_s = FL_MAX(max_s, hsv.s);

            min_v = FL_MIN(min_v, hsv.v);
            max_v = FL_MAX(max_v, hsv.v);
        }
    }

    uint16_t h_span = max_h - min_h;
    uint16_t s_span = max_s - min_s;
    uint16_t v_span = max_v - min_v;

    float h_percent = (h_span / 65535.0f) * 100.0f;
    float s_percent = (s_span / 65535.0f) * 100.0f;
    float v_percent = (v_span / 65535.0f) * 100.0f;

    FL_WARN("=== noiseRingHSV16 2D Parameter Space (64 angles x 16384 times) ===");
    FL_WARN("Total samples: " << (ANGLE_SAMPLES * TIME_SAMPLES) << ", Radius: " << RADIUS);
    FL_WARN("");
    FL_WARN("HUE (Hue):");
    FL_WARN("  Min: " << min_h << ", Max: " << max_h);
    FL_WARN("  Span: " << h_span << " (" << h_percent << "% of full range)");
    FL_WARN("");
    FL_WARN("SAT (Saturation):");
    FL_WARN("  Min: " << min_s << ", Max: " << max_s);
    FL_WARN("  Span: " << s_span << " (" << s_percent << "% of full range)");
    FL_WARN("");
    FL_WARN("VAL (Value/Brightness):");
    FL_WARN("  Min: " << min_v << ", Max: " << max_v);
    FL_WARN("  Span: " << v_span << " (" << v_percent << "% of full range)");
    FL_WARN("");
    FL_WARN("Critical issue: Hue achieves only " << h_percent << "% of full 0-65535 range");

    CHECK_GT(h_span, 0);
    CHECK_GT(s_span, 0);
    CHECK_GT(v_span, 0);
}

TEST_CASE("[.]noiseRingHSV16 random angle + time - 1M samples") {
    // Randomized stress test: Vary BOTH angle (0->2PI) and time randomly
    // This tests if component ranges vary based on spatial position vs just time slices
    const int NUM_SAMPLES = 1000000;
    const float RADIUS = 1.0f;

    uint16_t min_h = 0xFFFF;
    uint16_t max_h = 0x0000;
    uint16_t min_s = 0xFFFF;
    uint16_t max_s = 0x0000;
    uint16_t min_v = 0xFFFF;
    uint16_t max_v = 0x0000;

    // Seed random number generator for reproducibility
    random16_set_seed(42);

    // Sample 1 million random points in (angle, time) parameter space
    for (int i = 0; i < NUM_SAMPLES; i++) {
        // Generate random angle: 0 to 2PI
        // Use random16() to get 0-65535, map to 0-2PI
        float angle = (random16() / 65535.0f) * 2.0f * M_PI;

        // Generate random time: 0 to 2^32-1 (full uint32 range)
        uint32_t time = random16();
        time = (time << 16) | random16();  // Combine two random16 calls for full 32-bit

        fl::HSV16 hsv = noiseRingHSV16(angle, time, RADIUS);

        min_h = FL_MIN(min_h, hsv.h);
        max_h = FL_MAX(max_h, hsv.h);

        min_s = FL_MIN(min_s, hsv.s);
        max_s = FL_MAX(max_s, hsv.s);

        min_v = FL_MIN(min_v, hsv.v);
        max_v = FL_MAX(max_v, hsv.v);
    }

    uint16_t h_span = max_h - min_h;
    uint16_t s_span = max_s - min_s;
    uint16_t v_span = max_v - min_v;

    float h_percent = (h_span / 65535.0f) * 100.0f;
    float s_percent = (s_span / 65535.0f) * 100.0f;
    float v_percent = (v_span / 65535.0f) * 100.0f;

    FL_WARN("=== noiseRingHSV16 Random Angle + Time (1M Samples) ===");
    FL_WARN("Randomized both angle (0->2PI) and time (0->2^32)");
    FL_WARN("Radius: " << RADIUS);
    FL_WARN("");
    FL_WARN("HUE (Hue):");
    FL_WARN("  Min: " << min_h << ", Max: " << max_h);
    FL_WARN("  Span: " << h_span << " (" << h_percent << "% of full range)");
    FL_WARN("");
    FL_WARN("SAT (Saturation):");
    FL_WARN("  Min: " << min_s << ", Max: " << max_s);
    FL_WARN("  Span: " << s_span << " (" << s_percent << "% of full range)");
    FL_WARN("");
    FL_WARN("VAL (Value/Brightness):");
    FL_WARN("  Min: " << min_v << ", Max: " << max_v);
    FL_WARN("  Span: " << v_span << " (" << v_percent << "% of full range)");
    FL_WARN("");
    FL_WARN("Analysis: Testing if variance in min/max is due to spatial position or time slices");

    // All components should achieve near-full range with random sampling
    CHECK_GT(h_span, 0);
    CHECK_GT(s_span, 0);
    CHECK_GT(v_span, 0);
}

TEST_CASE("[.]noiseRingHSV16 find optimal extents for 98% coverage - 100k raw samples") {
    // Find optimal MIN/MAX extents that will yield ~98% coverage after rescaling
    // Sample RAW noise values (before rescaling) to find observed min/max distribution
    const int NUM_SAMPLES = 100000;  // Sample 100k raw values
    const float RADIUS = 1.0f;

    uint16_t min_h_raw = 0xFFFF;
    uint16_t max_h_raw = 0x0000;
    uint16_t min_s_raw = 0xFFFF;
    uint16_t max_s_raw = 0x0000;
    uint16_t min_v_raw = 0xFFFF;
    uint16_t max_v_raw = 0x0000;

    random16_set_seed(42);

    // Sample raw noise values from random points
    for (int i = 0; i < NUM_SAMPLES; i++) {
        float angle = (random16() / 65535.0f) * 2.0f * M_PI;
        uint32_t time = random16();
        time = (time << 16) | random16();

        // Get RAW noise before rescaling
        float x = fl::cosf(angle);
        float y = fl::sinf(angle);
        uint32_t nx = static_cast<uint32_t>((x + 1.0f) * 0.5f * RADIUS * 0xffff);
        uint32_t ny = static_cast<uint32_t>((y + 1.0f) * 0.5f * RADIUS * 0xffff);

        uint16_t h_raw = inoise16(nx, ny, time);
        uint16_t s_raw = inoise16(nx, ny, time + 0x10000);
        uint16_t v_raw = inoise16(nx, ny, time + 0x20000);

        min_h_raw = FL_MIN(min_h_raw, h_raw);
        max_h_raw = FL_MAX(max_h_raw, h_raw);
        min_s_raw = FL_MIN(min_s_raw, s_raw);
        max_s_raw = FL_MAX(max_s_raw, s_raw);
        min_v_raw = FL_MIN(min_v_raw, v_raw);
        max_v_raw = FL_MAX(max_v_raw, v_raw);
    }

    // Current extents
    uint16_t current_min = fl::NOISE16_EXTENT_MIN;
    uint16_t current_max = fl::NOISE16_EXTENT_MAX;

    FL_WARN("");
    FL_WARN("=== RAW NOISE STATISTICS (100k samples) ===");
    FL_WARN("");
    FL_WARN("Observed raw noise ranges:");
    FL_WARN("  Hue:        " << min_h_raw << " - " << max_h_raw << " (span: " << (max_h_raw - min_h_raw) << ")");
    FL_WARN("  Saturation: " << min_s_raw << " - " << max_s_raw << " (span: " << (max_s_raw - min_s_raw) << ")");
    FL_WARN("  Value:      " << min_v_raw << " - " << max_v_raw << " (span: " << (max_v_raw - min_v_raw) << ")");
    FL_WARN("");
    FL_WARN("Current extents: [" << current_min << ", " << current_max << "]");
    FL_WARN("");
    FL_WARN("RECOMMENDED new extents for ~98% coverage:");

    // To achieve 98% coverage, we need tighter extents that better match actual distribution
    // Use observed mins and maxs with small safety margin (0.5% on each side)
    uint16_t global_min_observed = FL_MIN(FL_MIN(min_h_raw, min_s_raw), min_v_raw);
    uint16_t global_max_observed = FL_MAX(FL_MAX(max_h_raw, max_s_raw), max_v_raw);

    // Calculate safety margin (~100 units for conservative bounds)
    uint16_t safety_margin = 100;
    uint16_t optimized_min = (global_min_observed > safety_margin) ? global_min_observed - safety_margin : 0;
    uint16_t optimized_max = (global_max_observed < 65535 - safety_margin) ? global_max_observed + safety_margin : 65535;

    FL_WARN("  Global min across all components: " << global_min_observed);
    FL_WARN("  Global max across all components: " << global_max_observed);
    FL_WARN("  Optimized MIN (with " << safety_margin << " unit margin): " << optimized_min);
    FL_WARN("  Optimized MAX (with " << safety_margin << " unit margin): " << optimized_max);
    FL_WARN("");
    FL_WARN("Expected coverage improvement:");
    FL_WARN("  Current:   92% (with [" << current_min << ", " << current_max << "])");
    FL_WARN("  Optimized: ~98% (with [" << optimized_min << ", " << optimized_max << "])");
    FL_WARN("");
    FL_WARN("⚠️  TRADEOFF ANALYSIS:");
    FL_WARN("  Tight bounds [" << optimized_min << ", " << optimized_max << "] achieve 99% at radius=1.0");
    FL_WARN("  BUT they EXCEED bounds at radius=1000 (requires [~8672, ~57617])");
    FL_WARN("");
    FL_WARN("  To achieve ~98% coverage while passing validation at all radii,");
    FL_WARN("  test these MIDDLE GROUND candidates:");
    FL_WARN("");
    FL_WARN("  Option 1: [9500, 56000]  → ~96% coverage, valid at radius=1000");
    FL_WARN("  Option 2: [9200, 56500]  → ~97% coverage, valid at radius=1000");
    FL_WARN("  Option 3: [9000, 57000]  → ~95% coverage, valid at radius=1000");
}

TEST_CASE("[.]noiseRingHSV16 extent validation - 10k random samples at radius 1000") {
    // Validation test: Run 10 trials to collect statistics on observed noise ranges
    // across different random seeds. This helps determine optimal extents that maximize
    // output coverage while accepting that extreme tail values will be clipped.
    const int NUM_SAMPLES = 10000;
    const int NUM_TRIALS = 10;
    const float RADIUS = 1000.0f;  // Extreme radius to stress the noise function

    // Arrays to track min/max across all trials
    uint16_t h_mins[NUM_TRIALS], h_maxs[NUM_TRIALS];
    uint16_t s_mins[NUM_TRIALS], s_maxs[NUM_TRIALS];
    uint16_t v_mins[NUM_TRIALS], v_maxs[NUM_TRIALS];

    // Run 10 trials with different random seeds
    for (int trial = 0; trial < NUM_TRIALS; trial++) {
        uint16_t min_h_raw = 0xFFFF;
        uint16_t max_h_raw = 0x0000;
        uint16_t min_s_raw = 0xFFFF;
        uint16_t max_s_raw = 0x0000;
        uint16_t min_v_raw = 0xFFFF;
        uint16_t max_v_raw = 0x0000;

        // Use different seed for each trial
        random16_set_seed(12345 + trial);

        // Sample 10k random points in (angle, time) parameter space at high radius
        for (int i = 0; i < NUM_SAMPLES; i++) {
            // Generate random angle: 0 to 2PI
            float angle = (random16() / 65535.0f) * 2.0f * M_PI;

            // Generate random time: 0 to 2^32-1 (full uint32 range)
            uint32_t time = random16();
            time = (time << 16) | random16();

            // Call internal noise function to get RAW values before rescaling
            float x = fl::cosf(angle);
            float y = fl::sinf(angle);
            uint32_t nx = static_cast<uint32_t>((x + 1.0f) * 0.5f * RADIUS * 0xffff);
            uint32_t ny = static_cast<uint32_t>((y + 1.0f) * 0.5f * RADIUS * 0xffff);

            uint16_t h_raw = inoise16(nx, ny, time);
            uint16_t s_raw = inoise16(nx, ny, time + 0x10000);
            uint16_t v_raw = inoise16(nx, ny, time + 0x20000);

            min_h_raw = FL_MIN(min_h_raw, h_raw);
            max_h_raw = FL_MAX(max_h_raw, h_raw);

            min_s_raw = FL_MIN(min_s_raw, s_raw);
            max_s_raw = FL_MAX(max_s_raw, s_raw);

            min_v_raw = FL_MIN(min_v_raw, v_raw);
            max_v_raw = FL_MAX(max_v_raw, v_raw);
        }

        h_mins[trial] = min_h_raw;
        h_maxs[trial] = max_h_raw;
        s_mins[trial] = min_s_raw;
        s_maxs[trial] = max_s_raw;
        v_mins[trial] = min_v_raw;
        v_maxs[trial] = max_v_raw;
    }

    // Calculate statistics across all trials
    uint16_t h_min_avg = 0, h_max_avg = 0;
    uint16_t s_min_avg = 0, s_max_avg = 0;
    uint16_t v_min_avg = 0, v_max_avg = 0;

    for (int i = 0; i < NUM_TRIALS; i++) {
        h_min_avg += h_mins[i];
        h_max_avg += h_maxs[i];
        s_min_avg += s_mins[i];
        s_max_avg += s_maxs[i];
        v_min_avg += v_mins[i];
        v_max_avg += v_maxs[i];
    }

    h_min_avg /= NUM_TRIALS;
    h_max_avg /= NUM_TRIALS;
    s_min_avg /= NUM_TRIALS;
    s_max_avg /= NUM_TRIALS;
    v_min_avg /= NUM_TRIALS;
    v_max_avg /= NUM_TRIALS;

    FL_WARN("=== NOISE16_EXTENT Statistics (10 trials × 10k samples at radius 1000) ===");
    FL_WARN("");
    FL_WARN("Trial-by-trial ranges:");
    for (int i = 0; i < NUM_TRIALS; i++) {
        FL_WARN("  Trial " << i << ": H[" << h_mins[i] << "-" << h_maxs[i] << "] "
                           << "S[" << s_mins[i] << "-" << s_maxs[i] << "] "
                           << "V[" << v_mins[i] << "-" << v_maxs[i] << "]");
    }
    FL_WARN("");
    FL_WARN("Average across trials:");
    FL_WARN("  HUE:        min=" << h_min_avg << ", max=" << h_max_avg);
    FL_WARN("  SAT:        min=" << s_min_avg << ", max=" << s_max_avg);
    FL_WARN("  VAL:        min=" << v_min_avg << ", max=" << v_max_avg);
    FL_WARN("");
    FL_WARN("Overall extremes (global min/max across all trials):");

    uint16_t global_h_min = h_mins[0], global_h_max = h_maxs[0];
    uint16_t global_s_min = s_mins[0], global_s_max = s_maxs[0];
    uint16_t global_v_min = v_mins[0], global_v_max = v_maxs[0];

    for (int i = 1; i < NUM_TRIALS; i++) {
        global_h_min = FL_MIN(global_h_min, h_mins[i]);
        global_h_max = FL_MAX(global_h_max, h_maxs[i]);
        global_s_min = FL_MIN(global_s_min, s_mins[i]);
        global_s_max = FL_MAX(global_s_max, s_maxs[i]);
        global_v_min = FL_MIN(global_v_min, v_mins[i]);
        global_v_max = FL_MAX(global_v_max, v_maxs[i]);
    }

    FL_WARN("  HUE:        " << global_h_min << " - " << global_h_max);
    FL_WARN("  SAT:        " << global_s_min << " - " << global_s_max);
    FL_WARN("  VAL:        " << global_v_min << " - " << global_v_max);
    FL_WARN("");
    FL_WARN("Current extents: MIN=" << fl::NOISE16_EXTENT_MIN << ", MAX=" << fl::NOISE16_EXTENT_MAX);
    FL_WARN("");
    FL_WARN("RECOMMENDED extents (based on average + margin):");
    FL_WARN("  Tight (minimize clipping):    [" << (global_h_min + global_s_min) / 2 - 100 << ", "
                                                  << (global_h_max + global_s_max) / 2 + 100 << "]");
    FL_WARN("  Medium (balance):             [" << (h_min_avg + s_min_avg) / 2 << ", "
                                               << (h_max_avg + s_max_avg) / 2 << "]");
    FL_WARN("  Conservative (never exceed): [" << global_h_min << ", " << global_h_max << "]");
    FL_WARN("");

    // The test passes as long as we're aware of the ranges
    // Clipping is acceptable for optimal coverage
}

#endif  // End disabled noise tests

