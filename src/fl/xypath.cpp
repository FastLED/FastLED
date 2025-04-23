
#include <math.h>

#include "fl/assert.h"
#include "fl/function.h"
#include "fl/lut.h"
#include "fl/map_range.h"
#include "fl/math_macros.h"
#include "fl/raster.h"
#include "fl/xypath.h"
#include "fl/xypath_renderer.h"

namespace fl {



point_xy_float XYPath::at(float alpha, const TransformFloat &tx) {
    // return compute_float(alpha, tx);
    return mPathRenderer->at(alpha, tx);
}

void XYPath::setDrawBounds(uint16_t width, uint16_t height) {
    mPathRenderer->setDrawBounds(width, height);
}

void XYPath::setScale(float scale) { mPathRenderer->setScale(scale); }

Str XYPath::name() const { return mPath->name(); }
Tile2x2_u8 XYPath::at_subpixel(float alpha) {
    return mPathRenderer->at_subpixel(alpha);
}

void XYPath::rasterize(float from, float to, int steps, XYRasterSparse &raster,
                       function<uint8_t(float)> *optional_alpha_gen) {
    mPathRenderer->rasterize(from, to, steps, raster, optional_alpha_gen);
}

point_xy_float XYPath::at(float alpha) { return mPathRenderer->at(alpha); }

TransformFloat &XYPath::transform() { return mPathRenderer->transform(); }

XYPath::XYPath(XYPathGeneratorPtr path, TransformFloat transform)
    : mPath(path) {
    mPathRenderer = XYPathRendererPtr::New(path, transform);
}

XYPath::~XYPath() {}



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

void XYPathRenderer::rasterize(
    float from, float to, int steps, XYRaster &raster,
    fl::function<uint8_t(float)> *optional_alpha_gen) {
    for (int i = 0; i < steps; ++i) {
        float alpha = fl::map_range<int, float>(i, 0, steps - 1, from, to);
        Tile2x2_u8 tile = at_subpixel(alpha);
        if (optional_alpha_gen) {
            // Scale the tile based on the alpha value.
            uint8_t a8 = (*optional_alpha_gen)(alpha);
            tile.scale(a8);
        }
        raster.rasterize(tile);
    }
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

GielisCurvePath::GielisCurvePath(
    const Ptr<GielisCurveParams> &p)
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

void XYPathRenderer::setDrawBounds(uint16_t width, uint16_t height) {
    // auto &tx = *(mGridTransform.mImpl);
    auto &tx = mGridTransform;

    // 1) map world‑X ∈ [–1..+1] → pixel‑X ∈ [0.5 .. width–0.5]
    //    scale_x  = ( (width–0.5) – 0.5 ) / 2 = (width–1)/2
    //    offset_x = (width–0.5 + 0.5) / 2 = width/2
    tx.set_scale_x((width - 1.0f) * 0.5f);
    // tx.scale_x = (width - 1.0f) * 0.5f;
    // tx.offset_x = width * 0.5f;
    tx.set_offset_x(width * 0.5f);

    // 2) map world‑Y ∈ [ -1 .. 1 ] → pixel‑Y ∈ [0.5 .. height–0.5]
    //    (your LinePath lives at Y=0, so it will sit at row‑0 center = 0.5)
    //    scale_y  = (height–0.5) – 0.5     = height–1
    //    offset_y = 0.5
    // tx.scale_y = (height - 1.0f) * 0.5f;
    // tx.offset_y = height * 0.5f;

    tx.set_scale_y((height - 1.0f) * 0.5f);
    tx.set_offset_y(height * 0.5f);

    onTransformFloatChanged();
    mDrawBoundsSet = true;
}

void XYPathRenderer::onTransformFloatChanged() {
    // Future use to allow recomputing the LUT.
}

TransformFloat &XYPathRenderer::transform() { return mTransform; }

void XYPathRenderer::setScale(float scale) {
    // mTransform.scale_x = scale;
    // mTransform.scale_y = scale;
    mTransform.set_scale(scale);
    onTransformFloatChanged();
}

point_xy_float XYPathRenderer::compute(float alpha) {
    return compute_float(alpha, mTransform);
}

point_xy_float XYPathRenderer::at(float alpha) { return at(alpha, mTransform); }

point_xy_float XYPathRenderer::at(float alpha, const TransformFloat &tx) {
    return compute_float(alpha, tx);
}

XYPathPtr XYPath::NewPointPath(float x, float y) {
    auto path = PointPathPtr::New(x, y);
    return XYPathPtr::New(path);
}

XYPathPtr XYPath::NewLinePath(float x0, float y0, float x1, float y1) {
    LinePathParamsPtr p = LinePathParamsPtr::New();
    auto &params = *p;
    params.x0 = x0;
    params.y0 = y0;
    params.x1 = x1;
    params.y1 = y1;
    auto path = LinePathPtr::New(p);
    return XYPathPtr::New(path);
}

XYPathPtr XYPath::NewLinePath(
    const Ptr<LinePathParams> &params) {
    auto path = NewPtr<LinePath>(params);
    return XYPathPtr::New(path);
}

XYPathPtr XYPath::NewCirclePath() {
    auto path = CirclePathPtr::New();
    return XYPathPtr::New(path);
}

XYPathPtr XYPath::NewCirclePath(uint16_t width, uint16_t height) {
    CirclePathPtr path = CirclePathPtr::New();
    XYPathPtr out = XYPathPtr::New(path);
    out->setDrawBounds(width, height);
    return out;
}

XYPathPtr XYPath::NewHeartPath() {
    HeartPathPtr path = HeartPathPtr::New();
    return XYPathPtr::New(path);
}

XYPathPtr XYPath::NewHeartPath(uint16_t width, uint16_t height) {
    HeartPathPtr path = HeartPathPtr::New();
    XYPathPtr out = XYPathPtr::New(path);
    out->setDrawBounds(width, height);
    return out;
}

XYPathPtr XYPath::NewArchimedeanSpiralPath(uint16_t width, uint16_t height) {
    ArchimedeanSpiralPathPtr path = ArchimedeanSpiralPathPtr::New();
    XYPathPtr out = XYPathPtr::New(path);
    out->setDrawBounds(width, height);
    return out;
}

XYPathPtr XYPath::NewArchimedeanSpiralPath() {
    ArchimedeanSpiralPathPtr path = ArchimedeanSpiralPathPtr::New();
    XYPathPtr out = XYPathPtr::New(path);
    return out;
}

XYPathPtr XYPath::NewRosePath(
    uint16_t width, uint16_t height,
    const Ptr<RosePathParams> &params) {
    RosePathPtr path = RosePathPtr::New(params);
    XYPathPtr out = XYPathPtr::New(path);
    if (width > 0 && height > 0) {
        out->setDrawBounds(width, height);
    }
    return out;
}

XYPathPtr XYPath::NewPhyllotaxisPath(
    uint16_t width, uint16_t height,
    const Ptr<PhyllotaxisParams> &args) {
    PhyllotaxisPathPtr path = PhyllotaxisPathPtr::New(args);
    XYPathPtr out = XYPathPtr::New(path);
    if (width > 0 && height > 0) {
        out->setDrawBounds(width, height);
    }
    return out;
}

XYPathPtr XYPath::NewGielisCurvePath(
    uint16_t width, uint16_t height,
    const Ptr<GielisCurveParams> &params) {
    GielisCurvePathPtr path = GielisCurvePathPtr::New(params);
    XYPathPtr out = XYPathPtr::New(path);
    if (width > 0 && height > 0) {
        out->setDrawBounds(width, height);
    }
    return out;
}



//Str CatmullRomPath::name() const { return "CatmullRomPath"; }

} // namespace fl
