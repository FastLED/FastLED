
#include <math.h>

#include "fl/lut.h"
#include "fl/math_macros.h"
#include "fl/xypaths.h"

namespace fl {

XYPath::XYPath(uint16_t steps) : mSteps(steps) {}

LUTXY16Ptr XYPath::generateLUT(uint16_t steps) {
    LUTXY16Ptr lut = LUTXY16Ptr::New(steps);
    pair_xy<uint16_t> *mutable_data = lut->getData();
    float stepsf = (float)steps;
    for (uint16_t i = 0; i < steps; i++) {
        float alpha = static_cast<float>(i) / stepsf;
        pair_xy<float> xy = at(alpha);
        mutable_data[i].x = (uint16_t)(xy.x * 65535.0f);
        mutable_data[i].y = (uint16_t)(xy.y * 65535.0f);
    }
    return lut;
}

void XYPath::initLutOnce() {
    if (mLut) {
        return;
    }
    mLut = generateLUT(mSteps);
}

pair_xy<uint16_t> XYPath::at16(uint16_t alpha, uint16_t scale) {
    if (mSteps > 0) {
        initLutOnce();
        if (mLut) {
            return mLut->getData()[alpha];
        } else {
            FASTLED_WARN("XYPath::at16: mLut is null");
        }
    }
    // Fallback to the default implementation. Fine most paths.
    float scalef = static_cast<float>(scale);
    float alpha_f = static_cast<float>(alpha) / scalef;
    pair_xy<float> xy = at(alpha_f);
    return {static_cast<uint16_t>(xy.x * scalef),
            static_cast<uint16_t>(xy.y * scalef)};
}

pair_xy<float> HeartPath::at(float alpha) {
    // 1) raw parametric heart
    // constexpr float PI = 3.14159265358979323846f;
    float t = alpha * 2.0f * PI;
    float s = sinf(t);
    float c = cosf(t);
    float xo = c * (1.0f - s);
    float yo = s * (1.0f - s);

    // 2) bounding box of (xo,yo) over t∈[0,2π]
    //    minx ≈ −1.299038, maxx ≈ +1.299038
    //    miny = −2.0,      maxy ≈ +0.25
    constexpr float minx = -1.29903805f, maxx = 1.29903805f;
    constexpr float miny = -2.0f, maxy = 0.25f;

    // 3) remap into [0,1]
    float x = (xo - minx) / (maxx - minx);
    float y = (yo - miny) / (maxy - miny);

    return {x, y};
}

} // namespace fl