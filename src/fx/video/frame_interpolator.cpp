#include "fx/video/stream.h"
#include "fx/video/frame_interpolator.h"
#include "fx/detail/circular_buffer.h"
#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

FrameInterpolator::FrameInterpolator(size_t nframes)
    : mFrames(nframes) {}

bool FrameInterpolator::draw(uint32_t now, Frame* dst) {
    if (mFrames.size() < 2) {
        return false;
    }

    const Frame* frame1 = nullptr;
    const Frame* frame2 = nullptr;

    // Find the two frames closest to the current time
    for (size_t i = 0; i < mFrames.size(); ++i) {
        const FramePtr& frame = mFrames[i];
        if (frame->getTimestamp() <= now) {
            frame1 = frame.get();
        } else {
            frame2 = frame.get();
            break;
        }
    }

    if (!frame1 || !frame2) {
        return false;
    }

    // Calculate interpolation factor
    uint32_t total_duration = frame2->getTimestamp() - frame1->getTimestamp();
    uint32_t elapsed = now - frame1->getTimestamp();
    uint8_t progress = (elapsed * 255) / total_duration;

    // Interpolate between the two frames
    dst->interpolate(*frame1, *frame2, progress);

    return true;
}

bool FrameInterpolator::add(const Frame& frame, uint32_t timestamp) {
    if (timestamp == 0) {
        timestamp = frame.getTimestamp();
    }

    // Check if the new frame's timestamp is newer than all existing frames
    if (!mFrames.empty() && timestamp <= mFrames.back()->getTimestamp()) {
        return false;  // Reject the frame if it's not newer than the newest frame
    }

    FramePtr newFrame;
    if (mFrames.full()) {
        // Reuse the oldest frame
        newFrame = mFrames.back();
        mFrames.pop_back();
        newFrame->copy(frame);
    } else {
        // Allocate a new frame
        newFrame = FramePtr::New(frame.size(), !!frame.alpha());
    }
    newFrame->setTimestamp(timestamp);

    // Insert the new frame at the front (it's guaranteed to be the newest)
    mFrames.push_front(newFrame);

    return true;
}

FASTLED_NAMESPACE_END

