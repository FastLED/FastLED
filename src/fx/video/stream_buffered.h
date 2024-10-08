#pragma once

#include "fx/video/stream.h"
#include "fx/frame.h"
#include "namespace.h"
#include "fx/detail/circular_buffer.h"

FASTLED_NAMESPACE_BEGIN

DECLARE_SMART_PTR(VideoStreamBuffered);

class VideoStreamBuffered : public Referent {
public:
    struct TimestampedFrame {
        uint32_t timestamp;
        FramePtr frame;
    };
    VideoStreamBuffered(VideoStreamPtr stream, size_t nframes);
    bool draw(uint32_t now, Frame* frame);
    int32_t FramesRemaining() const;
    bool Rewind();

private:
    void fillBuffer();
    VideoStreamPtr mStream;
    size_t mNFrames;
    CircularBuffer<TimestampedFrame> mFrames;
};

FASTLED_NAMESPACE_END

