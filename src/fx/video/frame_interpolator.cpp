#include "fx/video/frame_interpolator.h"
#include "fx/detail/circular_buffer.h"
#include "fx/video/stream.h"
#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

FrameInterpolator::FrameInterpolator(size_t nframes) : mFrames(nframes) {}

bool FrameInterpolator::draw(uint32_t now, Frame *dst) {
    const Frame *frame1 = nullptr;
    const Frame *frame2 = nullptr;

    if (!selectFrames(now, &frame1, &frame2)) {
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

bool FrameInterpolator::addWithTimestamp(const Frame &frame,
                                         uint32_t timestamp) {
    // Check if the new frame's timestamp is newer than all existing frames
    if (!mFrames.empty() && timestamp <= mFrames.back()->getTimestamp()) {
        return false; // Reject the frame if it's not newer than the newest
                      // frame
    }

    FramePtr newFrame;
    if (mFrames.full()) {
        // Reuse the oldest frame
        bool ok = mFrames.pop_back(&newFrame);
        if (!ok) {
            // Something unexpected happened. This should never occur.
            return false;
        }
    } else {
        // Allocate a new frame
        newFrame = FramePtr::New(frame.size(), !!frame.alpha());
    }
    newFrame->copy(frame);
    newFrame->setTimestamp(timestamp);

    // Insert the new frame at the front
    mFrames.push_front(newFrame);

    return true;
}

bool FrameInterpolator::add(const Frame &frame) {
    return addWithTimestamp(frame, frame.getTimestamp());
}

bool FrameInterpolator::selectFrames(uint32_t now, const Frame **frame1,
                                     const Frame **frame2) const {
    if (mFrames.empty()) {
        *frame1 = *frame2 = nullptr;
        return false;
    }

    if (mFrames.size() == 1) {
        *frame1 = *frame2 = mFrames.front().get();
        return true;
    }

    // Handle case before the first frame
    if (now <= mFrames.front()->getTimestamp()) {
        *frame1 = *frame2 = mFrames.front().get();
        return true;
    }

    // Handle case after the last frame
    if (now >= mFrames.back()->getTimestamp()) {
        *frame1 = *frame2 = mFrames.back().get();
        return true;
    }

    // Find the two frames that bracket the given timestamp
    for (size_t i = 0; i < mFrames.size() - 1; ++i) {
        if (mFrames[i]->getTimestamp() <= now && mFrames[i + 1]->getTimestamp() > now) {
            *frame1 = mFrames[i].get();
            *frame2 = mFrames[i + 1].get();
            return true;
        }
    }

    // This should never happen if all previous conditions are correct
    return false;
}

FASTLED_NAMESPACE_END
