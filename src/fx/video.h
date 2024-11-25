#pragma once

#include <stdint.h>

#include "namespace.h"
#include "fl/ptr.h"
#include "fx/fx1d.h"
#include "fx/time.h"
#include "fl/str.h"

FASTLED_NAMESPACE_BEGIN
struct CRGB;
FASTLED_NAMESPACE_END

namespace fl {

// Forward declare classes
FASTLED_SMART_PTR(FileHandle);
FASTLED_SMART_PTR(ByteStream);
FASTLED_SMART_PTR(VideoFx);
FASTLED_SMART_PTR(Frame);
FASTLED_SMART_PTR(VideoImpl);


// Video represents a video file that can be played back on a LED strip.
// The video file is expected to be a sequence of frames. You can either use
// a file handle or a byte stream to read the video data.
class Video {
public:
    static size_t DefaultFrameHistoryCount() {
        #ifdef __AVR__
        return 1;
        #else
        return 2;  // Allow interpolation by default.
        #endif
    }
    // frameHistoryCount is the number of frames to keep in the buffer after draw. This
    // allows for time based effects like syncing video speed to audio triggers.
    Video();
    Video(size_t pixelsPerFrame, float fps = 30.0f, size_t frameHistoryCount = DefaultFrameHistoryCount());  // Please use FileSytem to construct a Video.
    ~Video();
    Video(const Video&);
    Video& operator=(const Video&);
    // Api
    bool begin(fl::FileHandlePtr h);
    bool beginStream(fl::ByteStreamPtr s);
    bool draw(uint32_t now, CRGB* leds, uint8_t* alpha = nullptr);
    bool draw(uint32_t now, Frame* frame);
    void end();
    bool finished();
    bool rewind();
    void setTimeScale(float timeScale);
    float timeScale() const;
    fl::Str error() const;
    void setError(const fl::Str& error) { mError = error; }
    size_t pixelsPerFrame() const;
    void pause(uint32_t now);
    void resume(uint32_t now);

    // make compatible with if statements
    operator bool() const { return mImpl.get(); }
private:
    bool mFinished = false;
    VideoImplPtr mImpl;
    fl::Str mError;
};


// Fx1d because the video could be non rectangular or a strip.
class VideoFx : public Fx1d {
  public:
    VideoFx(Video video);
    void draw(DrawContext context) override;
    fl::Str fxName() const override;
    void pause(uint32_t now) override;
    void resume(uint32_t now) override;

  private:
    Video mVideo;
};


}  // namespace fl

