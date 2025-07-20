

#include <string.h>

#include "fx_layer.h"


#include "fl/memfill.h"

namespace fl {

void FxLayer::setFx(fl::shared_ptr<Fx> newFx) {
    if (newFx != fx) {
        release();
        fx = newFx;
    }
}

void FxLayer::draw(fl::u32 now) {
    // assert(fx);
    if (!frame) {
        frame = fl::make_shared<Frame>(fx->getNumLeds());
    }

    if (!running) {
        // Clear the frame
        fl::memfill((uint8_t*)frame->rgb(), 0, frame->size() * sizeof(CRGB));
        fx->resume(now);
        running = true;
    }
    Fx::DrawContext context = {now, frame->rgb()};
    fx->draw(context);
}

void FxLayer::pause(fl::u32 now) {
    if (fx && running) {
        fx->pause(now);
        running = false;
    }
}

void FxLayer::release() {
    pause(0);
    fx.reset();
}

fl::shared_ptr<Fx> FxLayer::getFx() { 
    return fx; 
}

CRGB* FxLayer::getSurface() { 
    return frame->rgb(); 
}

}
