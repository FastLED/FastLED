
#pragma once

// This is a drawing/graphics related class.
//
// XYPath represents a parameterized (x,y) path. The input will always be
// an alpha value between 0->1 (float) or 0->0xffff (uint16_t).
// A look up table can be used to optimize path calculations when steps > 0.
//
// We provide common paths discovered throughout human history, for use in
// your animations.

#include "fl/ptr.h"
#include "fl/tile2x2.h"
#include "fl/transform.h"
#include "fl/xypath_impls.h"

namespace fl {

class XYRasterSparse;
template <typename T> class function;

// Smart pointers for the XYPath family.
FASTLED_SMART_PTR(XYPath);
FASTLED_SMART_PTR(XYPathRenderer);
FASTLED_SMART_PTR(XYPathGenerator);

class XYPath : public Referent {
  public:
    static XYPathPtr NewPointPath(float x, float y);
    static XYPathPtr NewLinePath(float x0, float y0, float x1, float y1);
    static XYPathPtr
    NewLinePath(const Ptr<LinePathParams> &params = NewPtr<LinePathParams>());

    static XYPathPtr NewCirclePath();
    static XYPathPtr NewCirclePath(uint16_t width, uint16_t height);
    static XYPathPtr NewHeartPath();
    static XYPathPtr NewHeartPath(uint16_t width, uint16_t height);
    static XYPathPtr NewArchimedeanSpiralPath(uint16_t width, uint16_t height);
    static XYPathPtr NewArchimedeanSpiralPath();

    static XYPathPtr
    NewRosePath(uint16_t width = 0, uint16_t height = 0,
                const Ptr<RosePathParams> &params = NewPtr<RosePathParams>());

    static XYPathPtr NewPhyllotaxisPath(
        uint16_t width = 0, uint16_t height = 0,
        const Ptr<PhyllotaxisParams> &args = NewPtr<PhyllotaxisParams>());

    static XYPathPtr NewGielisCurvePath(
        uint16_t width = 0, uint16_t height = 0,
        const Ptr<GielisCurveParams> &params = NewPtr<GielisCurveParams>());

    // Create a new Catmull-Rom spline path with custom parameters
    static XYPathPtr NewCatmullRomPath(
        const Ptr<CatmullRomParams> &params = NewPtr<CatmullRomParams>());

    virtual ~XYPath();
    point_xy_float at(float alpha);
    Tile2x2_u8 at_subpixel(float alpha);
    void rasterize(float from, float to, int steps, XYRasterSparse &raster,
                   fl::function<uint8_t(float)> *optional_alpha_gen = nullptr);
    void setScale(float scale);
    Str name() const;
    // Overloaded to allow transform to be passed in.
    point_xy_float at(float alpha, const TransformFloat &tx);
    // Needed for drawing to the screen. When this called the rendering will
    // be centered on the width and height such that 0,0 -> maps to .5,.5,
    // which is convenient for drawing since each float pixel can be truncated
    // to an integer type.
    void setDrawBounds(uint16_t width, uint16_t height);
    TransformFloat &transform();

    XYPath(XYPathGeneratorPtr path,
           TransformFloat transform = TransformFloat());

  private:
    XYPathGeneratorPtr mPath;
    XYPathRendererPtr mPathRenderer;
};

} // namespace fl
