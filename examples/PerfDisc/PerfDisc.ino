/// @file    PerfDisc.ino
/// @brief   Benchmark drawDisc on AVR (32x8 canvas, blend mode)
/// @example PerfDisc.ino

#include <Arduino.h>
#include <FastLED.h>
#include <fl/gfx/gfx.h>

#define NUM_LEDS 1
#ifndef PIN_DATA
#define PIN_DATA 3
#endif

CRGB leds[NUM_LEDS];

// 32x8 = 256 pixels = 768 bytes — fits AVR 2KB RAM with room for stack.
static const int W = 32;
static const int H = 8;
static CRGB canvas_buf[W * H];

void setup() {
    Serial.begin(115200);
    FastLED.addLeds<NEOPIXEL, PIN_DATA>(leds, NUM_LEDS);
    delay(500);
    Serial.println(F("\n--- PerfDisc AVR Benchmark ---"));
}

void loop() {
    fl::CanvasRGB canvas(fl::span<CRGB>(canvas_buf, W * H), W, H);
    const int ITERS = 100;

    // Blend mode — radius 6 (spans ~12px)
    unsigned long t0 = micros();
    for (int i = 0; i < ITERS; i++) {
        canvas.drawDisc(CRGB::Blue, 16, 4, 6);
    }
    unsigned long t1 = micros();
    unsigned long bl_r6 = (t1 - t0) / ITERS;

    // Blend mode — radius 10 (spans ~20px)
    t0 = micros();
    for (int i = 0; i < ITERS; i++) {
        canvas.drawDisc(CRGB(255, 255, 255), 16, 4, 10);
    }
    t1 = micros();
    unsigned long bl_r10 = (t1 - t0) / ITERS;

    Serial.print(F("BL r6: ")); Serial.print(bl_r6); Serial.println(F(" us"));
    Serial.print(F("BL r10: ")); Serial.print(bl_r10); Serial.println(F(" us"));
    Serial.println(F("---"));

    delay(3000);
}
