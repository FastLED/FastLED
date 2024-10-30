#pragma once

#include "fx/detail/data_stream.h"
#include "fx/frame.h"
#include "namespace.h"
#include "fx/video/frame_interpolator.h"

FASTLED_NAMESPACE_BEGIN

FASTLED_SMART_REF(VideoStream);

class VideoStream : public Referent {
public:
    VideoStream(size_t pixelsPerFrame, size_t nFramesInBuffer, float fpsVideo);
    void begin(uint32_t now, FileHandleRef h);
    void beginStream(uint32_t now, ByteStreamRef s);
    void end();
    bool draw(uint32_t now, Frame* frame);
    bool draw(uint32_t now, CRGB* leds, uint8_t* alpha);
    bool Rewind();

    bool full() const {
        return mInterpolator->getFrames()->full();
    }

    FrameRef popOldest() {
        FrameRef frame;
        mInterpolator->pop_back(&frame);
        return frame;
    }

    void pushNewest(FrameRef frame) {
        mInterpolator->push_front(frame, frame->getTimestamp());
    }

private:
    void updateBufferIfNecessary(uint32_t now);
    uint32_t mPixelsPerFrame = 0;
    DataStreamRef mStream;
    FrameInterpolatorRef mInterpolator;
};

FASTLED_NAMESPACE_END

