


#pragma once

#include "fx/fx2d.h"
#include "fx/2d/wave.h"
#include "fx/2d/blend.h"

using namespace fl;

struct WaveEffect {
    WaveFxPtr wave_fx_low;;
    WaveFxPtr wave_fx_high;
    Blend2dPtr blend_stack;

    void draw(Fx::DrawContext context) {
        blend_stack->draw(context);
    }

    void addf(size_t x, size_t y, float value) {
        wave_fx_low->addf(x, y, value);
        wave_fx_high->addf(x, y, value);
    }
};


WaveEffect NewWaveSimulation2D(const XYMap xymap);

