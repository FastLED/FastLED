/// @file    NoisePlusPalette.hpp
/// @brief   Demonstrates how to mix noise generation with color palettes on a
/// 2D LED matrix
/// @example NoisePlusPalette.hpp

#pragma once

#include <stdint.h>

#include "FastLED.h"
#include "fx/fx2d.h"
#include "lib8tion/random8.h"
#include "noise.h"
#include "ref.h"
#include "xymap.h"

FASTLED_NAMESPACE_BEGIN

FASTLED_SMART_REF(NoisePalette);

class NoisePalette : public FxGrid {
  public:
    NoisePalette(XYMap xyMap)
        : FxGrid(xyMap), speed(0), scale(0), colorLoop(1) {
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

    // No need for a destructor, scoped_ptr will handle memory deallocation

    void lazyInit() override {}

    void draw(DrawContext context) override {
        fillnoise8();
        mapNoiseToLEDsUsingPalette(context.leds);
    }

    const char *fxName(int which) const override { return "NoisePalette"; }

    void mapNoiseToLEDsUsingPalette(CRGB *leds);

    uint8_t changeToRandomPalette();

    // There are 12 palette indexes but they don't have names. Use this to set
    // which one you want.
    uint8_t getPalettePresetCount() const { return 12; }
    uint8_t getPalettePreset() const { return currentPaletteIndex; }
    void setPalettePreset(int paletteIndex);
    void setPalette(const CRGBPalette16 &palette, uint16_t speed,
                    uint16_t scale, bool colorLoop) {
        currentPalette = palette;
        this->speed = speed;
        this->scale = scale;
        this->colorLoop = colorLoop;
    }
    void setSpeed(uint16_t speed) { this->speed = speed; }
    void setScale(uint16_t scale) { this->scale = scale; }

  private:
    uint16_t mX, mY, mZ;
    uint16_t width, height;
    uint16_t speed = 0;
    uint16_t scale = 0;
    scoped_ptr<uint8_t> noise;
    CRGBPalette16 currentPalette = PartyColors_p;
    bool colorLoop = 0;
    int currentPaletteIndex = 0;

    void fillnoise8();



    uint16_t XY(uint8_t x, uint8_t y) const { return mXyMap.mapToIndex(x, y); }

    void SetupRandomPalette() {
        CRGBPalette16 newPalette;
        do {
            newPalette = CRGBPalette16(
                CHSV(random8(), 255, 32), CHSV(random8(), 255, 255),
                CHSV(random8(), 128, 255), CHSV(random8(), 255, 255));
        } while (newPalette == currentPalette);
        currentPalette = newPalette;
    }

    void SetupBlackAndWhiteStripedPalette() {
        fill_solid(currentPalette, 16, CRGB::Black);
        currentPalette[0] = CRGB::White;
        currentPalette[4] = CRGB::White;
        currentPalette[8] = CRGB::White;
        currentPalette[12] = CRGB::White;
    }

    void SetupPurpleAndGreenPalette() {
        CRGB purple = CHSV(HUE_PURPLE, 255, 255);
        CRGB green = CHSV(HUE_GREEN, 255, 255);
        CRGB black = CRGB::Black;

        currentPalette = CRGBPalette16(
            green, green, black, black, purple, purple, black, black, green,
            green, black, black, purple, purple, black, black);
    }
};

inline void NoisePalette::setPalettePreset(int paletteIndex) {
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
    }
}

inline void NoisePalette::mapNoiseToLEDsUsingPalette(CRGB *leds) {
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

inline void NoisePalette::fillnoise8() {
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
        for (int j = 0; j < height; j++) {
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

inline uint8_t NoisePalette::changeToRandomPalette() {
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

FASTLED_NAMESPACE_END
