/*
Fx2d class that allows to blend multiple Fx2d layers together.
The bottom layer is always drawn at full capacity. Upper layers
are blended by the the max luminance of the components.
*/

#pragma once

#include <stdint.h>

#include "fl/namespace.h"
#include "fl/ptr.h"
#include "fl/vector.h"
#include "fl/warn.h"
#include "fl/xymap.h"
#include "fx/frame.h"
#include "fx/fx.h"
#include "fx/fx2d.h"

namespace fl {

class Blend2d : public Fx2d {
  public:
    // Note that if this xymap is non rectangular then it's recommended that the
    // Fx2d layers that are added should be rectangular.
    Blend2d(const XYMap &xymap);
    fl::Str fxName() const override;
    void add(Fx2dPtr layer);
    void add(Fx2d &layer);
    void draw(DrawContext context) override;
    void clear();
    void setBlurAmount(uint8_t blur_amount) {
        mBlurAmount = blur_amount;
    }
    void setBlurPasses(uint8_t blur_passes) {
        mBlurPasses = blur_passes;
    }
  protected:
    HeapVector<Fx2dPtr> mLayers;
    FramePtr mFrame;
    FramePtr mFrameTransform;
    uint8_t mBlurAmount = 0;
    uint8_t mBlurPasses = 1;
};

} // namespace fl
