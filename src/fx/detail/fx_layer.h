#pragma once

#include <stdint.h>
#include <string.h>

#include "crgb.h"
#include "fl/vector.h"
#include "fx/fx.h"
#include "fl/namespace.h"
#include "fl/ptr.h"
#include "fx/frame.h"
#include "fl/warn.h"

//#include <assert.h>

namespace fl {

FASTLED_SMART_PTR(FxLayer);
class FxLayer : public fl::Referent {
  public:
    void setFx(fl::Ptr<Fx> newFx) {
        if (newFx != fx) {
            release();
            fx = newFx;
        }
    }

    void draw(uint32_t now) {
        //assert(fx);
        if (!frame) {
            frame = FramePtr::New(fx->getNumLeds());
        }

        if (!running) {
            // Clear the frame
            memset(frame->rgb(), 0, frame->size() * sizeof(CRGB));
            fx->resume(now);
            running = true;
        }
        Fx::DrawContext context = {now, frame->rgb()};
        fx->draw(context);
    }

    void pause(uint32_t now) {
        if (fx && running) {
            fx->pause(now);
            running = false;
        }
    }

    void release() {
        pause(0);
        fx.reset();
    }

    fl::Ptr<Fx> getFx() { return fx; }

    CRGB *getSurface() { return frame->rgb(); }

  private:
    fl::Ptr<Frame> frame;
    fl::Ptr<Fx> fx;
    bool running = false;
};

}  // namespace fl
