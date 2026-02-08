/// @file noisegen.h
/// Noise generation classes

#pragma once

#include "fl/stl/stdint.h"
#include "noise.h"

#include "fl/math_macros.h"
// Simple noise generator for 1-d waves. Default values will give good results
// for most cases.
struct NoiseGenerator {
    fl::i32 iteration_scale;
    unsigned long time_multiplier;

    NoiseGenerator() : iteration_scale(10), time_multiplier(10) {}
    NoiseGenerator(fl::i32 itScale, fl::i32 timeMul) : iteration_scale(itScale), time_multiplier(timeMul) {}

    fl::u8 Value(fl::i32 i, unsigned long time_ms) const {
        fl::u32 input = iteration_scale * i + time_ms * time_multiplier;
        fl::u16 v1 = inoise16(input);
        return fl::u8(v1 >> 8);
    }

    int LedValue(fl::i32 i, unsigned long time_ms) const {
        int val = Value(i, time_ms);
        return FL_MAX(0, val - 128) * 2;
    }
};

