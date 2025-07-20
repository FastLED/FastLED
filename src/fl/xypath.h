
#pragma once

// This is a drawing/graphics related class.
//
// XYPath represents a parameterized (x,y) path. The input will always be
// an alpha value between 0->1 (float) or 0->0xffff (u16).
// A look up table can be used to optimize path calculations when steps > 0.
//
// We provide common paths discovered throughout human history, for use in
// your animations.

#include "fl/function.h"
#include "fl/leds.h"
#include "fl/pair.h"
#include "fl/memory.h"
#include "fl/tile2x2.h"
#include "fl/transform.h"
#include "fl/xypath_impls.h"

namespace fl {

class Gradient;

class XYRasterU8Sparse;
template <typename T> class function;

// Smart pointers for the XYPath family.
FASTLED_SMART_PTR(XYPath);
FASTLED_SMART_PTR(XYPathRenderer);
FASTLED_SMART_PTR(XYPathGenerator);
FASTLED_SMART_PTR(XYPathFunction);

namespace xypath_detail {
fl::string unique_missing_name(const char *prefix = "XYCustomPath: ");
} // namespace xypath_detail

class XYPath {
  public:
    /////////////////////////////////////////////
    // Begin pre-baked paths.
    // Point
    static XYPathPtr NewPointPath(float x, float y);
    // Lines and curves
    static XYPathPtr NewLinePath(float x0, float y0, float x1, float y1);
    static XYPathPtr
    NewLinePath(const fl::shared_ptr<LinePathParams> &params = fl::make_shared<LinePathParams>());
    // Cutmull allows for a path to be defined by a set of points. The path will
    // be a smooth curve through the points.
    static XYPathPtr NewCatmullRomPath(
        u16 width = 0, u16 height = 0,
        const fl::shared_ptr<CatmullRomParams> &params = fl::make_shared<CatmullRomParams>());

    // Custom path using just a function.
    static XYPathPtr
    NewCustomPath(const fl::function<vec2f(float)> &path,
                  const rect<i16> &drawbounds = rect<i16>(),
                  const TransformFloat &transform = TransformFloat(),
                  const char *name = nullptr);

    static XYPathPtr NewCirclePath();
    static XYPathPtr NewCirclePath(u16 width, u16 height);
    static XYPathPtr NewHeartPath();
    static XYPathPtr NewHeartPath(u16 width, u16 height);
    static XYPathPtr NewArchimedeanSpiralPath(u16 width, u16 height);
    static XYPathPtr NewArchimedeanSpiralPath();

    static XYPathPtr
    NewRosePath(u16 width = 0, u16 height = 0,
                const fl::shared_ptr<RosePathParams> &params = fl::make_shared<RosePathParams>());

    static XYPathPtr NewPhyllotaxisPath(
        u16 width = 0, u16 height = 0,
        const fl::shared_ptr<PhyllotaxisParams> &args = fl::make_shared<PhyllotaxisParams>());

    static XYPathPtr NewGielisCurvePath(
        u16 width = 0, u16 height = 0,
        const fl::shared_ptr<GielisCurveParams> &params = fl::make_shared<GielisCurveParams>());
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
           TransformFloat transform = TransformFloat());

    virtual ~XYPath();
    vec2f at(float alpha);
    Tile2x2_u8 at_subpixel(float alpha);

    // Rasterizes and draws to the leds.
    void drawColor(const CRGB &color, float from, float to, Leds *leds,
                   int steps = -1);

    void drawGradient(const Gradient &gradient, float from, float to,
                      Leds *leds, int steps = -1);

    // Low level draw function.
    void rasterize(float from, float to, int steps, XYRasterU8Sparse &raster,
                   AlphaFunction *optional_alpha_gen = nullptr);

    void setScale(float scale);
    string name() const;
    // Overloaded to allow transform to be passed in.
    vec2f at(float alpha, const TransformFloat &tx);
    xy_brightness at_brightness(float alpha) {
        vec2f p = at(alpha);
        return xy_brightness(p, 0xff); // Full brightness for now.
    }
    // Needed for drawing to the screen. When this called the rendering will
    // be centered on the width and height such that 0,0 -> maps to .5,.5,
    // which is convenient for drawing since each float pixel can be truncated
    // to an integer type.
    void setDrawBounds(u16 width, u16 height);
    bool hasDrawBounds() const;
    TransformFloat &transform();

    void setTransform(const TransformFloat &transform);

  private:
    int calculateSteps(float from, float to);

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
    XYPathFunction(fl::function<vec2f(float)> f) : mFunction(f) {}
    vec2f compute(float alpha) override { return mFunction(alpha); }
    const string name() const override { return mName; }
    void setName(const string &name) { mName = name; }

    fl::rect<i16> drawBounds() const { return mDrawBounds; }
    void setDrawBounds(const fl::rect<i16> &bounds) { mDrawBounds = bounds; }

    bool hasDrawBounds(fl::rect<i16> *bounds) override {
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
