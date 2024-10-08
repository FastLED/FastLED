#pragma once

#include "fx/video/stream.h"
#include "fx/frame.h"
#include "namespace.h"
#include "fx/detail/circular_buffer.h"

FASTLED_NAMESPACE_BEGIN

DECLARE_SMART_PTR(VideoStreamBuffered);

class VideoStreamBuffered : public Referent {
public:
    VideoStreamBuffered(VideoStreamPtr stream, size_t nframes);
    bool readFrame(Frame* frame);
    int32_t FramesRemaining() const;
    bool Rewind();

private:
    void fillBuffer();
    VideoStreamPtr mStream;
    size_t mNFrames;
    CircularBuffer<FramePtr> mFrames;
};

FASTLED_NAMESPACE_END

