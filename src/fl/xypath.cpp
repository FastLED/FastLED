
#include <math.h>

#include "fl/assert.h"
#include "fl/lut.h"
#include "fl/math_macros.h"
#include "fl/xypath.h"

namespace fl {

namespace {
uint8_t to_uint8(float f) {
    // convert to [0..255] range
    uint8_t i = static_cast<uint8_t>(f * 255.0f + .5f);
    return MIN(i, 255);
}
}

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



SubPixel2x2 XYPath::at_subpixel(float alpha) {
    // 1) continuous point, in “pixel‐centers” coordinates [0.5 … W–0.5]
    if (!mDrawBoundsSet) {
        FASTLED_WARN("XYPath::at_subpixel: draw bounds not set");
        return SubPixel2x2();
    }
    point_xy_float xy = at(alpha);

    // 2) shift back so whole‐pixels go 0…W–1, 0…H–1
    float x = xy.x - 0.5f;
    float y = xy.y - 0.5f;

    // 3) integer cell indices
    int cx = static_cast<int>(floorf(x));
    int cy = static_cast<int>(floorf(y));

    // 4) fractional offsets in [0..1)
    float fx = x - cx;
    float fy = y - cy;

    // 5) bilinear weights
    float w_ll = (1 - fx) * (1 - fy);  // lower‑left
    float w_lr = fx       * (1 - fy);  // lower‑right
    float w_ul = (1 - fx) * fy;        // upper‑left
    float w_ur = fx       * fy;        // upper‑right

    // 6) build SubPixel2x2 anchored at (cx,cy)
    SubPixel2x2 out(point_xy<int>(cx, cy));
    out.lower_left()  = to_uint8(w_ll);
    out.lower_right() = to_uint8(w_lr);
    out.upper_left()  = to_uint8(w_ul);
    out.upper_right() = to_uint8(w_ur);

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
    // α in [0,1] → (x,y) on the unit circle [-1, 1]
    float t = alpha * 2.0f * PI;
    float x = cosf(t);
    float y = sinf(t);
    return point_xy_float(x, y);
}

CirclePath::CirclePath() {}

HeartPath::HeartPath() {}

point_xy_float HeartPath::compute(float alpha) {
    // Parametric equation for a heart shape
    // α in [0,1] → (x,y) on the heart curve
    float t = alpha * 2.0f * PI;
    
    // Heart formula based on a modified cardioid with improved aesthetics
    float x = 16.0f * powf(sinf(t), 3);
    
    // Modified y formula for a more balanced heart shape
    // This creates a fuller bottom and more defined top curve
    float y = -(13.0f * cosf(t) - 5.0f * cosf(2.0f * t) - 2.0f * cosf(3.0f * t) - cosf(4.0f * t));
    
    // Scale to fit in [-1, 1] range
    // The 16.0f divisor for x ensures x is in [-1, 1]
    x /= 16.0f;
    
    // Scale y to ensure it's in [-1, 1]
    // The 16.0f divisor ensures proper scaling while maintaining proportions
    y /= -16.0f;
    
    // Apply a slight vertical stretch to fill the [-1, 1] range better
    y *= 1.10f;

    y += 0.17f; // Adjust y to fit within the range of [-1, 1]
    
    return point_xy_float(x, y);
}

ArchimedeanSpiralPath::ArchimedeanSpiralPath(uint8_t turns, float radius)
    : mTurns(turns), mRadius(radius) {}

point_xy_float ArchimedeanSpiralPath::compute(float alpha) {
    // Parametric equation for an Archimedean spiral
    // α in [0,1] → (x,y) on the spiral curve
    
    // Calculate the angle based on the number of turns
    float theta = alpha * 2.0f * PI * mTurns;
    
    // Calculate the radius at this angle (grows linearly with angle)
    // Scale by alpha to ensure we start at center and grow outward
    float r = alpha * mRadius;
    
    // Convert polar coordinates (r, theta) to Cartesian (x, y)
    float x = r * cosf(theta);
    float y = r * sinf(theta);
    
    // Ensure the spiral fits within [-1, 1] range
    // No additional scaling needed as we control the radius directly
    
    return point_xy_float(x, y);
}

RosePath::RosePath(uint8_t n, uint8_t d) : mN(n), mD(d) {}

point_xy_float RosePath::compute(float alpha) {
    // Parametric equation for a rose curve (rhodonea)
    // α in [0,1] → (x,y) on the rose curve
    
    // Map alpha to the full range needed for the rose
    // For a complete rose, we need to go through k*PI radians where k is:
    // - k = n if n is odd and d is 1
    // - k = 2n if n is even and d is 1
    // - k = n*d if n and d are coprime
    // For simplicity, we'll use 2*PI*n as a good approximation
    float theta = alpha * 2.0f * PI * mN;
    
    // Calculate the radius using the rose formula: r = cos(n*θ/d)
    // We use cosine for a rose that starts with a petal at theta=0
    float r = cosf(mN * theta / mD);
    
    // Scale to ensure the rose fits within [-1, 1] range
    // The absolute value ensures we get the proper shape
    r = fabsf(r);
    
    // Convert polar coordinates (r, theta) to Cartesian (x, y)
    float x = r * cosf(theta);
    float y = r * sinf(theta);
    
    return point_xy_float(x, y);
}

} // namespace fl
