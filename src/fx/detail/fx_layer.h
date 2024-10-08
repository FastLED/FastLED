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
            frame = FramePtr::New(fx->getNumLeds() * (fx->hasAlphaChannel() ? 4 : 3));
        }

        if (!running) {
            // Clear the frame
            memset(frame->data(), 0, fx->getNumLeds() * (fx->hasAlphaChannel() ? 4 : 3));
            fx->resume();
            running = true;
        }
        Fx::DrawContext context = {now, reinterpret_cast<CRGB*>(frame->data())};
        if (fx->hasAlphaChannel()) {
            context.alpha_channel = frame->data() + fx->getNumLeds() * 3;
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

    CRGB *getSurface() { return reinterpret_cast<CRGB*>(frame->data()); }
    uint8_t *getSurfaceAlpha() { return fx->hasAlphaChannel() ? frame->data() + fx->getNumLeds() * 3 : nullptr; }

  private:
    Ptr<Frame> frame;
    Ptr<Fx> fx;
    bool running = false;
};

FASTLED_NAMESPACE_END
