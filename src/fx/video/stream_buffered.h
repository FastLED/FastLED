#pragma once

#include "fx/detail/data_stream.h"
#include "fx/frame.h"
#include "namespace.h"
#include "fx/video/frame_interpolator.h"

FASTLED_NAMESPACE_BEGIN

DECLARE_SMART_PTR(VideoStream);

class VideoStream : public Referent {
public:
    VideoStream(size_t pixelsPerFrame, size_t nFramesInBuffer, float fpsVideo);
    void begin(uint32_t now, FileHandlePtr h);
    void beginStream(uint32_t now, ByteStreamPtr s);
    void end();
    bool draw(uint32_t now, Frame* frame);
    bool draw(uint32_t now, CRGB* leds, uint8_t* alpha);
    bool Rewind();

    bool full() const {
        return mInterpolator->getFrames()->full();
    }

    FramePtr popOldest() {
        FramePtr frame;
        mInterpolator->pop_back(&frame);
        return frame;
    }

    void pushNewest(FramePtr frame) {
        mInterpolator->push_front(frame, frame->getTimestamp());
    }

private:
    void updateBufferIfNecessary(uint32_t now);
    uint32_t mPixelsPerFrame = 0;
    DataStreamPtr mStream;
    FrameInterpolatorPtr mInterpolator;
};

FASTLED_NAMESPACE_END

