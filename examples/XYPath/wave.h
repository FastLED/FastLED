

#pragma once

#include "fx/2d/blend.h"
#include "fx/2d/wave.h"
#include "fx/fx2d.h"
#include "fl/raster.h"

using namespace fl;

struct WaveEffect {
    WaveFxPtr wave_fx_low;
    ;
    WaveFxPtr wave_fx_high;
    Blend2dPtr blend_stack;

    void draw(Fx::DrawContext context) { blend_stack->draw(context); }

    void addf(size_t x, size_t y, float value) {
        wave_fx_low->addf(x, y, value);
        wave_fx_high->addf(x, y, value);
    }
};

struct DrawRasterToWaveSimulator {
    DrawRasterToWaveSimulator(Raster *raster, WaveEffect* wave_fx) : mRaster(raster), mWaveFx(wave_fx) {}
    void draw(const point_xy<int> &pt, uint32_t index, uint8_t value) {
        float valuef = value / 255.0f;
        int xx = pt.x;
        int yy = pt.y;
        mWaveFx->addf(xx, yy, valuef);
    }

    Raster *mRaster;
    WaveEffect* mWaveFx;
};

WaveEffect NewWaveSimulation2D(const XYMap xymap);
