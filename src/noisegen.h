#pragma once

#include <stdint.h>
#include "noise.h"

#include "fl_math.h"
#include "namespace.h"

FASTLED_NAMESPACE_BEGIN


struct NoiseGenerator {
    int32_t iteration_scale;
    unsigned long time_multiplier;

    NoiseGenerator() : iteration_scale(10), time_multiplier(10) {}
    NoiseGenerator(int32_t itScale, int32_t timeMul) : iteration_scale(itScale), time_multiplier(timeMul) {}

    uint8_t Value(int32_t i, unsigned long time_ms) const {
        uint32_t input = iteration_scale * i + time_ms * time_multiplier;
        uint16_t v1 = inoise16(input);
        return uint8_t(v1 >> 8);
    }

    int LedValue(int32_t i, unsigned long time_ms) const {
        int val = Value(i, time_ms);
        return fl_math::max(0, val - 128) * 2;
    }
};

FASTLED_NAMESPACE_END
