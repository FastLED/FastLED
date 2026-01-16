#include "noise_test_helpers.h"

using namespace fl;
using namespace noise_test_helpers;

// Temporarily disable all non-critical noise tests for faster test runs
#if 0

TEST_CASE("[.]noiseCylinderHSV8 temporal smoothness - small time delta") {
    const int NUM_LEDS = 128;
    const float ANGLE_STEP = 2.0f * M_PI / NUM_LEDS;
    const float HEIGHT = 0.5f;

    CRGB frame_t0[NUM_LEDS];
    CRGB frame_t1[NUM_LEDS];

    uint32_t time_base = 5000;
    float radius = 1.0f;

    // Generate frame at time t0
    for (int i = 0; i < NUM_LEDS; i++) {
        float angle = i * ANGLE_STEP;
        CHSV hsv = noiseCylinderHSV8(angle, HEIGHT, time_base, radius);
        frame_t0[i] = hsv;
    }

    // Generate frame at t0 + 1ms (small time delta)
    // Noise should change very smoothly - small average color difference
    for (int i = 0; i < NUM_LEDS; i++) {
        float angle = i * ANGLE_STEP;
        CHSV hsv = noiseCylinderHSV8(angle, HEIGHT, time_base + 1, radius);
        frame_t1[i] = hsv;
    }

    float avg_diff_1ms = calcAverageColorDifference(frame_t0, frame_t1, NUM_LEDS);

    FL_WARN("=== noiseCylinderHSV8 Temporal Smoothness Test (Δt=1ms) ===");
    FL_WARN("Average color pixel difference: " << avg_diff_1ms);
    FL_WARN("Threshold for smooth animation: < 5.0");

    // At 1ms, the noise should change only minimally
    // This tests that the noise function provides smooth animation
    // as time progresses in small increments
    CHECK_LT(avg_diff_1ms, 5.0f);
}

TEST_CASE("[.]noiseCylinderHSV8 temporal evolution - large time delta") {
    const int NUM_LEDS = 128;
    const float ANGLE_STEP = 2.0f * M_PI / NUM_LEDS;
    const float HEIGHT = 0.5f;

    CRGB frame_t0[NUM_LEDS];
    CRGB frame_t1[NUM_LEDS];

    uint32_t time_base = 1000;
    float radius = 1.0f;

    // Generate frame at time t0
    for (int i = 0; i < NUM_LEDS; i++) {
        float angle = i * ANGLE_STEP;
        CHSV hsv = noiseCylinderHSV8(angle, HEIGHT, time_base, radius);
        frame_t0[i] = hsv;
    }

    // Generate frame at t0 + 1 second (large time delta)
    // Noise should change significantly - large average color difference
    for (int i = 0; i < NUM_LEDS; i++) {
        float angle = i * ANGLE_STEP;
        CHSV hsv = noiseCylinderHSV8(angle, HEIGHT, time_base + 1000, radius);
        frame_t1[i] = hsv;
    }

    float avg_diff_1sec = calcAverageColorDifference(frame_t0, frame_t1, NUM_LEDS);

    FL_WARN("=== noiseCylinderHSV8 Temporal Evolution Test (Δt=1s) ===");
    FL_WARN("Average color pixel difference: " << avg_diff_1sec);
    FL_WARN("Threshold for significant evolution: > 0.01");

    // At 1 second, the noise should have evolved significantly
    // This tests that the pattern changes meaningfully over time
    CHECK_GT(avg_diff_1sec, 0.01f);
}

