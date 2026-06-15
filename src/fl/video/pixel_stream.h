#pragma once

#include "fl/system/file_system.h"
#include "fl/stl/shared_ptr.h"         // For FASTLED_SHARED_PTR macros
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/string.h"
namespace fl {
class filebuf;
using filebuf_ptr = fl::shared_ptr<filebuf>;
} // namespace fl

namespace fl {
namespace video {

FASTLED_SHARED_PTR(PixelStream);

// PixelStream reads frames from a filebuf to serve data to the video system.
// A single handle is used for both seekable files and non-seekable streams.
// Seekability is auto-detected via seek() at begin() time.
//
// FLED container support (issue #3072): on seekable handles, begin() peeks
// the first 12 bytes for the "FLED" magic. If present, the header is
// consumed, the embedded screenmap JSON is stashed, and frame reads start
// past the header. Legacy headerless `.rgb` files keep working unchanged.
// Spec: https://github.com/zackees/ledmapper/blob/main/docs/fled-format.md
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

    // FLED v1 container accessors. True/non-empty only when begin() found
    // a valid FLED header on a seekable handle; otherwise empty / false.
    bool hasEmbeddedScreenMap() const FL_NOEXCEPT;
    const fl::string &embeddedScreenMapJson() const FL_NOEXCEPT;

  private:
    fl::i32 mbytesPerFrame;
    fl::filebuf_ptr mHandle;
    Type mType;

    // Byte offset of the first frame in the file. 0 for legacy headerless
    // `.rgb` files; 12 + jsonLength for FLED-formatted files.
    fl::size_t mPayloadOffset = 0;
    fl::string mEmbeddedScreenMapJson;

  public:
    virtual ~PixelStream() FL_NOEXCEPT;
};

} // namespace video
using PixelStream = video::PixelStream;
using PixelStreamPtr = video::PixelStreamPtr;
} // namespace fl
