

#pragma once

#include "fx/2d/blend.h"
#include "fx/2d/wave.h"
#include "fx/fx2d.h"
#include "fl/raster.h"

using namespace fl;

struct WaveEffect {
    WaveFxPtr wave_fx_low;
    WaveFxPtr wave_fx_high;
    Blend2dPtr blend_stack;
    void draw(Fx::DrawContext context) { blend_stack->draw(context); }
    void addf(size_t x, size_t y, float value) {
        wave_fx_low->addf(x, y, value);
        wave_fx_high->addf(x, y, value);
    }
};

struct DrawRasterToWaveSimulator {
    DrawRasterToWaveSimulator(WaveEffect* wave_fx) : mWaveFx(wave_fx) {}
    void draw(const vec2<uint16_t> &pt, uint32_t /*index*/, uint8_t value) {
        float valuef = value / 255.0f;
        size_t xx = pt.x;
        size_t yy = pt.y;
        mWaveFx->addf(xx, yy, valuef);
    }
    WaveEffect* mWaveFx;
};

WaveEffect NewWaveSimulation2D(const XYMap& xymap);
