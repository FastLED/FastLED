


#include "util.h"

#include "FastLED.h"


UISlider scaleXY("Scale", 8, 1, 100, 1);
UISlider scaleX("ScaleX", .3, 0.1, 3, .01);
UISlider invSpeedZ("Inverse SpeedZ", 20, 1, 100, 1);



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
    return noise_val;
}