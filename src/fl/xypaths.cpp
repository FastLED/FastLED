
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

void XYPath::buildLut(uint16_t steps) {
    mLut.reset();
    mSteps = steps;
    if (steps > 0) {
        mLut = generateLUT(steps);
    }
}

HeartPath::HeartPath(uint16_t steps) : XYPath(steps) {}

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

pair_xy<float> LissajousPath::at(float alpha) {
    // t in [0,2π]
    float t = alpha * 2.0f * PI;
    float x = 0.5f + 0.5f * sinf(mA * t + mDelta);
    float y = 0.5f + 0.5f * sinf(mB * t);
    return {x, y};
}

LissajousPath::LissajousPath(uint8_t a, uint8_t b, float delta, uint16_t steps)
    : XYPath(steps), mA(a), mB(b), mDelta(delta) {}

ArchimedeanSpiralPath::ArchimedeanSpiralPath(uint8_t turns, float radius,
                                             uint16_t steps)
    : XYPath(steps), mTurns(turns), mRadius(radius) {}

pair_xy<float> ArchimedeanSpiralPath::at(float alpha) {
    // α ∈ [0,1] → θ ∈ [0, 2π·turns]
    float t = alpha * 2.0f * PI * mTurns;
    // r grows linearly from 0 to mRadius as α goes 0→1
    float r = mRadius * alpha;
    // convert polar → cartesian, then shift center to (0.5,0.5)
    float x = 0.5f + r * cosf(t);
    float y = 0.5f + r * sinf(t);
    return {x, y};
}

RosePath::RosePath(uint8_t petals, uint16_t steps)
    : XYPath(steps), mPetals(petals) {}

pair_xy<float> RosePath::at(float alpha) {
    // α ∈ [0,1] → θ ∈ [0, 2π]
    float t = alpha * 2.0f * PI;
    // polar radius
    float r = sinf(mPetals * t);
    // polar → cartesian
    float x0 = r * cosf(t);
    float y0 = r * sinf(t);
    // remap from [-1,1] to [0,1]
    float x = 0.5f + 0.5f * x0;
    float y = 0.5f + 0.5f * y0;
    return {x, y};
}

} // namespace fl