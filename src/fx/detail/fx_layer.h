#pragma once


#include "fl/stdint.h"
#include "crgb.h"
#include "fl/namespace.h"
#include "fl/memory.h"
#include "fl/vector.h"
#include "fl/warn.h"
#include "fx/frame.h"
#include "fx/fx.h"


namespace fl {

FASTLED_SMART_PTR(FxLayer);
class FxLayer {
  public:
    void setFx(fl::shared_ptr<Fx> newFx);

    void draw(fl::u32 now);

    void pause(fl::u32 now);

    void release();

    fl::shared_ptr<Fx> getFx();

    CRGB *getSurface();

  private:
    fl::shared_ptr<Frame> frame;
    fl::shared_ptr<Fx> fx;
    bool running = false;
};

} // namespace fl
