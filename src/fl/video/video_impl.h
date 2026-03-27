#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/span.h"
#include "fl/stl/noexcept.h"

// Forward declarations - actual includes moved to cpp
namespace fl {
class filebuf;
class Frame;
class TimeWarp;

using filebuf_ptr = fl::shared_ptr<filebuf>;
FASTLED_SHARED_PTR(TimeWarp);
} // namespace fl

namespace fl {
namespace video {

class FrameInterpolator;
class PixelStream;

FASTLED_SHARED_PTR(VideoImpl);
FASTLED_SHARED_PTR(FrameInterpolator);
FASTLED_SHARED_PTR(PixelStream);

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
    ~VideoImpl() FL_NOEXCEPT;
    // Api
    void begin(fl::filebuf_ptr h);
    void setFade(fl::u32 fadeInTime, fl::u32 fadeOutTime);
    bool draw(fl::u32 now, fl::span<CRGB> leds);
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
    i32 durationMicros() const; // -1 if this is a stream.

  private:
    bool updateBufferIfNecessary(fl::u32 prev, fl::u32 now);
    bool updateBufferFromFile(fl::u32 now, bool forward);
    bool updateBufferFromStream(fl::u32 now);
    fl::u32 mPixelsPerFrame = 0;
    PixelStreamPtr mStream;
    fl::u32 mPrevNow = 0;
    FrameInterpolatorPtr mFrameInterpolator;
    fl::TimeWarpPtr mTime;
    fl::u32 mFadeInTime = 1000;
    fl::u32 mFadeOutTime = 1000;
    float mTimeScale = 1.0f;
};

} // namespace video
using VideoImpl = video::VideoImpl;
using VideoImplPtr = video::VideoImplPtr;
} // namespace fl