TEST_CASE("[.]noiseCylinderHSV8 temporal response ratio") {
    const int NUM_LEDS = 128;
    const float ANGLE_STEP = 2.0f * M_PI / NUM_LEDS;
    const float HEIGHT = 0.5f;

    CRGB frame_t0[NUM_LEDS];
    CRGB frame_t1_small[NUM_LEDS];
    CRGB frame_t1_large[NUM_LEDS];

    uint32_t time_base = 10000;
    float radius = 1.5f;

    // Generate frame at time t0
    for (int i = 0; i < NUM_LEDS; i++) {
        float angle = i * ANGLE_STEP;
        CHSV hsv = noiseCylinderHSV8(angle, HEIGHT, time_base, radius);
        frame_t0[i] = hsv;
    }

    // Generate frame at t0 + 1ms
    for (int i = 0; i < NUM_LEDS; i++) {
        float angle = i * ANGLE_STEP;
        CHSV hsv = noiseCylinderHSV8(angle, HEIGHT, time_base + 1, radius);
        frame_t1_small[i] = hsv;
    }

    // Generate frame at t0 + 1 second
    for (int i = 0; i < NUM_LEDS; i++) {
        float angle = i * ANGLE_STEP;
        CHSV hsv = noiseCylinderHSV8(angle, HEIGHT, time_base + 1000, radius);
        frame_t1_large[i] = hsv;
    }

    float avg_diff_1ms = calcAverageColorDifference(frame_t0, frame_t1_small, NUM_LEDS);
    float avg_diff_1sec = calcAverageColorDifference(frame_t0, frame_t1_large, NUM_LEDS);
    float ratio = (avg_diff_1ms > 0.1f) ? (avg_diff_1sec / avg_diff_1ms) : avg_diff_1sec;

    FL_WARN("=== noiseCylinderHSV8 Temporal Response Ratio Test ===");
    FL_WARN("Δt=1ms: " << avg_diff_1ms);
    FL_WARN("Δt=1s: " << avg_diff_1sec);
    FL_WARN("Ratio (1s / 1ms): " << ratio);
    FL_WARN("Expected ratio: > 1.0x (1 second change > 1 millisecond change)");

    // Small time changes should produce proportionally smaller differences
    // Large time changes should produce proportionally larger differences
    CHECK_GT(avg_diff_1sec, avg_diff_1ms);
}

TEST_CASE("[.]noiseCylinderCRGB temporal test") {
    const int NUM_LEDS = 128;
    const float ANGLE_STEP = 2.0f * M_PI / NUM_LEDS;
    const float HEIGHT = 0.5f;

    CRGB frame_t0[NUM_LEDS];
    CRGB frame_t1_small[NUM_LEDS];
    CRGB frame_t1_large[NUM_LEDS];

    uint32_t time_base = 20000;
    float radius = 2.0f;

    // Generate frame at time t0
    for (int i = 0; i < NUM_LEDS; i++) {
        float angle = i * ANGLE_STEP;
        frame_t0[i] = noiseCylinderCRGB(angle, HEIGHT, time_base, radius);
    }

    // Generate at t0 + 1ms
    for (int i = 0; i < NUM_LEDS; i++) {
        float angle = i * ANGLE_STEP;
        frame_t1_small[i] = noiseCylinderCRGB(angle, HEIGHT, time_base + 1, radius);
    }

    // Generate at t0 + 1 second
    for (int i = 0; i < NUM_LEDS; i++) {
        float angle = i * ANGLE_STEP;
        frame_t1_large[i] = noiseCylinderCRGB(angle, HEIGHT, time_base + 1000, radius);
    }

    float avg_diff_1ms = calcAverageColorDifference(frame_t0, frame_t1_small, NUM_LEDS);
    float avg_diff_1sec = calcAverageColorDifference(frame_t0, frame_t1_large, NUM_LEDS);

    FL_WARN("=== noiseCylinderCRGB Temporal Test ===");
    FL_WARN("Δt=1ms average difference: " << avg_diff_1ms);
    FL_WARN("Δt=1s average difference: " << avg_diff_1sec);

    // Small delta should be small
    CHECK_LT(avg_diff_1ms, 5.0f);

    // Large delta should be larger than small delta
    CHECK_GT(avg_diff_1sec, avg_diff_1ms);
}

