#pragma once

#include <stdint.h>
#include <string.h>

#include "crgb.h"
#include "fl/vector.h"
#include "fx/fx.h"
#include "namespace.h"
#include "fl/ptr.h"
#include "fx/frame.h"
#include "fl/warn.h"

//#include <assert.h>

FASTLED_NAMESPACE_BEGIN

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
            if (fx->hasAlphaChannel()) {
                FASTLED_WARN("Alpha channel not supported in FxLayer yet.");
            }
            fx->resume();
            running = true;
        }
        Fx::DrawContext context = {now, frame->rgb()};
        if (fx->hasAlphaChannel()) {
            FASTLED_WARN("Alpha channel not supported in FxLayer yet.");
        }
        fx->draw(context);
    }

    void pause() {
        if (fx && running) {
            fx->pause();
            running = false;
        }
    }

    void release() {
        pause();
        fx.reset();
    }

    fl::Ptr<Fx> getFx() { return fx; }

    CRGB *getSurface() { return frame->rgb(); }

  private:
    fl::Ptr<Frame> frame;
    fl::Ptr<Fx> fx;
    bool running = false;
};

FASTLED_NAMESPACE_END
