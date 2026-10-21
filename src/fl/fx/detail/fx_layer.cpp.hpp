

#include "fl/fx/detail/fx_layer.h"

#include "crgb.h"
#include "fl/fx/frame.h"
#include "fl/fx/fx.h"
#include "fl/stl/cstring.h"
#include "fl/stl/noexcept.h"

namespace fl {

void FxLayer::setFx(fl::shared_ptr<Fx> newFx) FL_NO_EXCEPT {
    if (newFx != fx) {
        release();
        fx = newFx;
    }
}

void FxLayer::draw(fl::u32 now, float speed, const AudioBatch *audio) FL_NO_EXCEPT {
    // assert(fx);
    if (!frame) {
        frame = fl::make_shared<Frame>(fx->getNumLeds());
    }

    if (!running) {
        // Clear the frame
        fl::memset((u8*)frame->rgb().data(), 0, frame->size() * sizeof(CRGB));
        fx->resume(now);
        mLastNow = now;
        running = true;
    }
    u16 frame_time = static_cast<u16>(now - mLastNow);
    mLastNow = now;
    Fx::DrawContext context(now, frame->rgb(), frame_time, speed, audio);
    fx->draw(context);
}

void FxLayer::pause(fl::u32 now) FL_NO_EXCEPT {
    if (fx && running) {
        fx->pause(now);
        running = false;
    }
}

void FxLayer::release() FL_NO_EXCEPT {
    pause(0);
    fx.reset();
}

fl::shared_ptr<Fx> FxLayer::getFx() FL_NO_EXCEPT {
    return fx; 
}

fl::span<CRGB> FxLayer::getSurface() FL_NO_EXCEPT {
    return frame->rgb();
}

}
