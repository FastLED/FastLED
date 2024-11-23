#pragma once

#include <stdint.h>
#include "fx/storage/bytestream.h"
#include "fx/detail/data_stream.h"
#include "fx/video/frame_interpolator.h"
#include "file_system.h"

#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

FASTLED_SMART_REF(VideoImpl);
FASTLED_SMART_REF(FileHandle);
FASTLED_SMART_REF(ByteStream);
FASTLED_SMART_REF(FrameInterpolator);
FASTLED_SMART_REF(DataStream)

class VideoImpl : public Referent {
  public:
    enum {
        kSizeRGB8 = 3,
    };
    // frameHistoryCount is the number of frames to keep in the buffer after
    // draw. This allows for time based effects like syncing video speed to
    // audio triggers.
    VideoImpl(size_t pixelsPerFrame, float fpsVideo,
              size_t frameHistoryCount = 0);
    ~VideoImpl();
    // Api
    void begin(FileHandleRef h);
    void beginStream(ByteStreamRef s);
    bool draw(uint32_t now, CRGB *leds, uint8_t *alpha = nullptr);
    void end();
    bool rewind();
    // internal use
    bool draw(uint32_t now, Frame *frame);
    bool full() const;
    void setTimeScale(float timeScale);
    float timeScale() const { return mTimeScale ? mTimeScale->scale() : 1.0f; }

  private:
    bool updateBufferIfNecessary(uint32_t prev, uint32_t now);
    uint32_t mPixelsPerFrame = 0;
    DataStreamRef mStream;
    uint32_t mPrevNow = 0;
    FrameInterpolatorRef mFrameInterpolator;
    TimeScaleRef mTimeScale;
};

FASTLED_NAMESPACE_END
