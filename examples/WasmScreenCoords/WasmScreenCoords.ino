/// @file    NoisePlusPalette.ino
/// @brief   Demonstrates how to mix noise generation with color palettes on a
/// 2D LED matrix
/// @example NoisePlusPalette.ino

// printf
#include <stdio.h>
#include <string>
#include <vector>

#include <FastLED.h>
#include "third_party/arduinojson/json.h"
#include "slice.h"
#include "screenmap.h"
#include "math_macros.h"



#define LED_PIN 3
#define BRIGHTNESS 96
#define COLOR_ORDER GRB
#define NUM_LEDS 2

CRGB leds[NUM_LEDS];
float to_rads(float degs) { return degs * PI / 180.0; }

void make_map(float angle, float step, int num, std::vector<pair_xy16>* _map) {
    float cos_angle = cos(angle);
    float sin_angle = sin(angle);
    std::vector<pair_xy16>& map = *_map;
    for (int i = 0; i < num; i++) {
        float radius = i * step;
        int16_t x = static_cast<int16_t>(radius * cos_angle);
        int16_t y = static_cast<int16_t>(radius * sin_angle);
        map.push_back(pair_xy16{x, y});
    }
}

void setup() {
    for (CRGB& c : leds) {
        c = CRGB::White;
    }
    FastLED.setBrightness(255);
    std::vector<pair_xy16> map;
    make_map(to_rads(225), 100, NUM_LEDS, &map);
    ScreenMap screenmap = ScreenMap(map.data(), map.size());

    // print out screen map
    for (int i = 0; i < map.size(); i++) {
        const pair_xy16& p = screenmap[i];
        printf("x: %d, y: %d\n", p.x, p.y);
    }

    // print out 

    FastLED.addLeds<WS2811, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)
        .setCanvasUi(screenmap);
}

void loop() {
    // FastLED.show();
}


