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
#endif

using namespace std;

FASTLED_NAMESPACE_BEGIN

FrameInterpolator::FrameInterpolator(size_t nframes, float fps)
    : mFrames(MAX(1, nframes)), mFrameTracker(fps) {
}

bool FrameInterpolator::draw(uint32_t now, Frame *dst) {
    bool ok = draw(now, dst->rgb(), dst->alpha(), &now);
    if (ok) {
        // dst->setTimestamp(now);
    }
    return ok;
}

bool FrameInterpolator::draw(uint32_t now, CRGB* leds, uint8_t* alpha, uint32_t* precise_timestamp) {
    const Frame *frameMin = nullptr;
    const Frame *frameMax = nullptr;
    if (!selectFrames(now, &frameMin, &frameMax)) {
        DBG(cout << "FrameInterpolator::draw: !selectFrames" << endl);
        return false;
    }
    if (!frameMin || !frameMax) {
        // we should not be here
        DBG(cout << "FrameInterpolator::draw: !frameMin || !frameMax" << endl);
        return false;
    }
    // Calculate interpolation factor
    uint32_t total_duration = frameMax->getTimestamp() - frameMin->getTimestamp();
    if (frameMin == frameMax || total_duration == 0) {
        DBG(cout << "FrameInterpolator::draw: " << frameMin << "==" << frameMax << ", " << "total_duration" << total_duration << endl);
        // There is only one frame, so just copy it
        frameMax->draw(leds, alpha);
        if (precise_timestamp) {
            *precise_timestamp = frameMax->getTimestamp();
        }
        DBG(cout << "FrameInterpolator::draw: frameMax->getTimestamp(): " << frameMax->getTimestamp() << endl);
        return true;
    }
    uint32_t elapsed = now - frameMin->getTimestamp();
    uint8_t progress = (elapsed * 255) / total_duration;
    // Interpolate between the two frames
    DBG(cout << "FrameInterpolator::draw: progress: " << progress << endl);
    Frame::interpolate(*frameMin, *frameMax, progress, leds, alpha);
    if (precise_timestamp) {
        *precise_timestamp = frameMin->getTimestamp() + elapsed;
    }
    return true;
}




bool FrameInterpolator::selectFrames(uint32_t now, const Frame **frameMin,
                                     const Frame **frameMax) const {
    if (mFrames.empty()) {
        *frameMin = *frameMax = nullptr;
        DBG(cout << "mFrames.empty()" << endl);
        return false;
    }

    if (mFrames.size() == 1) {
        *frameMin = *frameMax = mFrames.front().frame.get();
        DBG(cout << "mFrames.size() == 1" << endl);
        return true;
    }

    // handle case before first timestamp
    auto& back_frame = mFrames.back().frame;
    if (now <= back_frame->getTimestamp())
    {
        *frameMin = back_frame.get();
        *frameMax = back_frame.get();
        DBG(cout << "A: " << endl);
        return true;
    }

    // Handle case after the last frame
    auto& front_frame = mFrames.front().frame;
    if (now >= front_frame->getTimestamp()) {
        Frame* cur = front_frame.get();
        *frameMin = cur;
        *frameMax = cur;
        return true;
    }

    uint32_t frameNumber, nextFrameNumber;
    uint8_t amountOfNextFrame;

    mFrameTracker.get_interval_frames(now, &frameNumber, &nextFrameNumber, &amountOfNextFrame);
    DBG(cout << "frameNumber: " << frameNumber << ", nextFrameNumber: " << nextFrameNumber << ", amountOfNextFrame: " << int(amountOfNextFrame) << endl);
    if (!has(frameNumber) || !has(nextFrameNumber)) {
        return false;
    }
    *frameMin = get(frameNumber).get();
    *frameMax = get(nextFrameNumber).get();
    return true;
}

FASTLED_NAMESPACE_END
