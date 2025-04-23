
#include "fl/xypath_impls.h"

#include <math.h>

#include "fl/assert.h"
#include "fl/function.h"
#include "fl/lut.h"
#include "fl/map_range.h"
#include "fl/math_macros.h"
#include "fl/raster.h"

#include "fl/xypath_renderer.h"

namespace fl {

LinePath::LinePath(float x0, float y0, float x1, float y1) {
    mParams = NewPtr<LinePathParams>();
    params().x0 = x0;
    params().y0 = y0;
    params().x1 = x1;
    params().y1 = y1;
}

point_xy_float LinePath::compute(float alpha) {
    // α in [0,1] → (x,y) on the line
    float x = params().x0 + alpha * (params().x1 - params().x0);
    float y = params().y0 + alpha * (params().y1 - params().y0);
    return {x, y};
}

void LinePath::set(float x0, float y0, float x1, float y1) {
    params().x0 = x0;
    params().y0 = y0;
    params().x1 = x1;
    params().y1 = y1;
}

void LinePath::set(const LinePathParams &p) { params() = p; }

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
    float y = -(13.0f * cosf(t) - 5.0f * cosf(2.0f * t) -
                2.0f * cosf(3.0f * t) - cosf(4.0f * t));

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

RosePath::RosePath(uint8_t n, uint8_t d) {
    mParams = NewPtr<RosePathParams>();
    params().n = n;
    params().d = d;
}

point_xy_float RosePath::compute(float alpha) {
    // Parametric equation for a rose curve (rhodonea)
    // α in [0,1] → (x,y) on the rose curve

    // Map alpha to the full range needed for the rose
    // For a complete rose, we need to go through k*PI radians where k is:
    // - k = n if n is odd and d is 1
    // - k = 2n if n is even and d is 1
    // - k = n*d if n and d are coprime
    // For simplicity, we'll use 2*PI*n as a good approximation
    float theta = alpha * 2.0f * PI * params().n;

    // Calculate the radius using the rose formula: r = cos(n*θ/d)
    // We use cosine for a rose that starts with a petal at theta=0
    float r = cosf(params().n * theta / params().d);

    // Scale to ensure the rose fits within [-1, 1] range
    // The absolute value ensures we get the proper shape
    r = fabsf(r);

    // Convert polar coordinates (r, theta) to Cartesian (x, y)
    float x = r * cosf(theta);
    float y = r * sinf(theta);

    return point_xy_float(x, y);
}

point_xy_float PhyllotaxisPath::compute(float alpha) {
    // total number of points you want in the pattern
    const float N = static_cast<float>(params().c);

    // continuous “index” from 0…N
    float n = alpha * N;

    // use the golden angle in radians:
    //    π * (3 – √5) ≈ 2.399963229728653
    constexpr float goldenAngle = PI * (3.0f - 1.6180339887498948f);

    // normalized radius [0…1]: sqrt(n/N) gives uniform point density
    float r = sqrtf(n / N);

    // spiral angle
    float theta = n * goldenAngle;

    // polar → Cartesian
    float x = r * cosf(theta);
    float y = r * sinf(theta);

    return point_xy_float{x, y};
}

point_xy_float GielisCurvePath::compute(float alpha) {
    // 1) map alpha to angle θ ∈ [0 … 2π)
    constexpr float kTwoPi = 6.283185307179586f;
    float theta = alpha * kTwoPi;

    // 2) superformula parameters (members of your path)
    //    a, b control the “shape scale” (often both = 1)
    //    m  controls symmetry (integer number of lobes)
    //    n1,n2,n3 control curvature/sharpness
    float a = params().a;
    float b = params().b;
    float m = params().m;
    float n1 = params().n1;
    float n2 = params().n2;
    float n3 = params().n3;

    // 3) compute radius from superformula
    float t2 = m * theta / 4.0f;
    float part1 = powf(fabsf(cosf(t2) / a), n2);
    float part2 = powf(fabsf(sinf(t2) / b), n3);
    float r = powf(part1 + part2, -1.0f / n1);

    // 4) polar → Cartesian in unit circle
    float x = r * cosf(theta);
    float y = r * sinf(theta);

    return point_xy_float{x, y};
}

const Str CirclePath::name() const { return "CirclePath"; }

point_xy_float PointPath::compute(float alpha) {
    FASTLED_UNUSED(alpha);
    return mPoint;
}

const Str PointPath::name() const { return "PointPath"; }

void PointPath::set(float x, float y) { set(point_xy_float(x, y)); }

void PointPath::set(point_xy_float p) { mPoint = p; }

PointPath::PointPath(float x, float y) : mPoint(x, y) {}

PointPath::PointPath(point_xy_float p) : mPoint(p) {}

const Str LinePath::name() const { return "LinePath"; }

LinePathParams &LinePath::params() { return *mParams; }

const LinePathParams &LinePath::params() const { return *mParams; }

LinePath::LinePath(const LinePathParamsPtr &params) : mParams(params) {}

const Str HeartPath::name() const { return "HeartPath"; }

const Str ArchimedeanSpiralPath::name() const {
    return "ArchimedeanSpiralPath";
}

void ArchimedeanSpiralPath::setTurns(uint8_t turns) { mTurns = turns; }

void ArchimedeanSpiralPath::setRadius(float radius) { mRadius = radius; }

const Str RosePath::name() const { return "RosePath"; }

void RosePath::setN(uint8_t n) { params().n = n; }

void RosePath::setD(uint8_t d) { params().d = d; }

RosePath::RosePath(const Ptr<RosePathParams> &p) : mParams(p) {}

RosePathParams &RosePath::params() { return *mParams; }

const RosePathParams &RosePath::params() const { return *mParams; }

const Str PhyllotaxisPath::name() const { return "PhyllotaxisPath"; }

PhyllotaxisPath::PhyllotaxisPath(const Ptr<PhyllotaxisParams> &p)
    : mParams(p) {}

PhyllotaxisParams &PhyllotaxisPath::params() { return *mParams; }

const PhyllotaxisParams &PhyllotaxisPath::params() const { return *mParams; }

GielisCurvePath::GielisCurvePath(const Ptr<GielisCurveParams> &p)
    : mParams(p) {}

const Str GielisCurvePath::name() const { return "GielisCurvePath"; }
void GielisCurvePath::setA(float a) { params().a = a; }
void GielisCurvePath::setB(float b) { params().b = b; }
void GielisCurvePath::setM(float m) { params().m = m; }
void GielisCurvePath::setN1(float n1) { params().n1 = n1; }
void GielisCurvePath::setN2(float n2) { params().n2 = n2; }
void GielisCurvePath::setN3(float n3) { params().n3 = n3; }
GielisCurveParams &GielisCurvePath::params() { return *mParams; }
const GielisCurveParams &GielisCurvePath::params() const { return *mParams; }


// Str CatmullRomPath::name() const { return "CatmullRomPath"; }

} // namespace fl
