#pragma once

#include "test.h"
#include "fl/gfx/gfx.h"
#include "fl/stl/chrono.h"
#include "fl/stl/cstdio.h"

// Simple performance benchmark for 2D graphics primitives
// Tests drawDisc, drawRing, and drawLine on a 32x32 canvas

FL_TEST_FILE(FL_FILEPATH) {

FL_TEST_CASE("Graphics performance - drawDisc") {
    CRGB buffer[1024] = {};
    fl::CanvasRGB canvas(buffer, 32, 32);

    // Warm up
    canvas.drawDisc(CRGB(255, 0, 0), 16.0f, 16.0f, 8.0f);

    // Benchmark: 100 circles of varying sizes
    auto start = fl::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i) {
        float radius = 2.0f + (i % 10) * 0.5f;
        canvas.drawDisc(CRGB(255, 0, 0), 16.0f, 16.0f, radius);
    }
    auto end = fl::chrono::steady_clock::now();
    auto duration = fl::chrono::duration_cast<fl::chrono::microseconds>(end - start);

    double avg_us = duration.count() / 100.0;
    printf("✓ drawDisc: 100 circles in %lu µs (avg %.2f µs/call, ~%d calls/sec)\n",
           (unsigned long)duration.count(), avg_us, (int)(1000000.0 / avg_us));
    FL_CHECK(duration.count() < 100000);  // Should complete in < 100ms
}

FL_TEST_CASE("Graphics performance - drawRing") {
    CRGB buffer[1024] = {};
    fl::CanvasRGB canvas(buffer, 32, 32);

    // Warm up
    canvas.drawRing(CRGB(0, 255, 0), 16.0f, 16.0f, 6.0f, 2.0f);

    // Benchmark: 100 rings with varying radii/thickness
    auto start = fl::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i) {
        float radius = 2.0f + (i % 8) * 0.5f;
        float thickness = 1.0f + (i % 4) * 0.25f;
        canvas.drawRing(CRGB(0, 255, 0), 16.0f, 16.0f, radius, thickness);
    }
    auto end = fl::chrono::steady_clock::now();
    auto duration = fl::chrono::duration_cast<fl::chrono::microseconds>(end - start);

    double avg_us_ring = duration.count() / 100.0;
    printf("✓ drawRing: 100 rings in %lu µs (avg %.2f µs/call, ~%d calls/sec)\n",
           (unsigned long)duration.count(), avg_us_ring, (int)(1000000.0 / avg_us_ring));
    FL_CHECK(duration.count() < 150000);  // Should complete in < 150ms
}

FL_TEST_CASE("Graphics performance - drawLine") {
    CRGB buffer[1024] = {};
    fl::CanvasRGB canvas(buffer, 32, 32);

    // Warm up
    canvas.drawLine(CRGB(0, 0, 255), 0.0f, 0.0f, 31.0f, 31.0f);

    // Benchmark: 100 lines with varying slopes
    auto start = fl::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i) {
        float x_end = (i % 32);
        float y_end = ((i * 7) % 32);
        canvas.drawLine(CRGB(0, 0, 255), 0.0f, 0.0f, x_end, y_end);
    }
    auto end = fl::chrono::steady_clock::now();
    auto duration = fl::chrono::duration_cast<fl::chrono::microseconds>(end - start);

    double avg_us_line = duration.count() / 100.0;
    printf("✓ drawLine: 100 lines in %lu µs (avg %.2f µs/call, ~%d calls/sec)\n",
           (unsigned long)duration.count(), avg_us_line, (int)(1000000.0 / avg_us_line));
    FL_CHECK(duration.count() < 80000);  // Should complete in < 80ms
}

FL_TEST_CASE("Graphics performance - drawStrokeLine") {
    CRGB buffer[1024] = {};
    fl::CanvasRGB canvas(buffer, 32, 32);

    // Warm up
    canvas.drawStrokeLine(CRGB(255, 255, 0), 0.0f, 0.0f, 31.0f, 31.0f, 2.0f);

    // Benchmark: 100 stroked lines
    auto start = fl::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i) {
        float x_end = (i % 32);
        float y_end = ((i * 7) % 32);
        float thickness = 1.0f + (i % 4);
        canvas.drawStrokeLine(CRGB(255, 255, 0), 0.0f, 0.0f, x_end, y_end, thickness);
    }
    auto end = fl::chrono::steady_clock::now();
    auto duration = fl::chrono::duration_cast<fl::chrono::microseconds>(end - start);

    double avg_us_stroke = duration.count() / 100.0;
    printf("⚠️  drawStrokeLine: 100 stroked lines in %lu µs (avg %.2f µs/call, ~%d calls/sec) [sqrt per pixel]\n",
           (unsigned long)duration.count(), avg_us_stroke, (int)(1000000.0 / avg_us_stroke));
}

}  // FL_TEST_FILE
