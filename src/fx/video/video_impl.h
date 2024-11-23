#pragma once

#include <stdint.h>
#include "fx/storage/bytestream.h"
#include "fx/video/pixel_stream.h"
#include "fx/video/frame_interpolator.h"
#include "fl/file_system.h"

#include "namespace.h"

namespace fl {
FASTLED_SMART_PTR(FileHandle);
}

FASTLED_NAMESPACE_BEGIN

FASTLED_SMART_PTR(VideoImpl);
FASTLED_SMART_PTR(ByteStream);
FASTLED_SMART_PTR(FrameInterpolator);
FASTLED_SMART_PTR(PixelStream)

class VideoImpl : public fl::Referent {
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
    void begin(fl::FileHandlePtr h);
    void beginStream(ByteStreamPtr s);
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
    PixelStreamPtr mStream;
    uint32_t mPrevNow = 0;
    FrameInterpolatorPtr mFrameInterpolator;
    TimeScalePtr mTimeScale;
};

FASTLED_NAMESPACE_END
