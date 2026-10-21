
#pragma once

// This is a drawing/graphics related class.
//
// XYPath represents a parameterized (x,y) path. The input will always be
// an alpha value between 0->1 (float) or 0->0xffff (u16).
// A look up table can be used to optimize path calculations when steps > 0.
//
// We provide common paths discovered throughout human history, for use in
// your animations.

#include "fl/stl/function.h"
#include "fl/gfx/leds.h"  // IWYU pragma: keep
#include "fl/stl/pair.h"
#include "fl/stl/shared_ptr.h"         // For FASTLED_SHARED_PTR macros
#include "fl/gfx/tile2x2.h"  // IWYU pragma: keep
#include "fl/math/transform.h"
#include "fl/gfx/xypath_impls.h"
#include "fl/stl/noexcept.h"

namespace fl {

class Gradient;

class XYRasterU8Sparse;
template <typename T> class function;

// Smart pointers for the XYPath family.
FASTLED_SHARED_PTR(XYPath);
FASTLED_SHARED_PTR(XYPathRenderer);
FASTLED_SHARED_PTR(XYPathGenerator);
FASTLED_SHARED_PTR(XYPathFunction);

namespace xypath_detail {
fl::string unique_missing_name(const char *prefix = "XYCustomPath: ") FL_NO_EXCEPT;
} // namespace xypath_detail

class XYPath {
  public:
    /////////////////////////////////////////////
    // Begin pre-baked paths.
    // Point
    static XYPathPtr NewPointPath(float x, float y) FL_NO_EXCEPT;
    // Lines and curves
    static XYPathPtr NewLinePath(float x0, float y0, float x1, float y1) FL_NO_EXCEPT;
    static XYPathPtr
    NewLinePath(const fl::shared_ptr<LinePathParams> &params = fl::make_shared<LinePathParams>()) FL_NO_EXCEPT;
    // Cutmull allows for a path to be defined by a set of points. The path will
    // be a smooth curve through the points.
    static XYPathPtr NewCatmullRomPath(
        u16 width = 0, u16 height = 0,
        const fl::shared_ptr<CatmullRomParams> &params = fl::make_shared<CatmullRomParams>()) FL_NO_EXCEPT;

    // Custom path using just a function.
    static XYPathPtr
    NewCustomPath(const fl::function<vec2f(float)> &path,
                  const rect<i16> &drawbounds = rect<i16>(),
                  const TransformFloat &transform = TransformFloat(),
                  const char *name = nullptr) FL_NO_EXCEPT;

    static XYPathPtr NewCirclePath() FL_NO_EXCEPT;
    static XYPathPtr NewCirclePath(u16 width, u16 height) FL_NO_EXCEPT;
    static XYPathPtr NewHeartPath() FL_NO_EXCEPT;
    static XYPathPtr NewHeartPath(u16 width, u16 height) FL_NO_EXCEPT;
    static XYPathPtr NewArchimedeanSpiralPath(u16 width, u16 height) FL_NO_EXCEPT;
    static XYPathPtr NewArchimedeanSpiralPath() FL_NO_EXCEPT;

    static XYPathPtr
    NewRosePath(u16 width = 0, u16 height = 0,
                const fl::shared_ptr<RosePathParams> &params = fl::make_shared<RosePathParams>()) FL_NO_EXCEPT;

    static XYPathPtr NewPhyllotaxisPath(
        u16 width = 0, u16 height = 0,
        const fl::shared_ptr<PhyllotaxisParams> &args = fl::make_shared<PhyllotaxisParams>()) FL_NO_EXCEPT;

    static XYPathPtr NewGielisCurvePath(
        u16 width = 0, u16 height = 0,
        const fl::shared_ptr<GielisCurveParams> &params = fl::make_shared<GielisCurveParams>()) FL_NO_EXCEPT;
    // END pre-baked paths.

    // Takes in a float at time [0, 1] and returns alpha values
    // for that point in time.
    using AlphaFunction = fl::function<u8(float)>;

    // Future work: we don't actually want just the point, but also
    // it's intensity at that value. Otherwise a seperate class has to
    // made to also control the intensity and that sucks.
    using xy_brightness = fl::pair<vec2f, u8>;

    /////////////////////////////////////////////////////////////
    // Create a new Catmull-Rom spline path with custom parameters
    XYPath(XYPathGeneratorPtr path,
           TransformFloat transform = TransformFloat()) FL_NO_EXCEPT;

    virtual ~XYPath() FL_NO_EXCEPT;
    vec2f at(float alpha) FL_NO_EXCEPT;
    Tile2x2_u8 at_subpixel(float alpha) FL_NO_EXCEPT;

    // Rasterizes and draws to the leds.
    void drawColor(const CRGB &color, float from, float to, Leds *leds,
                   int steps = -1) FL_NO_EXCEPT;

    void drawGradient(const Gradient &gradient, float from, float to,
                      Leds *leds, int steps = -1) FL_NO_EXCEPT;

    // Low level draw function.
    void rasterize(float from, float to, int steps, XYRasterU8Sparse &raster,
                   AlphaFunction *optional_alpha_gen = nullptr) FL_NO_EXCEPT;

    void setScale(float scale) FL_NO_EXCEPT;
    string name() const FL_NO_EXCEPT;
    // Overloaded to allow transform to be passed in.
    vec2f at(float alpha, const TransformFloat &tx) FL_NO_EXCEPT;
    xy_brightness at_brightness(float alpha) FL_NO_EXCEPT {
        vec2f p = at(alpha);
        return xy_brightness(p, 0xff); // Full brightness for now.
    }
    // Needed for drawing to the screen. When this called the rendering will
    // be centered on the width and height such that 0,0 -> maps to .5,.5,
    // which is convenient for drawing since each float pixel can be truncated
    // to an integer type.
    void setDrawBounds(u16 width, u16 height) FL_NO_EXCEPT;
    bool hasDrawBounds() const FL_NO_EXCEPT;
    TransformFloat &transform() FL_NO_EXCEPT;

    void setTransform(const TransformFloat &transform) FL_NO_EXCEPT;

  private:
    int calculateSteps(float from, float to) FL_NO_EXCEPT;

    XYPathGeneratorPtr mPath;
    XYPathRendererPtr mPathRenderer;

    // By default the XYPath will use a shared raster. This is a problem on
    // multi threaded apps. Since there isn't an easy way to check for multi
    // threading, give the api user the ability to turn this off and use a local
    // raster.
    scoped_ptr<XYRasterU8Sparse> mOptionalRaster;
};

class XYPathFunction : public XYPathGenerator {
  public:
    XYPathFunction(fl::function<vec2f(float)> f) FL_NO_EXCEPT : mFunction(f) {}
    vec2f compute(float alpha) FL_NO_EXCEPT override { return mFunction(alpha); }
    const string name() const FL_NO_EXCEPT override { return mName; }
    void setName(const string &name) FL_NO_EXCEPT { mName = name; }

    fl::rect<i16> drawBounds() const FL_NO_EXCEPT { return mDrawBounds; }
    void setDrawBounds(const fl::rect<i16> &bounds) FL_NO_EXCEPT { mDrawBounds = bounds; }

    bool hasDrawBounds(fl::rect<i16> *bounds) FL_NO_EXCEPT override {
        if (bounds) {
            *bounds = mDrawBounds;
        }
        return true;
    }

  private:
    fl::function<vec2f(float)> mFunction;
    fl::string mName = "XYPathFunction Unnamed";
    fl::rect<i16> mDrawBounds;
};

} // namespace fl
