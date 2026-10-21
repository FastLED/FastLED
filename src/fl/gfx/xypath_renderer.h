
#pragma once

// #include "fl/stl/assert.h"
// #include "fl/stl/function.h"
// #include "fl/math/lut.h"
// #include "fl/map_range.h"
// #include "fl/math/math.h"
// #include "fl/gfx/raster.h"
// #include "fl/gfx/xypath.h"
#include "fl/stl/function.h"  // IWYU pragma: keep
#include "fl/stl/shared_ptr.h"         // For FASTLED_SHARED_PTR macros
#include "fl/gfx/tile2x2.h"  // IWYU pragma: keep
#include "fl/math/transform.h"
#include "fl/stl/noexcept.h"

namespace fl {

FASTLED_SHARED_PTR(XYPathGenerator);

class XYPathRenderer {
  public:
    XYPathRenderer(XYPathGeneratorPtr path,
                   TransformFloat transform = TransformFloat()) FL_NO_EXCEPT;
    virtual ~XYPathRenderer() FL_NO_EXCEPT = default; // Add virtual destructor for proper cleanup
    vec2f at(float alpha) FL_NO_EXCEPT;

    Tile2x2_u8 at_subpixel(float alpha) FL_NO_EXCEPT;

    void rasterize(float from, float to, int steps, XYRasterU8Sparse &raster,
                   fl::function<u8(float)> *optional_alpha_gen = nullptr) FL_NO_EXCEPT;

    // Overloaded to allow transform to be passed in.
    vec2f at(float alpha, const TransformFloat &tx) FL_NO_EXCEPT;

    // Needed for drawing to the screen. When this called the rendering will
    // be centered on the width and height such that 0,0 -> maps to .5,.5,
    // which is convenient for drawing since each float pixel can be truncated
    // to an integer type.
    void setDrawBounds(u16 width, u16 height) FL_NO_EXCEPT;
    bool hasDrawBounds() const FL_NO_EXCEPT { return mDrawBoundsSet; }

    void onTransformFloatChanged() FL_NO_EXCEPT;

    TransformFloat &transform() FL_NO_EXCEPT;

    void setTransform(const TransformFloat &transform) FL_NO_EXCEPT {
        mTransform = transform;
        onTransformFloatChanged();
    }

    void setScale(float scale) FL_NO_EXCEPT;

    vec2f compute(float alpha) FL_NO_EXCEPT;

  private:
    XYPathGeneratorPtr mPath;
    TransformFloat mTransform;
    TransformFloat mGridTransform;
    bool mDrawBoundsSet = false;
    vec2f compute_float(float alpha, const TransformFloat &tx) FL_NO_EXCEPT;
};

} // namespace fl
