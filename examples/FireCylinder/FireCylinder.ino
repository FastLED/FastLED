
/*
This demo is best viewed using the FastLED compiler.
Install: pip install fastled
Run: fastled <this sketch directory>
This will compile and preview the sketch in the browser, and enable
all the UI elements you see below.
*/

// Perlin noise fire procedure
// 16x16 rgb led cylinder demo
// Exactly the same as the FireMatrix example, but with a cylinder, meaning that the x=0
// and x = len-1 are connected.
// This also showcases the inoise16(x,y,z,t) function which handles 3D+time noise effects.
// Keep in mind that a cylinder is embedded in a 3D space with time being used to add
// additional noise to the effect.

#include "FastLED.h"
#include "fl/ui.h"
#include "fl/xymap.h"
#include "fx/time.h"

using namespace fl;

#define HEIGHT 100
#define WIDTH 100
#define SERPENTINE true
#define BRIGHTNESS 255

UITitle title("FireCylinder Demo");
UIDescription description("This Fire demo wraps around the cylinder. It uses Perlin noise to create a fire effect.");


TimeWarp timeScale(0, 1.0f);

UISlider scaleXY("Scale", 8, 1, 100, 1);
UISlider speedY("SpeedY", 1.3, 1, 6, .1);
UISlider scaleX("ScaleX", .3, 0.1, 3, .01);
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

DEFINE_GRADIENT_PALETTE(electricBlueFirePal){
    0,   0,   0,   0,   // Black
    32,  0,   0,   70,  // Dark blue
    128, 20,  57,  255, // Electric blue
    255, 255, 255, 255  // White
};

XYMap xyMap(HEIGHT, WIDTH, SERPENTINE);

void setup() {
    Serial.begin(115200);
    FastLED.addLeds<NEOPIXEL, 3>(leds, HEIGHT * WIDTH).setScreenMap(xyMap);
    FastLED.setCorrection(TypicalLEDStrip);
}

uint8_t getPaletteIndex(uint32_t millis32, int width, int max_width, int height, int max_height,
                        uint32_t y_speed) {
    // get palette index
    uint16_t scale = scaleXY.as<uint16_t>();
    float xf = (float)width / (float)max_width;
    uint8_t x = (uint8_t)(xf * 255);
    uint32_t cosx = cos8(x);
    uint32_t sinx = sin8(x);
    float trig_scale = scale * scaleX.value();
    cosx *= trig_scale;
    sinx *= trig_scale;
    uint32_t y = height * scale + y_speed; // swapped: now using height
    uint16_t z = millis32 / invSpeedZ.as<uint16_t>();

    uint16_t noise16 = inoise16(cosx << 8, sinx << 8, y << 8, 0);
    uint8_t noise_val = noise16 >> 8;
    int8_t subtraction_factor = abs8(height - (max_height - 1)) * 255 /
                                (max_height - 1); // swapped: now using height
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
    for (int width = 0; width < WIDTH; width++) {
        for (int height = 0; height < HEIGHT; height++) {
            uint8_t palette_index =
                getPaletteIndex(now, width, WIDTH, height, HEIGHT, y_speed);
            CRGB c = ColorFromPalette(myPal, palette_index, BRIGHTNESS);
            int index = xyMap((WIDTH - 1) - width, (HEIGHT - 1) - height);
            leds[index] = c;
        }
    }
    FastLED.show();
}
