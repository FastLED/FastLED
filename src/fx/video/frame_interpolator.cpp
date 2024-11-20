#include "fx/video/frame_interpolator.h"
#include "fx/detail/circular_buffer.h"
#include "fx/detail/data_stream.h"
#include "math_macros.h"
#include "namespace.h"

#ifdef FASTLED_DEBUG
#define DBG(X)(X)
#else
#define DBG(X)
#endif

#ifdef FASTLED_DEBUG
#include <iostream>  // ok include
using namespace std;
#endif


FASTLED_NAMESPACE_BEGIN

FrameInterpolator::FrameInterpolator(size_t nframes, float fps)
    : mFrames(MAX(1, nframes)), mFrameTracker(fps) {
}

bool FrameInterpolator::draw(uint32_t now, Frame *dst) {
    bool ok = draw(now, dst->rgb(), dst->alpha());
    if (ok) {
        // dst->setTimestamp(now);
    }
    return ok;
}

bool FrameInterpolator::draw(uint32_t now, CRGB* leds, uint8_t* alpha) {
    uint32_t frameNumber, nextFrameNumber;
    uint8_t amountOfNextFrame;
    mFrameTracker.get_interval_frames(now, &frameNumber, &nextFrameNumber, &amountOfNextFrame);
    if (!has(frameNumber)) {
        DBG("FrameInterpolator::draw: !has(frameNumber)");
        return false;
    }

    if (!has(nextFrameNumber)) {
        DBG("FrameInterpolator::draw: !has(frameNumber) || !has(nextFrameNumber)");
        return false;
    }

    if (has(frameNumber) && !has(nextFrameNumber)) {
        DBG("FrameInterpolator::draw: has(frameNumber) && !has(nextFrameNumber)");
        // just paint the current frame
        Frame* frame = get(frameNumber).get();
        frame->draw(leds, alpha);
        return true;
    }

    Frame* frame1 = get(frameNumber).get();
    Frame* frame2 = get(nextFrameNumber).get();
    Frame::interpolate(*frame1, *frame2, amountOfNextFrame, leds, alpha);
    return true;
}



FASTLED_NAMESPACE_END
