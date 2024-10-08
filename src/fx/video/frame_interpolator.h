#pragma once

#include "fx/video/stream.h"
#include "fx/frame.h"
#include "namespace.h"
#include "fx/detail/circular_buffer.h"

FASTLED_NAMESPACE_BEGIN

DECLARE_SMART_PTR(FrameInterpolator);

class FrameInterpolator : public Referent {
public:

    FrameInterpolator(size_t nframes);

    // Will search through the array, select the two frames that are closest to the current time
    // and then interpolate between them, storing the results in the provided frame.
    bool draw(uint32_t now, Frame* dst);

    // Frame's resources are copied into the internal data structures. The provided
    // timestamp will be set to the frame.
    void add(uint32_t timestamp, const Frame& frame);

private:
    CircularBuffer<FramePtr> mFrames;
};

FASTLED_NAMESPACE_END

