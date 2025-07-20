
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
    mParams = fl::make_shared<LinePathParams>();
    params().x0 = x0;
    params().y0 = y0;
    params().x1 = x1;
    params().y1 = y1;
}

vec2f LinePath::compute(float alpha) {
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

vec2f CirclePath::compute(float alpha) {
    // α in [0,1] → (x,y) on the unit circle [-1, 1]
    float t = alpha * 2.0f * PI;
    float x = cosf(t);
    float y = sinf(t);
    return vec2f(x, y);
}

CirclePath::CirclePath() {}

HeartPath::HeartPath() {}

vec2f HeartPath::compute(float alpha) {
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

    return vec2f(x, y);
}

ArchimedeanSpiralPath::ArchimedeanSpiralPath(u8 turns, float radius)
    : mTurns(turns), mRadius(radius) {}

vec2f ArchimedeanSpiralPath::compute(float alpha) {
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

    return vec2f(x, y);
}

RosePath::RosePath(u8 n, u8 d) {
    mParams = fl::make_shared<RosePathParams>();
    params().n = n;
    params().d = d;
}

vec2f RosePath::compute(float alpha) {
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

    return vec2f(x, y);
}

vec2f PhyllotaxisPath::compute(float alpha) {
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

    return vec2f{x, y};
}

vec2f GielisCurvePath::compute(float alpha) {
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

    return vec2f{x, y};
}

const string CirclePath::name() const { return "CirclePath"; }

vec2f PointPath::compute(float alpha) {
    FASTLED_UNUSED(alpha);
    return mPoint;
}

const string PointPath::name() const { return "PointPath"; }

void PointPath::set(float x, float y) { set(vec2f(x, y)); }

void PointPath::set(vec2f p) { mPoint = p; }

PointPath::PointPath(float x, float y) : mPoint(x, y) {}

PointPath::PointPath(vec2f p) : mPoint(p) {}

const string LinePath::name() const { return "LinePath"; }

LinePathParams &LinePath::params() { return *mParams; }

const LinePathParams &LinePath::params() const { return *mParams; }

LinePath::LinePath(const LinePathParamsPtr &params) : mParams(params) {}

const string HeartPath::name() const { return "HeartPath"; }

const string ArchimedeanSpiralPath::name() const {
    return "ArchimedeanSpiralPath";
}

void ArchimedeanSpiralPath::setTurns(u8 turns) { mTurns = turns; }

void ArchimedeanSpiralPath::setRadius(float radius) { mRadius = radius; }

const string RosePath::name() const { return "RosePath"; }

void RosePath::setN(u8 n) { params().n = n; }

void RosePath::setD(u8 d) { params().d = d; }

RosePath::RosePath(const fl::shared_ptr<RosePathParams> &p) : mParams(p) {}

RosePathParams &RosePath::params() { return *mParams; }

const RosePathParams &RosePath::params() const { return *mParams; }

const string PhyllotaxisPath::name() const { return "PhyllotaxisPath"; }

PhyllotaxisPath::PhyllotaxisPath(const fl::shared_ptr<PhyllotaxisParams> &p)
    : mParams(p) {}

PhyllotaxisParams &PhyllotaxisPath::params() { return *mParams; }

const PhyllotaxisParams &PhyllotaxisPath::params() const { return *mParams; }

GielisCurvePath::GielisCurvePath(const fl::shared_ptr<GielisCurveParams> &p)
    : mParams(p) {}

const string GielisCurvePath::name() const { return "GielisCurvePath"; }
void GielisCurvePath::setA(float a) { params().a = a; }
void GielisCurvePath::setB(float b) { params().b = b; }
void GielisCurvePath::setM(float m) { params().m = m; }
void GielisCurvePath::setN1(float n1) { params().n1 = n1; }
void GielisCurvePath::setN2(float n2) { params().n2 = n2; }
void GielisCurvePath::setN3(float n3) { params().n3 = n3; }
GielisCurveParams &GielisCurvePath::params() { return *mParams; }
const GielisCurveParams &GielisCurvePath::params() const { return *mParams; }

CatmullRomPath::CatmullRomPath(const fl::shared_ptr<CatmullRomParams> &p) : mParams(p) {}

void CatmullRomPath::addPoint(vec2f p) { params().addPoint(p); }

void CatmullRomPath::addPoint(float x, float y) { params().addPoint(x, y); }

void CatmullRomPath::clear() { params().clear(); }

fl::size CatmullRomPath::size() const { return params().size(); }

CatmullRomParams &CatmullRomPath::params() { return *mParams; }

const CatmullRomParams &CatmullRomPath::params() const { return *mParams; }

vec2f CatmullRomPath::compute(float alpha) {
    const auto &points = params().points;

    // Need at least 2 points to define a path
    if (points.size() < 2) {
        // Return origin if not enough points
        return vec2f(0.0f, 0.0f);
    }

    // If only 2 points, do linear interpolation
    if (points.size() == 2) {
        return vec2f(points[0].x + alpha * (points[1].x - points[0].x),
                     points[0].y + alpha * (points[1].y - points[0].y));
    }

    // For Catmull-Rom, we need 4 points to interpolate between the middle two
    // Scale alpha to the number of segments
    float scaledAlpha = alpha * (points.size() - 1);

    // Determine which segment we're in
    int segment = static_cast<int>(scaledAlpha);

    // Clamp to valid range
    if (segment >= static_cast<int>(points.size()) - 1) {
        segment = points.size() - 2;
        scaledAlpha = static_cast<float>(segment) + 1.0f;
    }

    // Get local alpha within this segment [0,1]
    float t = scaledAlpha - static_cast<float>(segment);

    // Get the four points needed for interpolation
    vec2f p0, p1, p2, p3;

    // Handle boundary cases
    if (segment == 0) {
        // For the first segment, duplicate the first point
        p0 = points[0];
        p1 = points[0];
        p2 = points[1];
        p3 = (points.size() > 2) ? points[2] : points[1];
    } else if (segment == static_cast<int>(points.size()) - 2) {
        // For the last segment, duplicate the last point
        p0 = (segment > 0) ? points[segment - 1] : points[0];
        p1 = points[segment];
        p2 = points[segment + 1];
        p3 = points[segment + 1];
    } else {
        // Normal case - we have points before and after
        p0 = points[segment - 1];
        p1 = points[segment];
        p2 = points[segment + 1];
        p3 = points[segment + 2];
    }

    // Perform Catmull-Rom interpolation
    auto out = interpolate(p0, p1, p2, p3, t);
    return out;
}

vec2f CatmullRomPath::interpolate(const vec2f &p0, const vec2f &p1,
                                  const vec2f &p2, const vec2f &p3,
                                  float t) const {

    // Catmull-Rom interpolation formula
    // Using alpha=0.5 for the "tension" parameter (standard Catmull-Rom)
    float t2 = t * t;
    float t3 = t2 * t;

    // Coefficients for x and y
    float a = -0.5f * p0.x + 1.5f * p1.x - 1.5f * p2.x + 0.5f * p3.x;
    float b = p0.x - 2.5f * p1.x + 2.0f * p2.x - 0.5f * p3.x;
    float c = -0.5f * p0.x + 0.5f * p2.x;
    float d = p1.x;

    float x = a * t3 + b * t2 + c * t + d;

    a = -0.5f * p0.y + 1.5f * p1.y - 1.5f * p2.y + 0.5f * p3.y;
    b = p0.y - 2.5f * p1.y + 2.0f * p2.y - 0.5f * p3.y;
    c = -0.5f * p0.y + 0.5f * p2.y;
    d = p1.y;

    float y = a * t3 + b * t2 + c * t + d;

    return vec2f(x, y);
}

const string CatmullRomPath::name() const { return "CatmullRomPath"; }

} // namespace fl
