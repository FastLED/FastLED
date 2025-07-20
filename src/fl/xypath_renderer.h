
#pragma once

// #include "fl/assert.h"
// #include "fl/function.h"
// #include "fl/lut.h"
// #include "fl/map_range.h"
// #include "fl/math_macros.h"
// #include "fl/raster.h"
// #include "fl/xypath.h"
#include "fl/function.h"
#include "fl/memory.h"
#include "fl/tile2x2.h"
#include "fl/transform.h"

namespace fl {

FASTLED_SMART_PTR(XYPathGenerator);

class XYPathRenderer {
  public:
    XYPathRenderer(XYPathGeneratorPtr path,
                   TransformFloat transform = TransformFloat());
    virtual ~XYPathRenderer() = default; // Add virtual destructor for proper cleanup
    vec2f at(float alpha);

    Tile2x2_u8 at_subpixel(float alpha);

    void rasterize(float from, float to, int steps, XYRasterU8Sparse &raster,
                   fl::function<u8(float)> *optional_alpha_gen = nullptr);

    // Overloaded to allow transform to be passed in.
    vec2f at(float alpha, const TransformFloat &tx);

    // Needed for drawing to the screen. When this called the rendering will
    // be centered on the width and height such that 0,0 -> maps to .5,.5,
    // which is convenient for drawing since each float pixel can be truncated
    // to an integer type.
    void setDrawBounds(u16 width, u16 height);
    bool hasDrawBounds() const { return mDrawBoundsSet; }

    void onTransformFloatChanged();

    TransformFloat &transform();

    void setTransform(const TransformFloat &transform) {
        mTransform = transform;
        onTransformFloatChanged();
    }

    void setScale(float scale);

    vec2f compute(float alpha);

  private:
    XYPathGeneratorPtr mPath;
    TransformFloat mTransform;
    TransformFloat mGridTransform;
    bool mDrawBoundsSet = false;
    vec2f compute_float(float alpha, const TransformFloat &tx);
};

} // namespace fl
