
#include <math.h>

#include "fl/assert.h"
#include "fl/lut.h"
#include "fl/math_macros.h"
#include "fl/xypaths.h"
#include "lib8tion/trig8.h"
#include "lib8tion/intmap.h"

namespace fl {

pair_xy_float TransformFloat::transform(const pair_xy_float &xy) const {
    float x = xy.x * scale + x_offset;
    float y = xy.y * scale + y_offset;
#pragma GCC diagnostic ignored "-Wfloat-equal"
    const bool has_rotation = (rotation != 0.0f);
#pragma GCC diagnostic pop
    if (has_rotation) {
        float cos_theta = cosf(rotation);
        float sin_theta = sinf(rotation);
        float x_rotated = x * cos_theta - y * sin_theta;
        float y_rotated = x * sin_theta + y * cos_theta;
        return pair_xy_float(x_rotated, y_rotated);
    }
    return pair_xy_float(x, y);
}

pair_xy<uint16_t> Transform16::transform(const pair_xy<uint16_t> &xy) const {
    pair_xy<uint16_t> out = xy;
    if (scale != 0xffff) {
        uint32_t x = out.x;
        uint32_t y = out.y;
        x *= scale;
        y *= scale;
        x = map32_to_16(x);
        y = map32_to_16(y);
        // x = (x * scale) >> 16;
        // y = (y * scale) >> 16;
        out.x = static_cast<uint16_t>(x);
        out.y = static_cast<uint16_t>(y);
    }
    if (x_offset != 0) {
        out.x += x_offset;
    }
    if (y_offset != 0) {
        out.y += y_offset;
    }
    if (rotation != 0) {
        // promote to signed so the muls don’t overflow
        int32_t x = static_cast<int32_t>(out.x);
        int32_t y = static_cast<int32_t>(out.y);

        // Q15 cosine & sine of 0…65535 angle
        int32_t c = cos16(rotation); // [-32768..+32767]
        int32_t s = sin16(rotation);

        // rotate:  x' = x*c - y*s ;  y' = x*s + y*c
        // >>15 to remove the Q15 factor
        int32_t xr = ((x * c) - (y * s)) >> 15;
        int32_t yr = ((x * s) + (y * c)) >> 15;

        out.x = static_cast<uint16_t>(xr);
        out.y = static_cast<uint16_t>(yr);
    }
    return out;
}

XYPath::XYPath(uint16_t steps) : mSteps(steps) {}
LUTXY16Ptr XYPath::generateLUT(uint16_t steps) {
    LUTXY16Ptr lut = LUTXY16Ptr::New(steps);
    pair_xy<uint16_t>* mutable_data = lut->getData();
    if (steps == 0) return lut;

    // use (steps‑1) as the denominator so that i=steps‑1 → alpha=1.0
    float denom = (steps > 1)
        ? static_cast<float>(steps - 1)
        : 1.0f;              // avoid divide-by-zero when steps==1

    for (uint16_t i = 0; i < steps; ++i) {
        float alpha = static_cast<float>(i) / denom;  // now last α == 1.0
        pair_xy_float xy = at(alpha);
        uint16_t x = static_cast<uint16_t>(xy.x * 65535.0f);
        uint16_t y = static_cast<uint16_t>(xy.y * 65535.0f);
        mutable_data[i] = { x, y };
    }
    return lut;
}

void XYPath::initLutOnce() {
    if (mLut) {
        return;
    }
    if (mSteps == 0) {
        return;
    }
    mLut = generateLUT(mSteps);
}

pair_xy_float XYPath::at(float alpha, const TransformFloat &tx) {
    pair_xy_float xy = at(alpha);
    return tx.transform(xy);
}

pair_xy<uint16_t> XYPath::at16(uint16_t alpha, const Transform16 &tx) {
    if (mSteps > 0) {
        initLutOnce();
        if (mLut) {
            pair_xy<uint16_t> out = mLut->interp16(alpha);
            out = tx.transform(out);
            return out;
        }
    }
    // Fallback to the default float implementation. Fine most paths.
    float scalef = static_cast<float>(tx.scale);
    float alpha_f = static_cast<float>(alpha) / 65535.0f;
    pair_xy_float xy = at(alpha_f);
    // Ensure values are clamped to the target range
    uint16_t x_val =
        MIN(static_cast<uint16_t>(xy.x * scalef) + tx.x_offset, tx.scale);
    uint16_t y_val =
        MIN(static_cast<uint16_t>(xy.y * scalef) + tx.y_offset, tx.scale);
    return pair_xy<uint16_t>(x_val, y_val);
}

void XYPath::buildLut(uint16_t steps) {
    mLut.reset();
    mSteps = steps;
    if (steps > 0) {
        mLut = generateLUT(steps);
    }
}

void XYPath::output(float alpha_start, float alpha_end, pair_xy_float *out,
                    uint16_t out_size, const TransformFloat &tx) {
    if (out_size == 0) {
        return;
    }
    if (out_size == 1) {
        pair_xy_float avg = at(alpha_start);
        avg += at(alpha_end);
        avg /= 2.0f;
        out[0] = avg;
        return;
    }

    out[0] = at(alpha_start, tx);
    out[out_size - 1] = at(alpha_end, tx);
    if (out_size == 2) {
        return;
    }

    const float inverse_out_size_minus_one = 1.0f / (out_size - 1);
    const float delta = alpha_end - alpha_start;

    for (uint16_t i = 1; i < out_size - 1; i++) {
        float alpha = alpha_start + (delta * i * inverse_out_size_minus_one);
        out[i] = at(alpha, tx);
    }
    return;
}

void XYPath::output16(uint16_t alpha_start, uint16_t alpha_end,
                      pair_xy<uint16_t> *out, uint16_t out_size,
                      const Transform16 &tx) {
    if (out_size == 0) {
        return;
    }

    if (out_size == 1) {
        pair_xy<uint16_t> avg = at16(alpha_start, tx);
        avg += at16(alpha_end, tx);
        avg /= 2;
        out[0] = avg;
        return;
    }
    out[0] = at16(alpha_start, tx);
    out[out_size - 1] = at16(alpha_end, tx);
    if (out_size == 2) {
        return;
    }
    const uint16_t delta = (alpha_end - alpha_start) / (out_size - 1);
    if (delta == 0) {
        // alpha_start == alpha_end
        for (uint16_t i = 1; i < out_size - 1; i++) {
            out[i] = out[0];
        }
        return;
    }

    for (uint16_t i = 1; i < out_size - 1; i++) {
        uint16_t alpha = alpha_start + (delta * i);
        out[i] = at16(alpha, tx);
    }
    return;
}

TransformPath::TransformPath(XYPathPtr path, const Params &params)
    : mPath(path), mParams(params) {
    FASTLED_ASSERT(mPath != nullptr, "TransformPath: path is null");
}

pair_xy_float TransformPath::at(float alpha) {
    pair_xy_float xy = mPath->at(alpha);
    xy.x = xy.x * mParams.scale + mParams.x_offset;
    xy.y = xy.y * mParams.scale + mParams.y_offset;
    return xy;
}

LinePath::LinePath(float x0, float y0, float x1, float y1, uint16_t steps)
    : XYPath(steps), mX0(x0), mY0(y0), mX1(x1), mY1(y1) {}

pair_xy_float LinePath::at(float alpha) {
    // α in [0,1] → (x,y) on the line
    float x = mX0 + alpha * (mX1 - mX0);
    float y = mY0 + alpha * (mY1 - mY0);
    return {x, y};
}

void LinePath::set(float x0, float y0, float x1, float y1) {
    XYPath::clearLut();
    mX0 = x0;
    mY0 = y0;
    mX1 = x1;
    mY1 = y1;
}

pair_xy_float CirclePath::at(float alpha) {
    // α in [0,1] → (x,y) on the circle
    float t = alpha * 2.0f * PI;
    float x = .5f * cosf(t) + .5f;
    float y = .5f * sinf(t) + .5f;
    return pair_xy_float(x, y);
}

CirclePath::CirclePath(uint16_t steps) : XYPath(steps) {}

HeartPath::HeartPath(uint16_t steps) : XYPath(steps) {}

pair_xy_float HeartPath::at(float alpha) {
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

    return pair_xy_float(x, y);
}

pair_xy_float LissajousPath::at(float alpha) {
    // t in [0,2π]
    float t = alpha * 2.0f * PI;
    float x = 0.5f + 0.5f * sinf(mA * t + mDelta);
    float y = 0.5f + 0.5f * sinf(mB * t);
    return pair_xy_float(x, y);
}

LissajousPath::LissajousPath(uint8_t a, uint8_t b, float delta, uint16_t steps)
    : XYPath(steps), mA(a), mB(b), mDelta(delta) {}

ArchimedeanSpiralPath::ArchimedeanSpiralPath(uint8_t turns, float radius,
                                             uint16_t steps)
    : XYPath(steps), mTurns(turns), mRadius(radius) {}

pair_xy_float ArchimedeanSpiralPath::at(float alpha) {
    // α ∈ [0,1] → θ ∈ [0, 2π·turns]
    float t = alpha * 2.0f * PI * mTurns;
    // r grows linearly from 0 to mRadius as α goes 0→1
    float r = mRadius * alpha;
    // convert polar → cartesian, then shift center to (0.5,0.5)
    float x = 0.5f + r * cosf(t);
    float y = 0.5f + r * sinf(t);
    return pair_xy_float(x, y);
}

RosePath::RosePath(uint8_t petals, uint16_t steps)
    : XYPath(steps), mPetals(petals) {}

pair_xy_float RosePath::at(float alpha) {
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
    return pair_xy_float(x, y);
}

GielisCurvePath::GielisCurvePath(uint8_t m, float a, float b, float n1,
                                 float n2, float n3, uint16_t steps)
    : XYPath(steps), mM(m), mA(a), mB(b), mN1(n1), mN2(n2), mN3(n3) {}

pair_xy_float GielisCurvePath::at(float alpha) {
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

    return pair_xy_float(x, y);
}

void XYPath::clearLut() { mLut.reset(); }

PhyllotaxisPath::PhyllotaxisPath(uint16_t count, float angle, uint16_t steps)
    : XYPath(steps), mCount(count), mAngle(angle) {}

pair_xy_float PhyllotaxisPath::at(float alpha) {
    // Map α∈[0,1] → n∈[0,count−1]
    float n = alpha * float(mCount > 1 ? mCount - 1 : 1);
    // Polar coords
    float theta = n * mAngle;
    float r = sqrtf(n / (mCount > 1 ? float(mCount - 1) : 1.0f));
    // Cartesian & normalize to [0,1]²
    float x0 = r * cosf(theta);
    float y0 = r * sinf(theta);
    return pair_xy_float(0.5f + 0.5f * x0, 0.5f + 0.5f * y0);
}

XYPathPtr TransformPath::getPath() const { return mPath; }

TransformPath::Params &TransformPath::params() { return mParams; }

CatmullRomPath::CatmullRomPath(uint16_t steps) : XYPath(steps) {}

void CatmullRomPath::addPoint(pair_xy_float p) { mPoints.push_back(p); }

pair_xy_float CatmullRomPath::at(float alpha) {
    const size_t n = mPoints.size();
    if (n == 0) {
        return pair_xy_float(.5f, 0.5f);
    }
    if (n == 1) {
        return mPoints[0];
    }

    // Scale α ∈ [0,1] to segment index [0..n-2]
    float scaled = alpha * (n - 1);
    size_t i1 = MIN(size_t(floorf(scaled)), n - 2);
    float t = scaled - i1;

    // Indices for p0, p1, p2, p3 (clamping at ends)
    size_t i0 = (i1 == 0 ? 0 : i1 - 1);
    size_t i2 = i1 + 1;
    size_t i3 = (i2 + 1 < n ? i2 + 1 : n - 1);

    auto P0 = mPoints[i0];
    auto P1 = mPoints[i1];
    auto P2 = mPoints[i2];
    auto P3 = mPoints[i3];

    // Catmull-Rom basis (tension = 0.5)
    float t2 = t * t;
    float t3 = t2 * t;

    float x = 0.5f * ((2.0f * P1.x) + (-P0.x + P2.x) * t +
                      (2.0f * P0.x - 5.0f * P1.x + 4.0f * P2.x - P3.x) * t2 +
                      (-P0.x + 3.0f * P1.x - 3.0f * P2.x + P3.x) * t3);

    float y = 0.5f * ((2.0f * P1.y) + (-P0.y + P2.y) * t +
                      (2.0f * P0.y - 5.0f * P1.y + 4.0f * P2.y - P3.y) * t2 +
                      (-P0.y + 3.0f * P1.y - 3.0f * P2.y + P3.y) * t3);

    return pair_xy_float(x, y);
}

} // namespace fl
