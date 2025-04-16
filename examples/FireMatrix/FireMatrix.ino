// Perlin noise fire procedure
// 16x16 rgb led matrix demo
// Yaroslaw Turbin, 22.06.2020
// https://vk.com/ldirko
// https://www.reddit.com/user/ldirko/
// https://www.reddit.com/r/FastLED/comments/hgu16i/my_fire_effect_implementation_based_on_perlin/

// idea in make perlin noise with time offset X and Z coord
// this automatic scroll fire pattern
// and distort fire noise.
// then substract Y based coodrd value to shift
// fire color (not brightness) in palette.
// this fadeout color from bottom matrix to up.
// this need some palette tweak for good looking fire color

#include "FastLED.h"
#include "fl/xymap.h"
#include "fl/ui.h"
#include "fx/time.h"

using namespace fl;

#define HEIGHT 100
#define WIDTH 100
#define SERPENTINE true
#define BRIGHTNESS 255

TimeScale timeScale(0, 1.0f);

UISlider scaleXY("Scale", 20, 1, 100, 1);
UISlider speedY("SpeedY", 1, 1, 6, 1);
UISlider invSpeedZ("Inverse SpeedZ", 20, 1, 100, 1);
UISlider brightness("Brightness", 255, 0, 255, 1);

CRGB leds[HEIGHT * WIDTH];

DEFINE_GRADIENT_PALETTE(firepal) {
    // define fire palette
    0,   0,   0,   0,  // black
    32,  255, 0,   0,  // red
    190, 255, 255, 0,  // yellow
    255, 255, 255, 255 // white
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

void loop() {
    FastLED.setBrightness(brightness);
    CRGBPalette16 myPal = firepal;
    uint32_t now = millis();
    uint32_t y_speed = (now * speedY.as<uint32_t>());
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
