#include "fl/fx/fx2d_to_1d.h"

#include "fl/math_macros.h"
#include "fl/xymap.h"
#include "crgb.h"
#include "fl/stl/allocator.h"

namespace fl {

Fx2dTo1d::Fx2dTo1d(u16 numLeds, Fx2dPtr fx2d, const ScreenMap &screenMap,
                   InterpolationMode mode)
    : Fx1d(numLeds), mFx2d(fx2d), mScreenMap(screenMap),
      mInterpolationMode(mode), mGrid(new CRGB[fx2d->getNumLeds()]) {}

void Fx2dTo1d::setFx2d(Fx2dPtr fx2d) {
    mFx2d = fx2d;
    // Reallocate grid buffer if needed
    mGrid.reset(new CRGB[fx2d->getNumLeds()]);
}

void Fx2dTo1d::draw(DrawContext context) {
    // Step 1: Render 2D effect to internal grid
    DrawContext grid_context(context.now, mGrid.get(), mFx2d->getNumLeds());
    mFx2d->draw(grid_context);

    // Step 2: Sample from grid to 1D output
    const XYMap &xyMap = mFx2d->getXYMap();

    for (u16 i = 0; i < mNumLeds; i++) {
        vec2f pos = mScreenMap[i];

        if (mInterpolationMode == BILINEAR) {
            context.leds[i] = sampleBilinear(mGrid.get(), xyMap, pos.x, pos.y);
        } else {
            context.leds[i] = sampleNearest(mGrid.get(), xyMap, pos.x, pos.y);
        }
    }
}

CRGB Fx2dTo1d::sampleBilinear(const CRGB *grid, const XYMap &xyMap, float x,
                              float y) const {
    int x0 = static_cast<int>(x);
    int y0 = static_cast<int>(y);
    int x1 = fl_min(x0 + 1, static_cast<int>(xyMap.getWidth()) - 1);
    int y1 = fl_min(y0 + 1, static_cast<int>(xyMap.getHeight()) - 1);

    // Clamp x0 and y0 to valid range
    x0 = fl_max(0, fl_min(x0, static_cast<int>(xyMap.getWidth()) - 1));
    y0 = fl_max(0, fl_min(y0, static_cast<int>(xyMap.getHeight()) - 1));

    float fx = x - x0;
    float fy = y - y0;

    // Get four neighboring pixels
    CRGB c00 = grid[xyMap.mapToIndex(x0, y0)];
    CRGB c10 = grid[xyMap.mapToIndex(x1, y0)];
    CRGB c01 = grid[xyMap.mapToIndex(x0, y1)];
    CRGB c11 = grid[xyMap.mapToIndex(x1, y1)];

    // Interpolate
    CRGB c0 = CRGB::blend(c00, c10, static_cast<u8>(fx * 255));
    CRGB c1 = CRGB::blend(c01, c11, static_cast<u8>(fx * 255));
    return CRGB::blend(c0, c1, static_cast<u8>(fy * 255));
}

CRGB Fx2dTo1d::sampleNearest(const CRGB *grid, const XYMap &xyMap, float x,
                             float y) const {
    int xi = static_cast<int>(x + 0.5f); // Round to nearest
    int yi = static_cast<int>(y + 0.5f);
    xi = fl_max(0, fl_min(xi, static_cast<int>(xyMap.getWidth()) - 1));
    yi = fl_max(0, fl_min(yi, static_cast<int>(xyMap.getHeight()) - 1));
    return grid[xyMap.mapToIndex(xi, yi)];
}

fl::string Fx2dTo1d::fxName() const {
    return fl::string("Fx2dTo1d(") + mFx2d->fxName() + ")";
}

} // namespace fl
