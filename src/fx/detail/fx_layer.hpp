

#include <string.h>

#include "fx_layer.h"


\
namespace fl {

void FxLayer::setFx(fl::Ptr<Fx> newFx) {
    if (newFx != fx) {
        release();
        fx = newFx;
    }
}

void FxLayer::draw(uint32_t now) {
    // assert(fx);
    if (!frame) {
        frame = FramePtr::New(fx->getNumLeds());
    }

    if (!running) {
        // Clear the frame
        memset((uint8_t*)frame->rgb(), 0, frame->size() * sizeof(CRGB));
        fx->resume(now);
        running = true;
    }
    Fx::DrawContext context = {now, frame->rgb()};
    fx->draw(context);
}

void FxLayer::pause(uint32_t now) {
    if (fx && running) {
        fx->pause(now);
        running = false;
    }
}

void FxLayer::release() {
    pause(0);
    fx.reset();
}

fl::Ptr<Fx> FxLayer::getFx() { 
    return fx; 
}

CRGB* FxLayer::getSurface() { 
    return frame->rgb(); 
}

}