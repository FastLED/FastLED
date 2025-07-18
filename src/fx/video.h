#pragma once

#include "fl/stdint.h"

#include "fl/namespace.h"
#include "fl/memory.h"
#include "fl/str.h"
#include "fx/fx1d.h"
#include "fx/time.h"

FASTLED_NAMESPACE_BEGIN
struct CRGB;
FASTLED_NAMESPACE_END

namespace fl {

// Forward declare classes
FASTLED_SMART_PTR(FileHandle);
FASTLED_SMART_PTR(ByteStream);
FASTLED_SMART_PTR(Frame);
FASTLED_SMART_PTR(VideoImpl);
FASTLED_SMART_PTR(VideoFxWrapper);
FASTLED_SMART_PTR(ByteStreamMemory);

// Video represents a video file that can be played back on a LED strip.
// The video file is expected to be a sequence of frames. You can either use
// a file handle or a byte stream to read the video data.
class Video : public Fx1d { // Fx1d because video can be irregular.
  public:
    static size_t DefaultFrameHistoryCount() {
#ifdef __AVR__
        return 1;
#else
        return 2; // Allow interpolation by default.
#endif
    }
    // frameHistoryCount is the number of frames to keep in the buffer after
    // draw. This allows for time based effects like syncing video speed to
    // audio triggers. If you are using a filehandle for you video then you can
    // just leave this as the default. For streaming byte streams you may want
    // to increase this number to allow momentary re-wind. If you'd like to use
    // a Video as a buffer for an fx effect then please see VideoFxWrapper.
    Video();
    Video(size_t pixelsPerFrame, float fps = 30.0f,
          size_t frameHistoryCount =
              DefaultFrameHistoryCount()); // Please use FileSytem to construct
                                           // a Video.
    ~Video();
    Video(const Video &);
    Video &operator=(const Video &);

    // Fx Api
    void draw(DrawContext context) override;
    Str fxName() const override;

    // Api
    bool begin(fl::FileHandlePtr h);
    bool beginStream(fl::ByteStreamPtr s);
    bool draw(fl::u32 now, CRGB *leds);
    bool draw(fl::u32 now, Frame *frame);
    void end();
    bool finished();
    bool rewind();
    void setTimeScale(float timeScale);
    float timeScale() const;
    Str error() const;
    void setError(const Str &error) { mError = error; }
    size_t pixelsPerFrame() const;
    void pause(fl::u32 now) override;
    void resume(fl::u32 now) override;
    void setFade(fl::u32 fadeInTime, fl::u32 fadeOutTime);
    int32_t durationMicros() const; // -1 if this is a stream.

    // make compatible with if statements
    operator bool() const { return mImpl.get(); }

  private:
    bool mFinished = false;
    VideoImplPtr mImpl;
    Str mError;
    Str mName;
};

// Wraps an Fx and stores a history of video frames. This allows
// interpolation between frames for FX for smoother effects.
// It also allows re-wind on fx that gnore time and always generate
// the next frame based on the previous frame and internal speed,
// for example NoisePalette.
class VideoFxWrapper : public Fx1d {
  public:
    VideoFxWrapper(FxPtr fx);
    ~VideoFxWrapper() override;
    void draw(DrawContext context) override;
    Str fxName() const override;
    void setFade(fl::u32 fadeInTime, fl::u32 fadeOutTime);

  private:
    FxPtr mFx;
    VideoImplPtr mVideo;
    ByteStreamMemoryPtr mByteStream;
    float mFps = 30.0f;
};

} // namespace fl
