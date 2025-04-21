
#include <math.h>

#include "fl/assert.h"
#include "fl/lut.h"
#include "fl/math_macros.h"
#include "fl/xypath.h"

namespace fl {

XYPath::XYPath(XYPathGeneratorPtr path, TransformFloatPtr transform,
               uint16_t steps)
    : mPath(path), mTransform(transform), mSteps(steps) {}

LUTXY16Ptr XYPath::generateLUT(uint16_t steps) {
    LUTXY16Ptr lut = LUTXY16Ptr::New(steps);
    point_xy<uint16_t> *mutable_data = lut->getDataMutable();
    if (steps == 0)
        return lut;

    // use (steps‑1) as the denominator so that i=steps‑1 → alpha=1.0
    float denom = (steps > 1) ? static_cast<float>(steps - 1)
                              : 1.0f; // avoid divide-by-zero when steps==1

    for (uint16_t i = 0; i < steps; ++i) {
        float alpha = static_cast<float>(i) / denom; // now last α == 1.0
        point_xy_float xy = at(alpha);
        uint16_t x = static_cast<uint16_t>(xy.x * 65535.0f);
        uint16_t y = static_cast<uint16_t>(xy.y * 65535.0f);
        mutable_data[i] = {x, y};
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

point_xy_float XYPath::compute_float(float alpha, const TransformFloat &tx) {
    point_xy_float xy = mPath->compute(alpha);
    point_xy_float out = tx.transform(xy);
    out = mGridTransform->transform(out);
    return out;
}

point_xy<uint16_t> XYPath::at16(uint16_t alpha, const Transform16 &tx) {
    if (mSteps > 0) {
        initLutOnce();
        if (mLut) {
            point_xy<uint16_t> out = mLut->interp16(alpha);
            out = tx.transform(out);
            return out;
        }
    }
    // Fallback to the default float implementation. Fine most paths.
    float scale_x_f = static_cast<float>(tx.scale_x);
    float scale_y_f = static_cast<float>(tx.scale_y);
    float alpha_f = static_cast<float>(alpha) / 65535.0f;
    point_xy_float xy = compute(alpha_f);
    // Ensure values are clamped to the target range
    uint16_t x_val =
        MIN(static_cast<uint16_t>(xy.x * scale_x_f) + tx.x_offset, tx.scale_x);
    uint16_t y_val =
        MIN(static_cast<uint16_t>(xy.y * scale_y_f) + tx.y_offset, tx.scale_y);
    return point_xy<uint16_t>(x_val, y_val);
}

void XYPath::buildLut(uint16_t steps) {
    mLut.reset();
    mSteps = steps;
    if (steps > 0) {
        mLut = generateLUT(steps);
    }
}

#if 0
void XYPath::output(float alpha_start, float alpha_end, point_xy_float *out,
                    uint16_t out_size, const TransformFloat &tx) {
    if (out_size == 0) {
        return;
    }
    if (out_size == 1) {
        point_xy_float avg = at(alpha_start);
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
                      point_xy<uint16_t> *out, uint16_t out_size,
                      const Transform16 &tx) {
    if (out_size == 0) {
        return;
    }

    if (out_size == 1) {
        point_xy<uint16_t> avg = at16(alpha_start, tx);
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
#endif

LinePath::LinePath(float x0, float y0, float x1, float y1)
    : mX0(x0), mY0(y0), mX1(x1), mY1(y1) {}

point_xy_float LinePath::compute(float alpha) {
    // α in [0,1] → (x,y) on the line
    float x = mX0 + alpha * (mX1 - mX0);
    float y = mY0 + alpha * (mY1 - mY0);
    return {x, y};
}

void LinePath::set(float x0, float y0, float x1, float y1) {
    mX0 = x0;
    mY0 = y0;
    mX1 = x1;
    mY1 = y1;
}

point_xy_float CirclePath::compute(float alpha) {
    // α in [0,1] → (x,y) on the circle
    float t = alpha * 2.0f * PI;
    float x = .5f * cosf(t) + .5f;
    float y = .5f * sinf(t) + .5f;
    return point_xy_float(x, y);
}

CirclePath::CirclePath() {}

} // namespace fl
