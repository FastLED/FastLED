

/*
This demo is best viewed using the FastLED compiler.
Install: pip install fastled
Run: fastled <this sketch directory>
This will compile and preview the sketch in the browser, and enable
all the UI elements you see below.
*/

// Perlin noise fire procedure
// 16x16 rgb led matrix demo
// Yaroslaw Turbin, 22.06.2020
// https://vk.com/ldirko
// https://www.reddit.com/user/ldirko/
// https://www.reddit.com/r/FastLED/comments/hgu16i/my_fire_effect_implementation_based_on_perlin/
// Based on the code found at: https://editor.soulmatelights.com/gallery/1229-

// idea in make perlin noise with time offset X and Z coord
// this automatic scroll fire pattern
// and distort fire noise.
// then substract Y based coodrd value to shift
// fire color (not brightness) in palette.
// this fadeout color from bottom matrix to up.
// this need some palette tweak for good looking fire color

#include "FastLED.h"
#include "fl/ui.h"
#include "fl/xymap.h"
#include "fx/time.h"

using namespace fl;

#define HEIGHT 100
#define WIDTH 100
#define SERPENTINE true
#define BRIGHTNESS 255

TimeWarp timeScale(0, 1.0f);

UISlider scaleXY("Scale", 20, 1, 100, 1);
UISlider speedY("SpeedY", 1, 1, 6, .1);
UISlider invSpeedZ("Inverse SpeedZ", 20, 1, 100, 1);
UISlider brightness("Brightness", 255, 0, 255, 1);
UINumberField palette("Palette", 0, 0, 2);

CRGB leds[HEIGHT * WIDTH];

DEFINE_GRADIENT_PALETTE(firepal){
    // define fire palette
    0,   0,   0,   0,  // black
    32,  255, 0,   0,  // red
    190, 255, 255, 0,  // yellow
    255, 255, 255, 255 // white
};

DEFINE_GRADIENT_PALETTE(electricGreenFirePal){
    0,   0,   0,   0,  // black
    32,  0,   70,  0,  // dark green
    190, 57,  255, 20, // electric neon green
    255, 255, 255, 255 // white
};

DEFINE_GRADIENT_PALETTE(electricBlueFirePal) {
    0,   0,   0,   0,    // Black
    32,  0,   0,  70,    // Dark blue
    128, 20, 57, 255,    // Electric blue
    255, 255, 255, 255   // White
};

XYMap xyMap(HEIGHT, WIDTH, SERPENTINE);

void setup() {
    Serial.begin(115200);
    FastLED.addLeds<NEOPIXEL, 3>(leds, HEIGHT * WIDTH).setScreenMap(xyMap);
    FastLED.setCorrection(TypicalLEDStrip);
}

uint8_t getPaletteIndex(uint32_t millis32, int i, int j, uint32_t y_speed) {
    // get palette index
    uint16_t scale = scaleXY.as<uint16_t>();
    uint16_t x = i * scale;
    uint32_t y = j * scale + y_speed;
    uint16_t z = millis32 / invSpeedZ.as<uint16_t>();
    uint16_t noise16 = inoise16(x << 8, y << 8, z << 8);
    uint8_t noise_val = noise16 >> 8;
    int8_t subtraction_factor = abs8(j - (WIDTH - 1)) * 255 / (WIDTH - 1);
    return qsub8(noise_val, subtraction_factor);
}

CRGBPalette16 getPalette() {
    // get palette
    switch (palette) {
    case 0:
        return firepal;
    case 1:
        return electricGreenFirePal;
    case 2:
        return electricBlueFirePal;
    default:
        return firepal;
    }
}

void loop() {
    FastLED.setBrightness(brightness);
    CRGBPalette16 myPal = getPalette();
    uint32_t now = millis();
    timeScale.setSpeed(speedY);
    uint32_t y_speed = timeScale.update(now);
    for (int i = 0; i < HEIGHT; i++) {
        for (int j = 0; j < WIDTH; j++) {
            uint8_t palette_index = getPaletteIndex(now, i, j, y_speed);
            CRGB c = ColorFromPalette(myPal, palette_index, BRIGHTNESS);
            int index = xyMap((HEIGHT - 1) - i, (WIDTH - 1) - j);
            leds[index] = c;
        }
    }
    FastLED.show();
}
