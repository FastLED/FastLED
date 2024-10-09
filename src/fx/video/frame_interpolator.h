#pragma once

#include "fx/video/stream.h"
#include "fx/frame.h"
#include "namespace.h"
#include "fx/detail/circular_buffer.h"

FASTLED_NAMESPACE_BEGIN

DECLARE_SMART_PTR(FrameInterpolator);

class FrameInterpolator : public Referent {
public:
    typedef CircularBuffer<FramePtr> FrameBuffer;
    FrameInterpolator(size_t nframes);

    // Will search through the array, select the two frames that are closest to the current time
    // and then interpolate between them, storing the results in the provided frame.
    // The destination frame will have "now" as the current timestamp.
    // Returns true if the interpolation was successful, false otherwise. If false then
    // the destination frame will not be modified.
    bool draw(uint32_t now, Frame* dst);

    // Frame's resources are copied into the internal data structures.
    bool add(const Frame& frame);

    bool addWithTimestamp(const Frame& frame, uint32_t timestamp);

    // Clear all frames
    void clear() { mFrames.clear(); }

    // Selects the two frames that are closest to the current time. Returns false on failure.
    // frameMin will be before or equal to the current time, frameMax will be after.
    bool selectFrames(uint32_t now, const Frame** frameMin, const Frame** frameMax) const;

    FrameBuffer& getFrames() { return mFrames; }

private:
    FrameBuffer mFrames;
};

FASTLED_NAMESPACE_END

