#pragma once

#include "fx/video/stream.h"
#include "fx/frame.h"
#include "namespace.h"
#include "fx/video/frame_interpolator.h"

FASTLED_NAMESPACE_BEGIN

DECLARE_SMART_PTR(VideoStreamBuffered);

class VideoStreamBuffered : public Referent {
public:
    VideoStreamBuffered(VideoStreamPtr stream, size_t nFramesInBuffer, float fpsVideo);
    bool draw(uint32_t now, Frame* frame);
    bool Rewind();

private:
    void fillBuffer();
    uint64_t mMicrosSecondsPerFrame;
    VideoStreamPtr mStream;
    size_t mNFrames;
    FrameInterpolatorPtr mInterpolator;
    uint32_t mLastDrawTime;
};

FASTLED_NAMESPACE_END

