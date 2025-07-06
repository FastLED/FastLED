#pragma once


#include "fl/stdint.h"
#include "crgb.h"
#include "fl/namespace.h"
#include "fl/ptr.h"
#include "fl/vector.h"
#include "fl/warn.h"
#include "fx/frame.h"
#include "fx/fx.h"


namespace fl {

FASTLED_SMART_PTR(FxLayer);
class FxLayer : public fl::Referent {
  public:
    void setFx(fl::Ptr<Fx> newFx);

    void draw(fl::u32 now);

    void pause(fl::u32 now);

    void release();

    fl::Ptr<Fx> getFx();

    CRGB *getSurface();

  private:
    fl::Ptr<Frame> frame;
    fl::Ptr<Fx> fx;
    bool running = false;
};

} // namespace fl
