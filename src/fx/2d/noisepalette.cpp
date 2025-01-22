


#include <stdint.h>

#define FASTLED_INTERNAL

#include "FastLED.h"
#include "fx/fx2d.h"
#include "lib8tion/random8.h"
#include "noise.h"
#include "fl/ptr.h"
#include "fl/xymap.h"

#include "noisepalette.h"

namespace fl {

NoisePalette::NoisePalette(XYMap xyMap, float fps)
    : Fx2d(xyMap), speed(0), scale(0), colorLoop(1), mFps(fps) {
    // currentPalette = PartyColors_p;
    static_assert(sizeof(currentPalette) == sizeof(CRGBPalette16),
                  "Palette size mismatch");
    currentPalette = PartyColors_p;
    width = xyMap.getWidth();
    height = xyMap.getHeight();

    // Initialize our coordinates to some random values
    mX = random16();
    mY = random16();
    mZ = random16();

    setPalettePreset(0);

    // Allocate memory for the noise array using scoped_ptr
    noise = scoped_ptr<uint8_t>(new uint8_t[width * height]);
}

void NoisePalette::setPalettePreset(int paletteIndex) {
    currentPaletteIndex = paletteIndex % 12; // Ensure the index wraps around
    switch (currentPaletteIndex) {
    case 0:
        currentPalette = RainbowColors_p;
        speed = 20;
        scale = 30;
        colorLoop = 1;
        break;
    case 1:
        SetupPurpleAndGreenPalette();
        speed = 10;
        scale = 50;
        colorLoop = 1;
        break;
    case 2:
        SetupBlackAndWhiteStripedPalette();
        speed = 20;
        scale = 30;
        colorLoop = 1;
        break;
    case 3:
        currentPalette = ForestColors_p;
        speed = 8;
        scale = 120;
        colorLoop = 0;
        break;
    case 4:
        currentPalette = CloudColors_p;
        speed = 4;
        scale = 30;
        colorLoop = 0;
        break;
    case 5:
        currentPalette = LavaColors_p;
        speed = 8;
        scale = 50;
        colorLoop = 0;
        break;
    case 6:
        currentPalette = OceanColors_p;
        speed = 20;
        scale = 90;
        colorLoop = 0;
        break;
    case 7:
        currentPalette = PartyColors_p;
        speed = 20;
        scale = 30;
        colorLoop = 1;
        break;
    case 8:
    case 9:
    case 10:
        SetupRandomPalette();
        speed = 20 + (currentPaletteIndex - 8) * 5;
        scale = 20 + (currentPaletteIndex - 8) * 5;
        colorLoop = 1;
        break;
    case 11:
        currentPalette = RainbowStripeColors_p;
        speed = 2;
        scale = 20;
        colorLoop = 1;
        break;
    default:
        break;
    }
}

void NoisePalette::mapNoiseToLEDsUsingPalette(CRGB *leds) {
    static uint8_t ihue = 0;

    for (uint16_t i = 0; i < width; i++) {
        for (uint16_t j = 0; j < height; j++) {
            // We use the value at the (i,j) coordinate in the noise
            // array for our brightness, and the flipped value from (j,i)
            // for our pixel's index into the color palette.

            uint8_t index = noise.get()[i * height + j];
            uint8_t bri = noise.get()[j * width + i];

            // if this palette is a 'loop', add a slowly-changing base value
            if (colorLoop) {
                index += ihue;
            }

            // brighten up, as the color palette itself often contains the
            // light/dark dynamic range desired
            if (bri > 127) {
                bri = 255;
            } else {
                bri = dim8_raw(bri * 2);
            }

            CRGB color = ColorFromPalette(currentPalette, index, bri);
            leds[XY(i, j)] = color;
        }
    }

    ihue += 1;
}

void NoisePalette::fillnoise8() {
    // If we're running at a low "speed", some 8-bit artifacts become
    // visible from frame-to-frame.  In order to reduce this, we can do some
    // fast data-smoothing. The amount of data smoothing we're doing depends
    // on "speed".
    uint8_t dataSmoothing = 0;
    if (speed < 50) {
        dataSmoothing = 200 - (speed * 4);
    }

    for (uint16_t i = 0; i < width; i++) {
        int ioffset = scale * i;
        for (uint16_t j = 0; j < height; j++) {
            int joffset = scale * j;

            uint8_t data = inoise8(mX + ioffset, mY + joffset, mZ);

            // The range of the inoise8 function is roughly 16-238.
            // These two operations expand those values out to roughly
            // 0..255 You can comment them out if you want the raw noise
            // data.
            data = qsub8(data, 16);
            data = qadd8(data, scale8(data, 39));

            if (dataSmoothing) {
                uint8_t olddata = noise.get()[i * height + j];
                uint8_t newdata = scale8(olddata, dataSmoothing) +
                                  scale8(data, 256 - dataSmoothing);
                data = newdata;
            }

            noise.get()[i * height + j] = data;
        }
    }

    mZ += speed;

    // apply slow drift to X and Y, just for visual variation.
    mX += speed / 8;
    mY -= speed / 16;
}

uint8_t NoisePalette::changeToRandomPalette() {
    while (true) {
        uint8_t new_idx = random8() % 12;
        if (new_idx == currentPaletteIndex) {
            continue;
        }
        currentPaletteIndex = new_idx;
        setPalettePreset(currentPaletteIndex);
        return currentPaletteIndex;
    }
}





}  // namespace fl
