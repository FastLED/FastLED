// AnimartrixRing: Sample a circle from an Animartrix rectangular grid
// This example generates a rectangular animation grid and samples a circular
// region from it to display on a ring of LEDs.

#include <FastLED.h>
#include "fl/math_macros.h"
#include "fl/screenmap.h"

#ifndef TWO_PI
#define TWO_PI 6.2831853071795864769252867665590057683943387987502116419498891846156328125724179972560696506842341359
#endif

#define NUM_LEDS 60
#define DATA_PIN 3
#define BRIGHTNESS 128

// Grid dimensions for Animartrix sampling
#define GRID_WIDTH 32
#define GRID_HEIGHT 32

CRGB leds[NUM_LEDS];
CRGB grid[GRID_WIDTH * GRID_HEIGHT];

// Animartrix parameters
XYMap xymap = XYMap::constructRectangularGrid(GRID_WIDTH, GRID_HEIGHT);

// ScreenMap for the ring - defines circular sampling positions using a lambda
fl::ScreenMap screenmap = fl::ScreenMap(NUM_LEDS, 0.5f, [](int index, fl::vec2f& pt_out) {
    float centerX = GRID_WIDTH / 2.0f;
    float centerY = GRID_HEIGHT / 2.0f;
    float radius = min(GRID_WIDTH, GRID_HEIGHT) / 2.0f - 1;
    float angle = (TWO_PI * index) / NUM_LEDS;
    pt_out.x = centerX + cos(angle) * radius;
    pt_out.y = centerY + sin(angle) * radius;
});

void setup() {
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS).setScreenMap(screenmap);
    FastLED.setBrightness(BRIGHTNESS);
}

void loop() {
    static uint16_t frame = 0;

    // Generate Animartrix pattern on rectangular grid
    uint16_t ms = millis();
    for (uint8_t y = 0; y < GRID_HEIGHT; y++) {
        for (uint8_t x = 0; x < GRID_WIDTH; x++) {
            uint16_t index = xymap.mapToIndex(x, y);

            // Create a moving rainbow plasma effect
            uint8_t hue = (x * 8) + (y * 8) + (ms / 20);
            uint8_t sat = 255 - ((x * 4) ^ (y * 4));
            uint8_t val = sin8((x * 16) + (ms / 30)) / 2 +
                          sin8((y * 16) + (ms / 40)) / 2;

            grid[index] = CHSV(hue, sat, val);
        }
    }

    // Sample circle from grid using screenmap
    for (uint8_t i = 0; i < NUM_LEDS; i++) {
        fl::vec2f pos = screenmap[i];
        float x = pos.x;
        float y = pos.y;

        // Bilinear interpolation for smooth sampling
        int x0 = (int)x;
        int y0 = (int)y;
        int x1 = min(x0 + 1, GRID_WIDTH - 1);
        int y1 = min(y0 + 1, GRID_HEIGHT - 1);

        float fx = x - x0;
        float fy = y - y0;

        // Get four neighboring pixels
        CRGB c00 = grid[xymap.mapToIndex(x0, y0)];
        CRGB c10 = grid[xymap.mapToIndex(x1, y0)];
        CRGB c01 = grid[xymap.mapToIndex(x0, y1)];
        CRGB c11 = grid[xymap.mapToIndex(x1, y1)];

        // Interpolate
        CRGB c0 = blend(c00, c10, fx * 255);
        CRGB c1 = blend(c01, c11, fx * 255);
        leds[i] = blend(c0, c1, fy * 255);
    }

    FastLED.show();
    frame++;
}
