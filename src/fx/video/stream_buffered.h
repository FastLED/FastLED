#pragma once

#include "fx/video/stream.h"
#include "fx/frame.h"
#include "namespace.h"
#include "fx/video/frame_interpolator.h"

FASTLED_NAMESPACE_BEGIN

DECLARE_SMART_PTR(VideoStreamBuffered);

class VideoStreamBuffered : public Referent {
public:
    VideoStreamBuffered(size_t pixelsPerFrame, size_t nFramesInBuffer, float fpsVideo);
    void begin(uint32_t now, FileHandlePtr h);
    void beginStream(uint32_t now, ByteStreamPtr s);
    bool draw(uint32_t now, Frame* frame);
    bool Rewind();

private:
    void fillBufferIfNecessary(uint32_t now);
    uint32_t mPixelsPerFrame = 0;
    uint32_t mFrameCounter = 0;
    uint32_t mStartTime = 0;
    uint64_t mMicrosSecondsPerFrame;
    VideoStreamPtr mStream;
    size_t mNFrames;
    FrameInterpolatorPtr mInterpolator;
};

FASTLED_NAMESPACE_END

