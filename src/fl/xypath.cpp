
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

namespace xypath_detail {
fl::Str unique_missing_name(const fl::Str& prefix) {
    static int sUniqueName = 0;
    int id = ++sUniqueName;
    Str name = prefix;
    name += id;
    return name;
}
} // namespace xypath_detail

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

void XYPath::rasterize(float from, float to, int steps, XYRasterU8Sparse &raster,
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

XYPathPtr XYPath::NewLinePath(const Ptr<LinePathParams> &params) {
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

XYPathPtr XYPath::NewRosePath(uint16_t width, uint16_t height,
                              const Ptr<RosePathParams> &params) {
    RosePathPtr path = RosePathPtr::New(params);
    XYPathPtr out = XYPathPtr::New(path);
    if (width > 0 && height > 0) {
        out->setDrawBounds(width, height);
    }
    return out;
}

XYPathPtr XYPath::NewPhyllotaxisPath(uint16_t width, uint16_t height,
                                     const Ptr<PhyllotaxisParams> &args) {
    PhyllotaxisPathPtr path = PhyllotaxisPathPtr::New(args);
    XYPathPtr out = XYPathPtr::New(path);
    if (width > 0 && height > 0) {
        out->setDrawBounds(width, height);
    }
    return out;
}

XYPathPtr XYPath::NewGielisCurvePath(uint16_t width, uint16_t height,
                                     const Ptr<GielisCurveParams> &params) {
    GielisCurvePathPtr path = GielisCurvePathPtr::New(params);
    XYPathPtr out = XYPathPtr::New(path);
    if (width > 0 && height > 0) {
        out->setDrawBounds(width, height);
    }
    return out;
}

XYPathPtr XYPath::NewCatmullRomPath(uint16_t width, uint16_t height,
                                    const Ptr<CatmullRomParams> &params) {
    CatmullRomPathPtr path = CatmullRomPathPtr::New(params);
    XYPathPtr out = XYPathPtr::New(path);
    if (width > 0 && height > 0) {
        out->setDrawBounds(width, height);
    }
    return out;
}

XYPathPtr
XYPath::NewCustomPath(const fl::function<point_xy_float(float)> &f,
                      const rect_xy<int> & drawbounds,
                      const TransformFloat &transform,
                      const Str &name) {

    XYPathFunctionPtr path = NewPtr<XYPathFunction>(f);
    path->setName(name);
    if (!drawbounds.empty()) {
        path->setDrawBounds(drawbounds);

    }
    XYPathPtr out = XYPathPtr::New(path);
    if (!transform.is_identity()) {
        out->setTransform(transform);
    }
    rect_xy<int> bounds;
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



void XYPath::draw(const CRGB &color, const XYMap &xyMap, CRGB *leds) {
    FASTLED_UNUSED(color);
    FASTLED_UNUSED(xyMap);
    FASTLED_UNUSED(leds);
  
    // mPathRenderer->draw(color, xyMap, leds);
}

} // namespace fl
