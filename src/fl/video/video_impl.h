#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/span.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/string.h"  // For fl::string return type on embeddedScreenMapJson()

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
struct PixelSample;

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
              size_t frameHistoryCount = 0) FL_NO_EXCEPT;
    ~VideoImpl() FL_NO_EXCEPT;
    // Api
    bool begin(fl::filebuf_ptr h) FL_NO_EXCEPT;
    void setBestEffortFled(bool enabled) FL_NO_EXCEPT { mBestEffortFled = enabled; }
    void setFade(fl::u32 fadeInTime, fl::u32 fadeOutTime) FL_NO_EXCEPT;
    bool draw(fl::u32 now, fl::span<CRGB> leds) FL_NO_EXCEPT;
    void end() FL_NO_EXCEPT;
    bool rewind() FL_NO_EXCEPT;
    // internal use
    bool draw(fl::u32 now, Frame *frame) FL_NO_EXCEPT;
    bool full() const FL_NO_EXCEPT;
    void setTimeScale(float timeScale) FL_NO_EXCEPT;
    float timeScale() const FL_NO_EXCEPT { return mTimeScale; }
    size_t pixelsPerFrame() const FL_NO_EXCEPT { return mPixelsPerFrame; }
    void pause(fl::u32 now) FL_NO_EXCEPT;
    void resume(fl::u32 now) FL_NO_EXCEPT;
    bool needsFrame(fl::u32 now) const FL_NO_EXCEPT;
    i32 durationMicros() const FL_NO_EXCEPT; // -1 if this is a stream.

    // FLED v1 container accessors. Forwards to the underlying PixelStream;
    // empty / false for legacy headerless `.rgb` files.
    bool hasEmbeddedScreenMap() const FL_NO_EXCEPT;
    const fl::string &embeddedScreenMapJson() const FL_NO_EXCEPT;
    bool videoColor(fled::VideoColor *out) const FL_NO_EXCEPT;
    bool pixelStorage(fled::PixelStorage *out) const FL_NO_EXCEPT;
    bool readSample(PixelSample *out) FL_NO_EXCEPT;

  private:
    bool updateBufferIfNecessary(fl::u32 prev, fl::u32 now) FL_NO_EXCEPT;
    bool updateBufferFromFile(fl::u32 now, bool forward) FL_NO_EXCEPT;
    bool updateBufferFromStream(fl::u32 now) FL_NO_EXCEPT;
    fl::u32 mPixelsPerFrame = 0;
    PixelStreamPtr mStream;
    fl::u32 mPrevNow = 0;
    FrameInterpolatorPtr mFrameInterpolator;
    fl::TimeWarpPtr mTime;
    fl::u32 mFadeInTime = 1000;
    fl::u32 mFadeOutTime = 1000;
    float mTimeScale = 1.0f;
    bool mBestEffortFled = false;
};

} // namespace video
using VideoImpl = video::VideoImpl;
using VideoImplPtr = video::VideoImplPtr;
} // namespace fl
