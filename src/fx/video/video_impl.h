#pragma once

#include "fl/bytestream.h"
#include "fl/file_system.h"
#include "fx/video/frame_interpolator.h"
#include "fx/video/pixel_stream.h"
#include "fl/stdint.h"

#include "fl/namespace.h"

namespace fl {
FASTLED_SMART_PTR(FileHandle);
FASTLED_SMART_PTR(ByteStream);
} // namespace fl

namespace fl {

FASTLED_SMART_PTR(VideoImpl);
FASTLED_SMART_PTR(FrameInterpolator);
FASTLED_SMART_PTR(PixelStream)

class VideoImpl {
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
    void setFade(fl::u32 fadeInTime, fl::u32 fadeOutTime);
    bool draw(fl::u32 now, CRGB *leds);
    void end();
    bool rewind();
    // internal use
    bool draw(fl::u32 now, Frame *frame);
    bool full() const;
    void setTimeScale(float timeScale);
    float timeScale() const { return mTimeScale; }
    size_t pixelsPerFrame() const { return mPixelsPerFrame; }
    void pause(fl::u32 now);
    void resume(fl::u32 now);
    bool needsFrame(fl::u32 now) const;
    int32_t durationMicros() const; // -1 if this is a stream.

  private:
    bool updateBufferIfNecessary(fl::u32 prev, fl::u32 now);
    bool updateBufferFromFile(fl::u32 now, bool forward);
    bool updateBufferFromStream(fl::u32 now);
    fl::u32 mPixelsPerFrame = 0;
    PixelStreamPtr mStream;
    fl::u32 mPrevNow = 0;
    FrameInterpolatorPtr mFrameInterpolator;
    TimeWarpPtr mTime;
    fl::u32 mFadeInTime = 1000;
    fl::u32 mFadeOutTime = 1000;
    float mTimeScale = 1.0f;
};

} // namespace fl
