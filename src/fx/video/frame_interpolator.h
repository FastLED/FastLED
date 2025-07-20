#pragma once

#include "fl/map.h"
#include "fl/namespace.h"
#include "fx/frame.h"
#include "fx/video/frame_tracker.h"
#include "fx/video/pixel_stream.h"

namespace fl {

FASTLED_SMART_PTR(FrameInterpolator);

// Holds onto frames and allow interpolation. This allows
// effects to have high effective frame rate and also
// respond to things like sound which can modify the timing.
class FrameInterpolator {
  public:
    struct Less {
        bool operator()(fl::u32 a, fl::u32 b) const { return a < b; }
    };
    typedef fl::SortedHeapMap<fl::u32, FramePtr, Less> FrameBuffer;
    FrameInterpolator(size_t nframes, float fpsVideo);

    // Will search through the array, select the two frames that are closest to
    // the current time and then interpolate between them, storing the results
    // in the provided frame. The destination frame will have "now" as the
    // current timestamp if and only if there are two frames that can be
    // interpolated. Else it's set to the timestamp of the frame that was
    // selected. Returns true if the interpolation was successful, false
    // otherwise. If false then the destination frame will not be modified. Note
    // that this adjustable_time is allowed to go pause or go backward in time.
    bool draw(fl::u32 adjustable_time, Frame *dst);
    bool draw(fl::u32 adjustable_time, CRGB *leds);
    bool insert(fl::u32 frameNumber, FramePtr frame) {
        InsertResult result;
        mFrames.insert(frameNumber, frame, &result);
        return result != InsertResult::kMaxSize;
    }

    // Clear all frames
    void clear() { mFrames.clear(); }

    bool empty() const { return mFrames.empty(); }

    bool has(fl::u32 frameNum) const { return mFrames.has(frameNum); }

    FramePtr erase(fl::u32 frameNum) {
        FramePtr out;
        auto it = mFrames.find(frameNum);
        if (it == mFrames.end()) {
            return out;
        }
        out = it->second;
        mFrames.erase(it);
        return out;
    }

    FramePtr get(fl::u32 frameNum) const {
        auto it = mFrames.find(frameNum);
        if (it != mFrames.end()) {
            return it->second;
        }
        return FramePtr();
    }

    bool full() const { return mFrames.full(); }
    size_t capacity() const { return mFrames.capacity(); }

    FrameBuffer *getFrames() { return &mFrames; }

    bool needsFrame(fl::u32 now, fl::u32 *currentFrameNumber,
                    fl::u32 *nextFrameNumber) const {
        mFrameTracker.get_interval_frames(now, currentFrameNumber,
                                          nextFrameNumber);
        return !has(*currentFrameNumber) || !has(*nextFrameNumber);
    }

    bool get_newest_frame_number(fl::u32 *frameNumber) const {
        if (mFrames.empty()) {
            return false;
        }
        auto &front = mFrames.back();
        *frameNumber = front.first;
        return true;
    }

    bool get_oldest_frame_number(fl::u32 *frameNumber) const {
        if (mFrames.empty()) {
            return false;
        }
        auto &front = mFrames.front();
        *frameNumber = front.first;
        return true;
    }

    fl::u32 get_exact_timestamp_ms(fl::u32 frameNumber) const {
        return mFrameTracker.get_exact_timestamp_ms(frameNumber);
    }

    FrameTracker &getFrameTracker() { return mFrameTracker; }

  private:
    FrameBuffer mFrames;
    FrameTracker mFrameTracker;
};

} // namespace fl
