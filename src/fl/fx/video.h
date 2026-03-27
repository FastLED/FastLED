#pragma once

#include "fl/stl/stdint.h"

#include "fl/stl/shared_ptr.h"         // For FASTLED_SHARED_PTR macros
#include "fl/stl/span.h"
#include "fl/stl/string.h"
#include "fl/stl/detail/memory_file_handle.h"
#include "fl/fx/fx1d.h"
#include "fl/stl/noexcept.h"

namespace fl {

struct CRGB;
using CRGB = fl::CRGB;  // CRGB is now a typedef

// Forward declare classes
class filebuf;
using filebuf_ptr = fl::shared_ptr<filebuf>;
FASTLED_SHARED_PTR(Frame);
FASTLED_SHARED_PTR(VideoFxWrapper);
FASTLED_SHARED_PTR_NO_FWD(memorybuf);

namespace video {
class VideoImpl;
} // namespace video
using VideoImpl = video::VideoImpl;
using VideoImplPtr = fl::shared_ptr<VideoImpl>;

// Video represents a video file that can be played back on a LED strip.
// The video file is expected to be a sequence of frames. Pass any filebuf
// to begin() — streaming vs seekable is auto-detected.
class Video : public Fx1d { // Fx1d because video can be irregular.
  public:
    static size_t DefaultFrameHistoryCount() {
#ifdef FL_IS_AVR
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
    Video() FL_NOEXCEPT;
    Video(size_t pixelsPerFrame, float fps = 30.0f,
          size_t frameHistoryCount =
              DefaultFrameHistoryCount()); // Please use FileSytem to construct
                                           // a Video.
    ~Video() FL_NOEXCEPT;
    Video(const Video &) FL_NOEXCEPT;
    Video &operator=(const Video &) FL_NOEXCEPT;

    // Fx Api
    void draw(DrawContext context) override;
    string fxName() const override;

    // Api
    bool begin(fl::filebuf_ptr h);
    bool draw(fl::u32 now, fl::span<CRGB> leds);
    bool draw(fl::u32 now, Frame *frame);
    void end();
    bool finished();
    bool rewind();
    void setTimeScale(float timeScale);
    float timeScale() const;
    string error() const;
    void setError(const string &error) { mError = error; }
    size_t pixelsPerFrame() const;
    void pause(fl::u32 now) override;
    void resume(fl::u32 now) override;
    void setFade(fl::u32 fadeInTime, fl::u32 fadeOutTime);
    i32 durationMicros() const; // -1 if this is a stream.

    // make compatible with if statements
    operator bool() const { return mImpl.get(); }

  private:
    bool mFinished = false;
    VideoImplPtr mImpl;
    string mError;
    string mName;
};

// Wraps an Fx and stores a history of video frames. This allows
// interpolation between frames for FX for smoother effects.
// It also allows re-wind on fx that gnore time and always generate
// the next frame based on the previous frame and internal speed,
// for example NoisePalette.
class VideoFxWrapper : public Fx1d {
  public:
    VideoFxWrapper(FxPtr fx);
    ~VideoFxWrapper() FL_NOEXCEPT override;
    void draw(DrawContext context) override;
    string fxName() const override;
    void setFade(fl::u32 fadeInTime, fl::u32 fadeOutTime);

  private:
    FxPtr mFx;
    VideoImplPtr mVideo;
    memorybufPtr mByteStream;
    float mFps = 30.0f;
};

} // namespace fl
