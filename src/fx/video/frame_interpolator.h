#pragma once

#include "fx/detail/data_stream.h"
#include "fx/frame.h"
#include "namespace.h"
#include "fx/detail/circular_buffer.h"

FASTLED_NAMESPACE_BEGIN

DECLARE_SMART_PTR(FrameInterpolator);

// Holds onto frames and allow interpolation. This allows
// effects to have high effective frame rate and also
// respond to things like sound which can modify the timing.
class FrameInterpolator : public Referent {
public:
    typedef CircularBuffer<FramePtr> FrameBuffer;
    FrameInterpolator(size_t nframes);

    // Will search through the array, select the two frames that are closest to the current time
    // and then interpolate between them, storing the results in the provided frame.
    // The destination frame will have "now" as the current timestamp if and only if
    // there are two frames that can be interpolated. Else it's set to the timestamp of the
    // frame that was selected.
    // Returns true if the interpolation was successful, false otherwise. If false then
    // the destination frame will not be modified.
    // Note that this adjustable_time is allowed to go pause or go backward in time. 
    bool draw(uint32_t adjustable_time, Frame* dst);

    // Frame's resources are copied into the internal data structures.
    bool add(const Frame& frame);

    // Used for recycling externally.
    bool pop_back(FramePtr* dst) { return mFrames.pop_back(dst); }
    bool push_front(FramePtr frame, uint32_t timestamp) {
        if (mFrames.full()) {
            return false;
        }
        if (!mFrames.empty() && timestamp <= mFrames.front()->getTimestamp()) {
            return false;
        }
        frame->setTimestamp(timestamp);
        return mFrames.push_front(frame);
    }

    bool addWithTimestamp(const Frame& frame, uint32_t timestamp);

    // Clear all frames
    void clear() { mFrames.clear(); }

    // Selects the two frames that are closest to the current time. Returns false on failure.
    // frameMin will be before or equal to the current time, frameMax will be after.
    bool selectFrames(uint32_t now, const Frame** frameMin, const Frame** frameMax) const;

    FrameBuffer* getFrames() { return &mFrames; }

private:
    FrameBuffer mFrames;
};

FASTLED_NAMESPACE_END

