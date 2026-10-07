/// @file    noisepalette.h
/// @brief   Demonstrates how to mix noise generation with color palettes on a
/// 2D LED matrix

#pragma once

#include "fl/stl/stdint.h"

#include "fl/stl/shared_ptr.h"         // For FASTLED_SHARED_PTR macros
#include "fl/stl/shared_ptr.h"  // For shared_ptr
#include "fl/math/xymap.h"
#include "fl/fx/fx2d.h"
#include "fl/math/random8.h"
#include "fl/stl/noexcept.h"

namespace fl {

FASTLED_SHARED_PTR(NoisePalette);

class NoisePalette : public Fx2d {
  public:
    // Fps is used by the fx_engine to maintain a fixed frame rate, ignored
    // otherwise.
    NoisePalette(XYMap xyMap, float fps = 60.f) FL_NO_EXCEPT;

    bool hasFixedFrameRate(float *fps) const FL_NO_EXCEPT override {
        *fps = mFps;
        return true;
    }

    // No need for a destructor, scoped_ptr will handle memory deallocation

    void draw(DrawContext context) FL_NO_EXCEPT override {
        fillnoise8();
        mapNoiseToLEDsUsingPalette(context.leds);
    }

    string fxName() const FL_NO_EXCEPT override { return "NoisePalette"; }
    void mapNoiseToLEDsUsingPalette(fl::span<CRGB> leds) FL_NO_EXCEPT;

    u8 changeToRandomPalette() FL_NO_EXCEPT;

    // There are 12 palette indexes but they don't have names. Use this to set
    // which one you want.
    u8 getPalettePresetCount() const FL_NO_EXCEPT { return 12; }
    u8 getPalettePreset() const FL_NO_EXCEPT { return currentPaletteIndex; }
    void setPalettePreset(int paletteIndex) FL_NO_EXCEPT;
    void setPalette(const CRGBPalette16 &palette, u16 speed,
                    u16 scale, bool colorLoop) FL_NO_EXCEPT {
        currentPalette = palette;
        this->speed = speed;
        this->scale = scale;
        this->colorLoop = colorLoop;
    }
    void setSpeed(u16 speed) FL_NO_EXCEPT { this->speed = speed; }
    void setScale(u16 scale) FL_NO_EXCEPT { this->scale = scale; }

  private:
    u16 mX, mY, mZ;
    u16 width, height;
    u16 speed = 0;
    u16 scale = 0;
    fl::vector_psram<u8> noise;
    CRGBPalette16 currentPalette;
    bool colorLoop = 0;
    int currentPaletteIndex = 0;
    float mFps = 60.f;

    void fillnoise8() FL_NO_EXCEPT;

    u16 XY(u8 x, u8 y) const FL_NO_EXCEPT { return mXyMap.mapToIndex(x, y); }

    void SetupRandomPalette() FL_NO_EXCEPT {
        CRGBPalette16 newPalette;
        do {
            newPalette = CRGBPalette16(
                CHSV(random8(), 255, 32), CHSV(random8(), 255, 255),
                CHSV(random8(), 128, 255), CHSV(random8(), 255, 255));
        } while (newPalette == currentPalette);
        currentPalette = newPalette;
    }

    void SetupBlackAndWhiteStripedPalette() FL_NO_EXCEPT {
        fill_solid(currentPalette, 16, CRGB::Black);
        currentPalette[0] = CRGB::White;
        currentPalette[4] = CRGB::White;
        currentPalette[8] = CRGB::White;
        currentPalette[12] = CRGB::White;
    }

    void SetupPurpleAndGreenPalette() FL_NO_EXCEPT {
        CRGB purple = CHSV(HUE_PURPLE, 255, 255);
        CRGB green = CHSV(HUE_GREEN, 255, 255);
        CRGB black = CRGB::Black;

        currentPalette = CRGBPalette16(
            green, green, black, black, purple, purple, black, black, green,
            green, black, black, purple, purple, black, black);
    }
};

} // namespace fl
