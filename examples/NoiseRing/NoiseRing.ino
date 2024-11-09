

/// @file    NoisePlusPalette.ino
/// @brief   Demonstrates how to mix noise generation with color palettes on a
/// 2D LED matrix
/// @example NoisePlusPalette.ino

#include <Arduino.h>

#include <stdio.h>

#include "json.h"
#include "math_macros.h"
#include "noisegen.h"
#include "screenmap.h"
#include "slice.h"
#include "ui.h"
#include "FastLED.h"
#include "pir.h"
#include "timer.h"

#define LED_PIN 2
#define BRIGHTNESS 96
#define COLOR_ORDER GRB
#define NUM_LEDS 250
#define PIN_PIR 0

CRGB leds[NUM_LEDS];

Slider brightness("Brightness", 255, 0, 255);
Slider scale("Scale", 4, .1, 4, .1);
Slider timeBitshift("Time Bitshift", 5, 0, 16, 1);
Slider timescale("Time Scale", 1, .1, 10, .1);
Pir pir(PIN_PIR);

Timer timer;
float current_brightness = 0;
bool first = true;

uint8_t update_brightness(bool clicked, uint32_t now) {
    if (clicked) {
        timer.start(millis(), 1000 * 60 * 3);
    }
    if (timer.update(now)) {
        current_brightness += .6;
    } else {
        current_brightness *= .97;
    }
    uint32_t brightness = current_brightness;
    if (brightness > 255) {
        brightness = 255;
    }
    return brightness;
}


void setup() {
    ScreenMap xyMap = ScreenMap::Circle(NUM_LEDS, 2.0, 2.0);
    FastLED.addLeds<WS2811, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)
        .setCorrection(TypicalLEDStrip)
        .setScreenMap(xyMap);
    FastLED.setBrightness(brightness);
}

void loop() {
    const bool detectMotion = pir.detect();
    const bool detected_motion = detectMotion || first || detectMotion;
    first = false;
    uint8_t bri = update_brightness(detected_motion, millis());
    FastLED.setBrightness(bri);
    uint32_t now = millis();
    double angle_offset = double(now) / 32000.0 * 2 * M_PI;
    //angle_offset = 0;
    now = (now << timeBitshift.as<int>()) * timescale.as<double>();
    // inoise8(x + ioffset,y + joffset,z);
    // go in circular formation and set the leds
    for (int i = 0; i < NUM_LEDS; i++) {
        float angle = i * 2 * M_PI / NUM_LEDS + angle_offset;
        float x = cos(angle);
        float y = sin(angle);
        x *= 0xffff * scale.as<double>();
        y *= 0xffff * scale.as<double>();
        uint16_t noise = inoise16(x, y, now);
        uint16_t noise2 = inoise16(x, y, 0xfff + now);
        uint16_t noise3 = inoise16(x, y, 0xffff + now);
        noise3 = noise3 >> 8;
        int16_t noise4 = map(noise3, 0, 255, -64, 255);
        if (noise4 < 0) {
            noise4 = 0;
        }

        leds[i] = CHSV(noise >> 8, MAX(128, noise2 >> 8), noise4);
        // std::swap(leds[i].b, leds[i].g);
    }
    FastLED.show();
}
