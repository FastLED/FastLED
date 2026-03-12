#pragma once

#include "test.h"
#include "fl/gfx/gfx.h"
#include "fl/stl/chrono.h"
#include "fl/stl/cstdio.h"
#include "fl/stl/fixed_point/s16x16.h"

// Performance benchmark for 2D graphics primitives.
// All types are fl:: fixed-width integers; coordinates are s16x16 fixed-point.

static const fl::u16 kIters = 1000;

// Helper: compute ns/call = (total_us * 1000) / iters
static fl::u32 nsPerCall(fl::u32 us) { return (us * 1000) / kIters; }

FL_TEST_FILE(FL_FILEPATH) {

FL_TEST_CASE("Graphics performance - drawDisc") {
    CRGB buffer[1024] = {};
    fl::CanvasRGB canvas(buffer, 32, 32);
    fl::s16x16 cx = fl::s16x16(16), cy = fl::s16x16(16);
    canvas.drawDisc(CRGB(255, 0, 0), cx, cy, fl::s16x16(8));

    auto start = fl::chrono::steady_clock::now();
    for (fl::u16 i = 0; i < kIters; ++i) {
        fl::s16x16 r = fl::s16x16(2) + fl::s16x16::from_raw((fl::i32)(i % 10) * (fl::s16x16::SCALE / 2));
        canvas.drawDisc(CRGB(255, 0, 0), cx, cy, r);
    }
    auto end = fl::chrono::steady_clock::now();
    fl::u32 us = (fl::u32)fl::chrono::duration_cast<fl::chrono::microseconds>(end - start).count();
    fl::printf("  drawDisc: %u us total, %u ns/call\n", (unsigned)us, (unsigned)nsPerCall(us));
    FL_CHECK(us < 100000);
}

FL_TEST_CASE("Graphics performance - drawDisc large") {
    CRGB buffer[4096] = {};
    fl::CanvasRGB canvas(buffer, 64, 64);
    fl::s16x16 cx = fl::s16x16(32), cy = fl::s16x16(32);
    canvas.drawDisc(CRGB(255, 0, 0), cx, cy, fl::s16x16(20));

    auto start = fl::chrono::steady_clock::now();
    for (fl::u16 i = 0; i < kIters; ++i) {
        fl::s16x16 r = fl::s16x16(8) + fl::s16x16::from_raw((fl::i32)(i % 20) * (fl::s16x16::SCALE / 2));
        canvas.drawDisc(CRGB(255, 0, 0), cx, cy, r);
    }
    auto end = fl::chrono::steady_clock::now();
    fl::u32 us = (fl::u32)fl::chrono::duration_cast<fl::chrono::microseconds>(end - start).count();
    fl::printf("  drawDisc(large): %u us total, %u ns/call\n", (unsigned)us, (unsigned)nsPerCall(us));
    FL_CHECK(us < 200000);
}

FL_TEST_CASE("Graphics performance - drawRing") {
    CRGB buffer[1024] = {};
    fl::CanvasRGB canvas(buffer, 32, 32);
    fl::s16x16 cx = fl::s16x16(16), cy = fl::s16x16(16);
    canvas.drawRing(CRGB(0, 255, 0), cx, cy, fl::s16x16(6), fl::s16x16(2));

    auto start = fl::chrono::steady_clock::now();
    for (fl::u16 i = 0; i < kIters; ++i) {
        fl::s16x16 r = fl::s16x16(2) + fl::s16x16::from_raw((fl::i32)(i % 8) * (fl::s16x16::SCALE / 2));
        fl::s16x16 t = fl::s16x16(1) + fl::s16x16::from_raw((fl::i32)(i % 4) * (fl::s16x16::SCALE / 4));
        canvas.drawRing(CRGB(0, 255, 0), cx, cy, r, t);
    }
    auto end = fl::chrono::steady_clock::now();
    fl::u32 us = (fl::u32)fl::chrono::duration_cast<fl::chrono::microseconds>(end - start).count();
    fl::printf("  drawRing: %u us total, %u ns/call\n", (unsigned)us, (unsigned)nsPerCall(us));
    FL_CHECK(us < 150000);
}

FL_TEST_CASE("Graphics performance - drawLine") {
    CRGB buffer[1024] = {};
    fl::CanvasRGB canvas(buffer, 32, 32);
    canvas.drawLine(CRGB(0, 0, 255), fl::s16x16(0), fl::s16x16(0), fl::s16x16(31), fl::s16x16(31));

    auto start = fl::chrono::steady_clock::now();
    for (fl::u16 i = 0; i < kIters; ++i) {
        fl::s16x16 xe = fl::s16x16((fl::i32)(i % 32));
        fl::s16x16 ye = fl::s16x16((fl::i32)((i * 7) % 32));
        canvas.drawLine(CRGB(0, 0, 255), fl::s16x16(0), fl::s16x16(0), xe, ye);
    }
    auto end = fl::chrono::steady_clock::now();
    fl::u32 us = (fl::u32)fl::chrono::duration_cast<fl::chrono::microseconds>(end - start).count();
    fl::printf("  drawLine: %u us total, %u ns/call\n", (unsigned)us, (unsigned)nsPerCall(us));
    FL_CHECK(us < 80000);
}

FL_TEST_CASE("Graphics performance - drawStrokeLine") {
    CRGB buffer[1024] = {};
    fl::CanvasRGB canvas(buffer, 32, 32);
    canvas.drawStrokeLine(CRGB(255, 255, 0), fl::s16x16(0), fl::s16x16(0),
                          fl::s16x16(31), fl::s16x16(31), fl::s16x16(2));

    auto start = fl::chrono::steady_clock::now();
    for (fl::u16 i = 0; i < kIters; ++i) {
        fl::s16x16 xe = fl::s16x16((fl::i32)(i % 32));
        fl::s16x16 ye = fl::s16x16((fl::i32)((i * 7) % 32));
        fl::s16x16 t = fl::s16x16(1 + (fl::i32)(i % 4));
        canvas.drawStrokeLine(CRGB(255, 255, 0), fl::s16x16(0), fl::s16x16(0), xe, ye, t);
    }
    auto end = fl::chrono::steady_clock::now();
    fl::u32 us = (fl::u32)fl::chrono::duration_cast<fl::chrono::microseconds>(end - start).count();
    fl::printf("  drawStrokeLine: %u us total, %u ns/call\n", (unsigned)us, (unsigned)nsPerCall(us));
}

// === OVERWRITE MODE benchmarks (same workloads, DRAW_MODE_OVERWRITE) ===

FL_TEST_CASE("Graphics performance - drawDisc OVERWRITE") {
    CRGB buffer[1024] = {};
    fl::CanvasRGB canvas(buffer, 32, 32);
    fl::s16x16 cx = fl::s16x16(16), cy = fl::s16x16(16);
    canvas.drawDisc(CRGB(255, 0, 0), cx, cy, fl::s16x16(8), fl::DRAW_MODE_OVERWRITE);

    auto start = fl::chrono::steady_clock::now();
    for (fl::u16 i = 0; i < kIters; ++i) {
        fl::s16x16 r = fl::s16x16(2) + fl::s16x16::from_raw((fl::i32)(i % 10) * (fl::s16x16::SCALE / 2));
        canvas.drawDisc(CRGB(255, 0, 0), cx, cy, r, fl::DRAW_MODE_OVERWRITE);
    }
    auto end = fl::chrono::steady_clock::now();
    fl::u32 us = (fl::u32)fl::chrono::duration_cast<fl::chrono::microseconds>(end - start).count();
    fl::printf("  drawDisc(OW): %u us total, %u ns/call\n", (unsigned)us, (unsigned)nsPerCall(us));
    FL_CHECK(us < 100000);
}

FL_TEST_CASE("Graphics performance - drawDisc large OVERWRITE") {
    CRGB buffer[4096] = {};
    fl::CanvasRGB canvas(buffer, 64, 64);
    fl::s16x16 cx = fl::s16x16(32), cy = fl::s16x16(32);
    canvas.drawDisc(CRGB(255, 0, 0), cx, cy, fl::s16x16(20), fl::DRAW_MODE_OVERWRITE);

    auto start = fl::chrono::steady_clock::now();
    for (fl::u16 i = 0; i < kIters; ++i) {
        fl::s16x16 r = fl::s16x16(8) + fl::s16x16::from_raw((fl::i32)(i % 20) * (fl::s16x16::SCALE / 2));
        canvas.drawDisc(CRGB(255, 0, 0), cx, cy, r, fl::DRAW_MODE_OVERWRITE);
    }
    auto end = fl::chrono::steady_clock::now();
    fl::u32 us = (fl::u32)fl::chrono::duration_cast<fl::chrono::microseconds>(end - start).count();
    fl::printf("  drawDisc(large,OW): %u us total, %u ns/call\n", (unsigned)us, (unsigned)nsPerCall(us));
    FL_CHECK(us < 200000);
}

FL_TEST_CASE("Graphics performance - drawRing OVERWRITE") {
    CRGB buffer[1024] = {};
    fl::CanvasRGB canvas(buffer, 32, 32);
    fl::s16x16 cx = fl::s16x16(16), cy = fl::s16x16(16);
    canvas.drawRing(CRGB(0, 255, 0), cx, cy, fl::s16x16(6), fl::s16x16(2), fl::DRAW_MODE_OVERWRITE);

    auto start = fl::chrono::steady_clock::now();
    for (fl::u16 i = 0; i < kIters; ++i) {
        fl::s16x16 r = fl::s16x16(2) + fl::s16x16::from_raw((fl::i32)(i % 8) * (fl::s16x16::SCALE / 2));
        fl::s16x16 t = fl::s16x16(1) + fl::s16x16::from_raw((fl::i32)(i % 4) * (fl::s16x16::SCALE / 4));
        canvas.drawRing(CRGB(0, 255, 0), cx, cy, r, t, fl::DRAW_MODE_OVERWRITE);
    }
    auto end = fl::chrono::steady_clock::now();
    fl::u32 us = (fl::u32)fl::chrono::duration_cast<fl::chrono::microseconds>(end - start).count();
    fl::printf("  drawRing(OW): %u us total, %u ns/call\n", (unsigned)us, (unsigned)nsPerCall(us));
    FL_CHECK(us < 150000);
}

FL_TEST_CASE("Graphics performance - drawLine OVERWRITE") {
    CRGB buffer[1024] = {};
    fl::CanvasRGB canvas(buffer, 32, 32);
    canvas.drawLine(CRGB(0, 0, 255), fl::s16x16(0), fl::s16x16(0),
                    fl::s16x16(31), fl::s16x16(31), fl::DRAW_MODE_OVERWRITE);

    auto start = fl::chrono::steady_clock::now();
    for (fl::u16 i = 0; i < kIters; ++i) {
        fl::s16x16 xe = fl::s16x16((fl::i32)(i % 32));
        fl::s16x16 ye = fl::s16x16((fl::i32)((i * 7) % 32));
        canvas.drawLine(CRGB(0, 0, 255), fl::s16x16(0), fl::s16x16(0), xe, ye, fl::DRAW_MODE_OVERWRITE);
    }
    auto end = fl::chrono::steady_clock::now();
    fl::u32 us = (fl::u32)fl::chrono::duration_cast<fl::chrono::microseconds>(end - start).count();
    fl::printf("  drawLine(OW): %u us total, %u ns/call\n", (unsigned)us, (unsigned)nsPerCall(us));
    FL_CHECK(us < 80000);
}

FL_TEST_CASE("Graphics performance - drawStrokeLine OVERWRITE") {
    CRGB buffer[1024] = {};
    fl::CanvasRGB canvas(buffer, 32, 32);
    canvas.drawStrokeLine(CRGB(255, 255, 0), fl::s16x16(0), fl::s16x16(0),
                          fl::s16x16(31), fl::s16x16(31), fl::s16x16(2),
                          fl::LineCap::FLAT, fl::DRAW_MODE_OVERWRITE);

    auto start = fl::chrono::steady_clock::now();
    for (fl::u16 i = 0; i < kIters; ++i) {
        fl::s16x16 xe = fl::s16x16((fl::i32)(i % 32));
        fl::s16x16 ye = fl::s16x16((fl::i32)((i * 7) % 32));
        fl::s16x16 t = fl::s16x16(1 + (fl::i32)(i % 4));
        canvas.drawStrokeLine(CRGB(255, 255, 0), fl::s16x16(0), fl::s16x16(0), xe, ye, t,
                              fl::LineCap::FLAT, fl::DRAW_MODE_OVERWRITE);
    }
    auto end = fl::chrono::steady_clock::now();
    fl::u32 us = (fl::u32)fl::chrono::duration_cast<fl::chrono::microseconds>(end - start).count();
    fl::printf("  drawStrokeLine(OW): %u us total, %u ns/call\n", (unsigned)us, (unsigned)nsPerCall(us));
}

}  // FL_TEST_FILE
