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
    Blend2d(const XYMap &xymap);
    fl::Str fxName() const override;
    void add(Fx2dPtr layer);
    void add(Fx2d &layer);
    virtual void draw(DrawContext context) override;
    void clear();
  protected:
    HeapVector<Fx2dPtr> mLayers;
    FramePtr mFrame;
    FramePtr mFrameTransform;
};

} // namespace fl
