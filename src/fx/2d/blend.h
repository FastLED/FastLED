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
    void add(Fx2dPtr layer, uint8_t blur_amount = 0, uint8_t blur_passes = 1);
    void add(Fx2d &layer, uint8_t blur_amount = 0, uint8_t blur_passes = 1);
    void draw(DrawContext context) override;
    void clear();
    void setGlobalBlurAmount(uint8_t blur_amount) {
        mGlobalBlurAmount = blur_amount;
    }
    void setGlobalBlurPasses(uint8_t blur_passes) {
        mGlobalBlurPasses = blur_passes;
    }
    bool setBlurParams(Fx2dPtr fx, uint8_t blur_amount, uint8_t blur_passes);
    bool setBlurParams(Fx2d &fx, uint8_t blur_amount, uint8_t blur_passes);
  protected:
    struct Entry {
        Fx2dPtr fx;
        uint8_t blur_amount = 0;
        uint8_t blur_passes = 1;
    };
    HeapVector<Entry> mLayers;
    FramePtr mFrame;
    FramePtr mFrameTransform;
    uint8_t mGlobalBlurAmount = 0;
    uint8_t mGlobalBlurPasses = 1;
};

} // namespace fl
