
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

pair_xy<uint16_t> XYPath::at16(uint16_t alpha, uint16_t scale,
                               uint16_t x_translate_left, uint16_t y_translate_down) {
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
    return {static_cast<uint16_t>(xy.x * scalef) - x_translate_left,
            static_cast<uint16_t>(xy.y * scalef) - y_translate_down};
}

void XYPath::buildLut(uint16_t steps) {
    mLut.reset();
    mSteps = steps;
    if (steps > 0) {
        mLut = generateLUT(steps);
    }
}

LinePath::LinePath(float x0, float y0, float x1, float y1, uint16_t steps)
    : XYPath(steps), mX0(x0), mY0(y0), mX1(x1), mY1(y1) {}

pair_xy<float> LinePath::at(float alpha) {
    // α in [0,1] → (x,y) on the line
    float x = mX0 + alpha * (mX1 - mX0);
    float y = mY0 + alpha * (mY1 - mY0);
    return {x, y};
}

pair_xy<float> CirclePath::at(float alpha) {
    // α in [0,1] → (x,y) on the circle
    float t = alpha * 2.0f * PI;
    float x = .5f * cosf(t);
    float y = .5f * sinf(t);
    return {0.5f + 0.5f * x, 0.5f + 0.5f * y};
}

CirclePath::CirclePath(uint16_t steps) : XYPath(steps) {}

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

GielisCurvePath::GielisCurvePath(uint8_t m, float a, float b, float n1,
                                 float n2, float n3, uint16_t steps)
    : XYPath(steps), mM(m), mA(a), mB(b), mN1(n1), mN2(n2), mN3(n3) {}

pair_xy<float> GielisCurvePath::at(float alpha) {
    // α ∈ [0,1] → θ ∈ [0, 2π]
    float t = alpha * 2.0f * PI;

    // Superformula radial term
    float p1 = powf(fabsf(cosf(mM * t / 4.0f) / mA), mN2);
    float p2 = powf(fabsf(sinf(mM * t / 4.0f) / mB), mN3);
    float denom = powf(p1 + p2, 1.0f / mN1);
    float r = 1.0f / denom;

    // Polar → Cartesian
    float x0 = r * cosf(t);
    float y0 = r * sinf(t);

    // Normalize from [-1..1] → [0..1]
    float x = 0.5f + 0.5f * x0;
    float y = 0.5f + 0.5f * y0;

    return {x, y};
}

void XYPath::clearLut() { mLut.reset(); }

PhyllotaxisPath::PhyllotaxisPath(uint16_t count, float angle, uint16_t steps)
    : XYPath(steps), mCount(count), mAngle(angle) {}

pair_xy<float> PhyllotaxisPath::at(float alpha) {
    // Map α∈[0,1] → n∈[0,count−1]
    float n = alpha * float(mCount > 1 ? mCount - 1 : 1);
    // Polar coords
    float theta = n * mAngle;
    float r = sqrtf(n / (mCount > 1 ? float(mCount - 1) : 1.0f));
    // Cartesian & normalize to [0,1]²
    float x0 = r * cosf(theta);
    float y0 = r * sinf(theta);
    return {0.5f + 0.5f * x0, 0.5f + 0.5f * y0};
}

} // namespace fl