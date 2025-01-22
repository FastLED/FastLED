#include "fx/video/frame_interpolator.h"
#include "fl/circular_buffer.h"
#include "fx/video/pixel_stream.h"
#include "fl/math_macros.h"
#include "fl/namespace.h"

#include "fl/dbg.h"

#include "fl/math_macros.h"
#include <math.h>

#define DBG FASTLED_DBG


namespace fl {

FrameInterpolator::FrameInterpolator(size_t nframes, float fps)
    : mFrameTracker(fps) {
    size_t capacity = MAX(1, nframes);
    mFrames.setMaxSize(capacity);
}

bool FrameInterpolator::draw(uint32_t now, Frame *dst) {
    bool ok = draw(now, dst->rgb());
    return ok;
}

bool FrameInterpolator::draw(uint32_t now, CRGB* leds) {
    uint32_t frameNumber, nextFrameNumber;
    uint8_t amountOfNextFrame;
    // DBG("now: " << now);
    mFrameTracker.get_interval_frames(now, &frameNumber, &nextFrameNumber, &amountOfNextFrame);
    if (!has(frameNumber)) {
        return false;
    }

    if (has(frameNumber) && !has(nextFrameNumber)) {
        // just paint the current frame
        Frame* frame = get(frameNumber).get();
        frame->draw(leds);
        return true;
    }

    Frame* frame1 = get(frameNumber).get();
    Frame* frame2 = get(nextFrameNumber).get();

    Frame::interpolate(*frame1, *frame2, amountOfNextFrame, leds);
    return true;
}

}  // namespace fl
