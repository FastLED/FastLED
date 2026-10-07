#pragma once

#include "fl/fx/frame.h"
#include "fl/stl/flat_map.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/span.h"
#include "fl/fx/frame.h"
#include "fl/video/frame_tracker.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace video {

FASTLED_SHARED_PTR(FrameInterpolator);

// Holds onto frames and allow interpolation. This allows
// effects to have high effective frame rate and also
// respond to things like sound which can modify the timing.
class FrameInterpolator {
  public:
    struct Less {
        bool operator()(fl::u32 a, fl::u32 b) const FL_NO_EXCEPT { return a < b; }
    };
    typedef fl::flat_map<fl::u32, FramePtr, Less> FrameBuffer;
    FrameInterpolator(size_t nframes, float fpsVideo) FL_NO_EXCEPT;

    // Will search through the array, select the two frames that are closest to
    // the current time and then interpolate between them, storing the results
    // in the provided frame. The destination frame will have "now" as the
    // current timestamp if and only if there are two frames that can be
    // interpolated. Else it's set to the timestamp of the frame that was
    // selected. Returns true if the interpolation was successful, false
    // otherwise. If false then the destination frame will not be modified. Note
    // that this adjustable_time is allowed to go pause or go backward in time.
    bool draw(fl::u32 adjustable_time, Frame *dst) FL_NO_EXCEPT;
    bool draw(fl::u32 adjustable_time, fl::span<CRGB> leds) FL_NO_EXCEPT;
    bool insert(fl::u32 frameNumber, FramePtr frame) FL_NO_EXCEPT {
        FrameBuffer::insert_result result;
        mFrames.insert(frameNumber, frame, &result);
        return result != FrameBuffer::at_capacity;
    }

    // Clear all frames
    void clear() FL_NO_EXCEPT { mFrames.clear(); }

    bool empty() const FL_NO_EXCEPT { return mFrames.empty(); }

    bool has(fl::u32 frameNum) const FL_NO_EXCEPT { return mFrames.has(frameNum); }

    FramePtr erase(fl::u32 frameNum) FL_NO_EXCEPT {
        FramePtr out;
        auto it = mFrames.find(frameNum);
        if (it == mFrames.end()) {
            return out;
        }
        out = it->second;
        mFrames.erase(it);
        return out;
    }

    FramePtr get(fl::u32 frameNum) const FL_NO_EXCEPT {
        auto it = mFrames.find(frameNum);
        if (it != mFrames.end()) {
            return it->second;
        }
        return FramePtr();
    }

    bool full() const FL_NO_EXCEPT { return mFrames.full(); }
    size_t capacity() const FL_NO_EXCEPT { return mFrames.capacity(); }

    FrameBuffer *getFrames() FL_NO_EXCEPT { return &mFrames; }

    bool needsFrame(fl::u32 now, fl::u32 *currentFrameNumber,
                    fl::u32 *nextFrameNumber) const FL_NO_EXCEPT {
        mFrameTracker.get_interval_frames(now, currentFrameNumber,
                                          nextFrameNumber);
        return !has(*currentFrameNumber) || !has(*nextFrameNumber);
    }

    bool get_newest_frame_number(fl::u32 *frameNumber) const FL_NO_EXCEPT {
        if (mFrames.empty()) {
            return false;
        }
        auto &front = mFrames.back();
        *frameNumber = front.first;
        return true;
    }

    bool get_oldest_frame_number(fl::u32 *frameNumber) const FL_NO_EXCEPT {
        if (mFrames.empty()) {
            return false;
        }
        auto &front = mFrames.front();
        *frameNumber = front.first;
        return true;
    }

    fl::u32 get_exact_timestamp_ms(fl::u32 frameNumber) const FL_NO_EXCEPT {
        return mFrameTracker.get_exact_timestamp_ms(frameNumber);
    }

    FrameTracker &getFrameTracker() FL_NO_EXCEPT { return mFrameTracker; }

  private:
    FrameBuffer mFrames;
    FrameTracker mFrameTracker;
};

} // namespace video
} // namespace fl
