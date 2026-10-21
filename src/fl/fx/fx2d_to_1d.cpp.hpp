#include "fl/fx/fx2d_to_1d.h"

#include "fl/gfx/sample.h"
#include "fl/math/xymap.h"
#include "crgb.h"
#include "fl/stl/allocator.h"
#include "fl/stl/noexcept.h"

namespace fl {

Fx2dTo1d::Fx2dTo1d(u16 numLeds, Fx2dPtr fx2d, const ScreenMap &screenMap,
                   InterpolationMode mode)
    FL_NO_EXCEPT : Fx1d(numLeds), mFx2d(fx2d), mScreenMap(screenMap),
      mInterpolationMode(mode), mGrid(new CRGB[fx2d->getNumLeds()]) {}  // ok bare allocation (array new)

void Fx2dTo1d::setFx2d(Fx2dPtr fx2d) FL_NO_EXCEPT {
    mFx2d = fx2d;
    // Reallocate grid buffer if needed
    mGrid.reset(new CRGB[fx2d->getNumLeds()]);  // ok bare allocation (array new)
}

void Fx2dTo1d::draw(DrawContext context) FL_NO_EXCEPT {
    // Step 1: Render 2D effect to internal grid
    DrawContext grid_context(context.now, fl::span<CRGB>(mGrid.get(), mFx2d->getNumLeds()));
    mFx2d->draw(grid_context);

    // Step 2: Sample from grid to 1D output using fl::sample
    const XYMap &xyMap = mFx2d->getXYMap();
    SampleMode mode = static_cast<SampleMode>(mInterpolationMode);

    for (u16 i = 0; i < mNumLeds; i++) {
        vec2f pos = mScreenMap[i];
        context.leds[i] = fl::sample(mGrid.get(), xyMap, pos.x, pos.y, mode);
    }
}

fl::string Fx2dTo1d::fxName() const FL_NO_EXCEPT {
    return fl::string("Fx2dTo1d(") + mFx2d->fxName() + ")";
}

} // namespace fl
