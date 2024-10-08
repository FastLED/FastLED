#pragma once

#include "crgb.h"
#include "fixed_vector.h"
#include "fx/fx.h"
#include "namespace.h"
#include "ptr.h"
#include "fx/frame.h"
#include <stdint.h>
#include <string.h>
//#include <assert.h>

FASTLED_NAMESPACE_BEGIN

DECLARE_SMART_PTR(FxLayer);
class FxLayer : public Referent {
  public:
    void setFx(Ptr<Fx> newFx) {
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
                memset(frame->alpha(), 0, frame->size());
            }
            fx->resume();
            running = true;
        }
        Fx::DrawContext context = {now, frame->rgb()};
        if (fx->hasAlphaChannel()) {
            context.alpha_channel = frame->alpha();
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

    Ptr<Fx> getFx() { return fx; }

    CRGB *getSurface() { return frame->rgb(); }
    uint8_t *getSurfaceAlpha() {
        return fx->hasAlphaChannel() ? frame->alpha() : nullptr;
    }

  private:
    Ptr<Frame> frame;
    Ptr<Fx> fx;
    bool running = false;
};

FASTLED_NAMESPACE_END
