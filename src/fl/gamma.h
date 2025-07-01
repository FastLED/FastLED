#pragma once

#include "crgb.h"
#include "fl/stdint.h"
#include "fl/int.h"
#include "fl/ease.h"

namespace fl {

// Forward declaration - gamma_2_8 is now defined in fl/ease.h
extern const u16 gamma_2_8[256];

inline void gamma16(const CRGB &rgb, u16* r16, u16* g16, u16* b16) {
    *r16 = gamma_2_8[rgb.r];
    *g16 = gamma_2_8[rgb.g];
    *b16 = gamma_2_8[rgb.b];
}

} // namespace fl
