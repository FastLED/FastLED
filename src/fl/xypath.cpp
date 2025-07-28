

#ifdef __AVR__

// do nothing

#else

#include <math.h>

#include "fl/assert.h"
#include "fl/function.h"
#include "fl/gradient.h"
#include "fl/lut.h"
#include "fl/map_range.h"
#include "fl/math_macros.h"
#include "fl/raster.h"
#include "fl/xypath.h"
#include "fl/xypath_renderer.h"

#include "fl/atomic.h"
#include "fl/thread_local.h"

namespace fl {

namespace { // anonymous namespace

XYRasterU8Sparse &get_tls_raster() {
    static ThreadLocal<XYRasterU8Sparse> tls_raster;
    return tls_raster.access();
}

} // namespace

namespace xypath_detail {
fl::string unique_missing_name(const char *prefix) {
    static fl::atomic<u32> sUniqueName(0);
    u32 id = ++sUniqueName;
    string name = prefix;
    name += id;
    return name;
}
} // namespace xypath_detail

vec2f XYPath::at(float alpha, const TransformFloat &tx) {
    // return compute_float(alpha, tx);
    return mPathRenderer->at(alpha, tx);
}

void XYPath::setDrawBounds(u16 width, u16 height) {
    mPathRenderer->setDrawBounds(width, height);
}

void XYPath::setScale(float scale) { mPathRenderer->setScale(scale); }

string XYPath::name() const { return mPath->name(); }
Tile2x2_u8 XYPath::at_subpixel(float alpha) {
    return mPathRenderer->at_subpixel(alpha);
}

void XYPath::rasterize(float from, float to, int steps,
                       XYRasterU8Sparse &raster,
                       XYPath::AlphaFunction *optional_alpha_gen) {
    mPathRenderer->rasterize(from, to, steps, raster, optional_alpha_gen);
}

vec2f XYPath::at(float alpha) { return mPathRenderer->at(alpha); }

TransformFloat &XYPath::transform() { return mPathRenderer->transform(); }

XYPath::XYPath(XYPathGeneratorPtr path, TransformFloat transform)
    : mPath(path) {
    mPathRenderer = fl::make_shared<XYPathRenderer>(path, transform);
}

XYPath::~XYPath() {}

void XYPathRenderer::rasterize(
    float from, float to, int steps, XYRaster &raster,
    fl::function<u8(float)> *optional_alpha_gen) {
    for (int i = 0; i < steps; ++i) {
        float alpha = fl::map_range<int, float>(i, 0, steps - 1, from, to);
        Tile2x2_u8 tile = at_subpixel(alpha);
        if (optional_alpha_gen) {
            // Scale the tile based on the alpha value.
            u8 a8 = (*optional_alpha_gen)(alpha);
            tile.scale(a8);
        }
        raster.rasterize(tile);
    }
}

void XYPathRenderer::setDrawBounds(u16 width, u16 height) {
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

vec2f XYPathRenderer::compute(float alpha) {
    return compute_float(alpha, mTransform);
}

vec2f XYPathRenderer::at(float alpha) { return at(alpha, mTransform); }

vec2f XYPathRenderer::at(float alpha, const TransformFloat &tx) {
    return compute_float(alpha, tx);
}

XYPathPtr XYPath::NewPointPath(float x, float y) {
    auto path = fl::make_shared<PointPath>(x, y);
    return fl::make_shared<XYPath>(path);
}

XYPathPtr XYPath::NewLinePath(float x0, float y0, float x1, float y1) {
    LinePathParamsPtr p = fl::make_shared<LinePathParams>();
    auto &params = *p;
    params.x0 = x0;
    params.y0 = y0;
    params.x1 = x1;
    params.y1 = y1;
    auto path = fl::make_shared<LinePath>(p);
    return fl::make_shared<XYPath>(path);
}

XYPathPtr XYPath::NewLinePath(const fl::shared_ptr<LinePathParams> &params) {
    auto path = fl::make_shared<LinePath>(params);
    return fl::make_shared<XYPath>(path);
}

XYPathPtr XYPath::NewCirclePath() {
    auto path = fl::make_shared<CirclePath>();
    return fl::make_shared<XYPath>(path);
}

XYPathPtr XYPath::NewCirclePath(u16 width, u16 height) {
    CirclePathPtr path = fl::make_shared<CirclePath>();
    XYPathPtr out = fl::make_shared<XYPath>(path);
    out->setDrawBounds(width, height);
    return out;
}

XYPathPtr XYPath::NewHeartPath() {
    HeartPathPtr path = fl::make_shared<HeartPath>();
    return fl::make_shared<XYPath>(path);
}

XYPathPtr XYPath::NewHeartPath(u16 width, u16 height) {
    HeartPathPtr path = fl::make_shared<HeartPath>();
    XYPathPtr out = fl::make_shared<XYPath>(path);
    out->setDrawBounds(width, height);
    return out;
}

XYPathPtr XYPath::NewArchimedeanSpiralPath(u16 width, u16 height) {
    ArchimedeanSpiralPathPtr path = fl::make_shared<ArchimedeanSpiralPath>();
    XYPathPtr out = fl::make_shared<XYPath>(path);
    out->setDrawBounds(width, height);
    return out;
}

XYPathPtr XYPath::NewArchimedeanSpiralPath() {
    ArchimedeanSpiralPathPtr path = fl::make_shared<ArchimedeanSpiralPath>();
    XYPathPtr out = fl::make_shared<XYPath>(path);
    return out;
}

XYPathPtr XYPath::NewRosePath(u16 width, u16 height,
                              const fl::shared_ptr<RosePathParams> &params) {
    RosePathPtr path = fl::make_shared<RosePath>(params);
    XYPathPtr out = fl::make_shared<XYPath>(path);
    if (width > 0 && height > 0) {
        out->setDrawBounds(width, height);
    }
    return out;
}

XYPathPtr XYPath::NewPhyllotaxisPath(u16 width, u16 height,
                                     const fl::shared_ptr<PhyllotaxisParams> &args) {
    PhyllotaxisPathPtr path = fl::make_shared<PhyllotaxisPath>(args);
    XYPathPtr out = fl::make_shared<XYPath>(path);
    if (width > 0 && height > 0) {
        out->setDrawBounds(width, height);
    }
    return out;
}

XYPathPtr XYPath::NewGielisCurvePath(u16 width, u16 height,
                                     const fl::shared_ptr<GielisCurveParams> &params) {
    GielisCurvePathPtr path = fl::make_shared<GielisCurvePath>(params);
    XYPathPtr out = fl::make_shared<XYPath>(path);
    if (width > 0 && height > 0) {
        out->setDrawBounds(width, height);
    }
    return out;
}

XYPathPtr XYPath::NewCatmullRomPath(u16 width, u16 height,
                                    const fl::shared_ptr<CatmullRomParams> &params) {
    CatmullRomPathPtr path = fl::make_shared<CatmullRomPath>(params);
    XYPathPtr out = fl::make_shared<XYPath>(path);
    if (width > 0 && height > 0) {
        out->setDrawBounds(width, height);
    }
    return out;
}

XYPathPtr XYPath::NewCustomPath(const fl::function<vec2f(float)> &f,
                                const rect<i16> &drawbounds,
                                const TransformFloat &transform,
                                const char *name) {

    XYPathFunctionPtr path = fl::make_shared<XYPathFunction>(f);
    path->setName(name);
    if (!drawbounds.empty()) {
        path->setDrawBounds(drawbounds);
    }
    XYPathPtr out = fl::make_shared<XYPath>(path);
    if (!transform.is_identity()) {
        out->setTransform(transform);
    }
    rect<i16> bounds;
    if (path->hasDrawBounds(&bounds)) {
        if (!bounds.mMin.is_zero()) {
            // Set the bounds to the path's bounds
            FASTLED_WARN(
                "Bounds with an origin other than 0,0 is not supported yet");
        }
        auto w = bounds.width();
        auto h = bounds.height();
        out->setDrawBounds(w, h);
    }
    return out;
}

void XYPath::setTransform(const TransformFloat &transform) {
    mPathRenderer->setTransform(transform);
}

void XYPath::drawColor(const CRGB &color, float from, float to, Leds *leds,
                       int steps) {
    XYRasterU8Sparse &raster = get_tls_raster();
    raster.clear();
    steps = steps > 0 ? steps : calculateSteps(from, to);
    rasterize(from, to, steps, raster);
    raster.draw(color, leds);
}

void XYPath::drawGradient(const Gradient &gradient, float from, float to,
                          Leds *leds, int steps) {
    XYRasterU8Sparse &raster = get_tls_raster();
    raster.clear();
    steps = steps > 0 ? steps : calculateSteps(from, to);
    rasterize(from, to, steps, raster);
    raster.drawGradient(gradient, leds);
}

int XYPath::calculateSteps(float from, float to) {
    float diff = fl::clamp(ABS(to - from), 0.0f, 1.0f);
    return MAX(1, 200 * diff);
}

bool XYPath::hasDrawBounds() const { return mPathRenderer->hasDrawBounds(); }

} // namespace fl

#endif // __AVR__