TEST_CASE("[.]noiseCylinderHSV16 full circumference coverage") {
    const int NUM_ANGLES = 256;
    const float ANGLE_STEP = 2.0f * M_PI / NUM_ANGLES;
    const float HEIGHT = 0.5f;

    uint32_t time_sample = 12345;
    float radius = 1.0f;

    fl::HSV16 min_hsv(0xFFFF, 0xFFFF, 0xFFFF);
    fl::HSV16 max_hsv(0, 0, 0);
    uint32_t h_sum = 0, s_sum = 0, v_sum = 0;

    // Sample the full circumference
    for (int i = 0; i < NUM_ANGLES; i++) {
        float angle = i * ANGLE_STEP;
        fl::HSV16 hsv = noiseCylinderHSV16(angle, HEIGHT, time_sample, radius);

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

    uint32_t h_avg = h_sum / NUM_ANGLES;
    uint32_t s_avg = s_sum / NUM_ANGLES;
    uint32_t v_avg = v_sum / NUM_ANGLES;

    FL_WARN("=== noiseCylinderHSV16 Full Circumference Coverage ===");
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

TEST_CASE("[.]noiseCylinderHSV8 radius level of detail") {
    const int NUM_LEDS = 64;
    const float ANGLE_STEP = 2.0f * M_PI / NUM_LEDS;
    const float HEIGHT = 0.5f;

    uint32_t time_sample = 54321;

    CRGB frame_radius_0p5[NUM_LEDS];
    CRGB frame_radius_2p0[NUM_LEDS];

    // Generate at radius = 0.5 (fine detail)
    for (int i = 0; i < NUM_LEDS; i++) {
        float angle = i * ANGLE_STEP;
        CHSV hsv = noiseCylinderHSV8(angle, HEIGHT, time_sample, 0.5f);
        frame_radius_0p5[i] = hsv;
    }

    // Generate at radius = 2.0 (coarse detail)
    for (int i = 0; i < NUM_LEDS; i++) {
        float angle = i * ANGLE_STEP;
        CHSV hsv = noiseCylinderHSV8(angle, HEIGHT, time_sample, 2.0f);
        frame_radius_2p0[i] = hsv;
    }

    float avg_diff = calcAverageColorDifference(frame_radius_0p5, frame_radius_2p0, NUM_LEDS);

    FL_WARN("=== noiseCylinderHSV8 Radius Level of Detail Test ===");
    FL_WARN("Average color difference (radius 0.5 vs 2.0): " << avg_diff);
    FL_WARN("Different radius values should sample different detail levels");

    // Different radius values should produce noticeably different patterns
    CHECK_GT(avg_diff, 10.0f);
}

TEST_CASE("[.]noiseCylinderHSV8 height variation") {
    const int NUM_HEIGHTS = 64;
    const float ANGLE = 0.0f;  // Fixed angle

    uint32_t time_sample = 54321;
    float radius = 1.0f;

    CRGB frame_bottom[NUM_HEIGHTS];
    CRGB frame_top[NUM_HEIGHTS];

    // Generate at bottom (height = 0.0)
    for (int i = 0; i < NUM_HEIGHTS; i++) {
        float height = i / (float)NUM_HEIGHTS;  // 0.0 to ~1.0
        CHSV hsv = noiseCylinderHSV8(ANGLE, height, time_sample, radius);
        frame_bottom[i] = hsv;
    }

    // Generate at top (height = 1.0 to 2.0)
    for (int i = 0; i < NUM_HEIGHTS; i++) {
        float height = 1.0f + (i / (float)NUM_HEIGHTS);  // 1.0 to ~2.0
        CHSV hsv = noiseCylinderHSV8(ANGLE, height, time_sample, radius);
        frame_top[i] = hsv;
    }

    float avg_diff = calcAverageColorDifference(frame_bottom, frame_top, NUM_HEIGHTS);

    FL_WARN("=== noiseCylinderHSV8 Height Variation Test ===");
    FL_WARN("Average color difference (bottom vs top): " << avg_diff);
    FL_WARN("Different heights should produce different patterns");

    // Different heights should produce noticeably different patterns
    CHECK_GT(avg_diff, 5.0f);
}

TEST_CASE("[.]noiseCylinderHSV16 full cylinder coverage (angle + height)") {
    const int ANGLE_SAMPLES = 32;
    const int HEIGHT_SAMPLES = 16;
    const float ANGLE_STEP = 2.0f * M_PI / ANGLE_SAMPLES;
    const float HEIGHT_STEP = 1.0f / HEIGHT_SAMPLES;

    uint32_t time_sample = 12345;
    float radius = 1.0f;

    fl::HSV16 min_hsv(0xFFFF, 0xFFFF, 0xFFFF);
    fl::HSV16 max_hsv(0, 0, 0);
    uint32_t h_sum = 0, s_sum = 0, v_sum = 0;
    int sample_count = 0;

    // Sample across the full cylinder surface
    for (int a = 0; a < ANGLE_SAMPLES; a++) {
        for (int h = 0; h < HEIGHT_SAMPLES; h++) {
            float angle = a * ANGLE_STEP;
            float height = h * HEIGHT_STEP;
            fl::HSV16 hsv = noiseCylinderHSV16(angle, height, time_sample, radius);

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

    FL_WARN("=== noiseCylinderHSV16 Full Cylinder Coverage ===");
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

TEST_CASE("[.]noiseCylinderCRGB full cylinder coverage") {
    const int ANGLE_SAMPLES = 32;
    const int HEIGHT_SAMPLES = 16;
    const float ANGLE_STEP = 2.0f * M_PI / ANGLE_SAMPLES;
    const float HEIGHT_STEP = 1.0f / HEIGHT_SAMPLES;

    uint32_t time_sample = 54321;
    float radius = 1.5f;

    uint16_t min_r = 0xFFFF, max_r = 0x0000;
    uint16_t min_g = 0xFFFF, max_g = 0x0000;
    uint16_t min_b = 0xFFFF, max_b = 0x0000;
    uint32_t r_sum = 0, g_sum = 0, b_sum = 0;
    int sample_count = 0;

    // Sample across the full cylinder surface
    for (int a = 0; a < ANGLE_SAMPLES; a++) {
        for (int h = 0; h < HEIGHT_SAMPLES; h++) {
            float angle = a * ANGLE_STEP;
            float height = h * HEIGHT_STEP;
            CRGB rgb = noiseCylinderCRGB(angle, height, time_sample, radius);

            // Track ranges
            min_r = FL_MIN(min_r, (uint16_t)rgb.r);
            max_r = FL_MAX(max_r, (uint16_t)rgb.r);

            min_g = FL_MIN(min_g, (uint16_t)rgb.g);
            max_g = FL_MAX(max_g, (uint16_t)rgb.g);

            min_b = FL_MIN(min_b, (uint16_t)rgb.b);
            max_b = FL_MAX(max_b, (uint16_t)rgb.b);

            // Track sums for average
            r_sum += rgb.r;
            g_sum += rgb.g;
            b_sum += rgb.b;
            sample_count++;
        }
    }

    uint16_t r_avg = r_sum / sample_count;
    uint16_t g_avg = g_sum / sample_count;
    uint16_t b_avg = b_sum / sample_count;

    FL_WARN("=== noiseCylinderCRGB Full Cylinder Coverage ===");
    FL_WARN("Samples: " << sample_count);
    FL_WARN("Red   - min: " << min_r << ", max: " << max_r << ", avg: " << r_avg << ", span: " << (max_r - min_r));
    FL_WARN("Green - min: " << min_g << ", max: " << max_g << ", avg: " << g_avg << ", span: " << (max_g - min_g));
    FL_WARN("Blue  - min: " << min_b << ", max: " << max_b << ", avg: " << b_avg << ", span: " << (max_b - min_b));

    // All components should have decent range variation
    CHECK_GT(max_r - min_r, 30);  // Good red variation
    CHECK_GT(max_g - min_g, 30);  // Good green variation
    CHECK_GT(max_b - min_b, 30);  // Good blue variation

    // Averages should be reasonably distributed (not too dark or bright)
    CHECK_GT(r_avg, 25);
    CHECK_LT(r_avg, 230);
}

#endif  // End disabled noise tests
