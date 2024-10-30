#pragma once

#include <stdint.h>
#include <string.h>

#include "crgb.h"
#include "fixed_vector.h"
#include "fx/fx.h"
#include "namespace.h"
#include "ref.h"
#include "fx/frame.h"

//#include <assert.h>

FASTLED_NAMESPACE_BEGIN

FASTLED_SMART_REF(FxLayer);
class FxLayer : public Referent {
  public:
    void setFx(Ref<Fx> newFx) {
        if (newFx != fx) {
            release();
            fx = newFx;
        }
    }

    void draw(uint32_t now) {
        //assert(fx);
        if (!frame) {
            frame = FrameRef::New(fx->getNumLeds());
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

    Ref<Fx> getFx() { return fx; }

    CRGB *getSurface() { return frame->rgb(); }
    uint8_t *getSurfaceAlpha() {
        return fx->hasAlphaChannel() ? frame->alpha() : nullptr;
    }

  private:
    Ref<Frame> frame;
    Ref<Fx> fx;
    bool running = false;
};

FASTLED_NAMESPACE_END
