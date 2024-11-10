#pragma once

#include "fx/detail/data_stream.h"
#include "fx/frame.h"
#include "namespace.h"
#include "fx/video/frame_interpolator.h"

FASTLED_NAMESPACE_BEGIN

FASTLED_SMART_REF(VideoImpl);
FASTLED_SMART_REF(ByteStream);
FASTLED_SMART_REF(DataStream);
FASTLED_SMART_REF(FileHandle);
FASTLED_SMART_REF(FrameInterpolator);
FASTLED_SMART_REF(Frame);

class VideoImpl : public Referent {
public:
    // frameHistoryCount is the number of frames to keep in the buffer after draw. This
    // allows for time based effects like syncing video speed to audio triggers.
    VideoImpl(size_t pixelsPerFrame, float fpsVideo, size_t frameHistoryCount = 0);
    ~VideoImpl();
    // Api
    void begin(FileHandleRef h);
    void beginStream(ByteStreamRef s);
    bool draw(uint32_t now, CRGB* leds, uint8_t* alpha = nullptr);
    void end();
    bool rewind();

    // internal use
    bool draw(uint32_t now, Frame* frame);
    bool full() const;
    FrameRef popOldest();
    void pushNewest(FrameRef frame);

private:
    void updateBufferIfNecessary(uint32_t now);
    uint32_t mPixelsPerFrame = 0;
    DataStreamRef mStream;
    FrameInterpolatorRef mInterpolator;
};

class Video {
public:
    // frameHistoryCount is the number of frames to keep in the buffer after draw. This
    // allows for time based effects like syncing video speed to audio triggers.
    Video();  // Please use FileSytem to construct a Video.
    ~Video();
    // Api
    void begin(FileHandleRef h, size_t pixelsPerFrame, float fps = 30.0f, size_t frameHistoryCount = 0);
    void beginStream(ByteStreamRef s, size_t pixelsPerFrame, float fps = 30.0f, size_t frameHistoryCount = 0);
    bool draw(uint32_t now, CRGB* leds, uint8_t* alpha = nullptr);
    void end();
    bool rewind();
    // make compatible with if statements
    operator bool() const { return mImpl.get(); }
private:
    VideoImplRef mImpl;
};

FASTLED_NAMESPACE_END

