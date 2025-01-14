#pragma once

#include <stdint.h>
#include "fl/bytestream.h"
#include "fx/video/pixel_stream.h"
#include "fx/video/frame_interpolator.h"
#include "fl/file_system.h"

#include "fl/namespace.h"

namespace fl {
FASTLED_SMART_PTR(FileHandle);
FASTLED_SMART_PTR(ByteStream);
}

namespace fl {

FASTLED_SMART_PTR(VideoImpl);
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
    void beginStream(fl::ByteStreamPtr s);
    void setFade(uint32_t fadeInTime, uint32_t fadeOutTime);
    bool draw(uint32_t now, CRGB *leds);
    void end();
    bool rewind();
    // internal use
    bool draw(uint32_t now, Frame *frame);
    bool full() const;
    void setTimeScale(float timeScale);
    float timeScale() const { return mTimeScale; }
    size_t pixelsPerFrame() const { return mPixelsPerFrame; }
    void pause(uint32_t now);
    void resume(uint32_t now);
    bool needsFrame(uint32_t now) const;
    int32_t durationMicros() const;  // -1 if this is a stream.


  private:
    bool updateBufferIfNecessary(uint32_t prev, uint32_t now);
    bool updateBufferFromFile(uint32_t now, bool forward);
    bool updateBufferFromStream(uint32_t now);
    uint32_t mPixelsPerFrame = 0;
    PixelStreamPtr mStream;
    uint32_t mPrevNow = 0;
    FrameInterpolatorPtr mFrameInterpolator;
    TimeScalePtr mTime;
    uint32_t mFadeInTime = 1000;
    uint32_t mFadeOutTime = 1000;
    float mTimeScale = 1.0f;
};

}  // namespace fl
