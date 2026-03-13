#pragma once

#include "fl/file_system.h"
#include "fl/stl/shared_ptr.h"         // For FASTLED_SHARED_PTR macros
#include "fl/stl/int.h"
namespace fl {
class filebuf;
using filebuf_ptr = fl::shared_ptr<filebuf>;
} // namespace fl

namespace fl {

FASTLED_SHARED_PTR(PixelStream);

// PixelStream reads frames from a filebuf to serve data to the video system.
// A single handle is used for both seekable files and non-seekable streams.
// Seekability is auto-detected via seek() at begin() time.
class PixelStream {
  public:
    enum Type {
        kStreaming, // Non-seekable (e.g. memorybuf circular buffer)
        kFile,      // Seekable (e.g. posix_filebuf, SD card)
    };

    explicit PixelStream(int bytes_per_frame);

    // Opens a handle. Streaming vs seekable is auto-detected:
    // seek(0, beg) succeeds → kFile, fails → kStreaming.
    bool begin(fl::filebuf_ptr h);

    void close();
    i32 bytesPerFrame();
    bool readPixel(CRGB *dst);
    size_t readBytes(u8 *dst, size_t len);

    bool readFrame(Frame *frame);
    bool readFrameAt(fl::u32 frameNumber, Frame *frame);
    bool hasFrame(fl::u32 frameNumber);
    i32 framesRemaining() const; // -1 if this is a stream.
    i32 framesDisplayed() const;
    bool available() const;
    bool atEnd() const;

    i32 bytesRemaining() const;
    i32 bytesRemainingInFrame() const;
    bool rewind(); // Returns false for non-seekable streams.
    Type getType() const;

  private:
    fl::i32 mbytesPerFrame;
    fl::filebuf_ptr mHandle;
    Type mType;

  public:
    virtual ~PixelStream();
};

} // namespace fl
