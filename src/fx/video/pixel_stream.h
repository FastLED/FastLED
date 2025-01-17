#pragma once

#include "fl/namespace.h"
#include "crgb.h"
#include "fl/ptr.h"
#include "fl/bytestream.h"
#include "fl/file_system.h"
#include "fx/frame.h"
namespace fl {
FASTLED_SMART_PTR(FileHandle);
FASTLED_SMART_PTR(ByteStream);
}

namespace fl {

FASTLED_SMART_PTR(PixelStream);


// PixelStream takes either a file handle or a byte stream
// and reads frames from it in order to serve data to the
// video system.
class PixelStream: public fl::Referent {
 public:

  enum Type {
    kStreaming,
    kFile,
  };

  explicit PixelStream(int bytes_per_frame);

  bool begin(fl::FileHandlePtr h);
  bool beginStream(fl::ByteStreamPtr s);
  void close();
  int32_t bytesPerFrame();
  bool readPixel(CRGB* dst);  // Convenience function to read a pixel
  size_t readBytes(uint8_t* dst, size_t len);

  bool readFrame(Frame* frame);
  bool readFrameAt(uint32_t frameNumber, Frame* frame);
  bool hasFrame(uint32_t frameNumber);
  int32_t framesRemaining() const;  // -1 if this is a stream.
  int32_t framesDisplayed() const;
  bool available() const;
  bool atEnd() const;

  int32_t bytesRemaining() const;
  int32_t bytesRemainingInFrame() const;
  bool rewind();  // Returns false on failure, which can happen for streaming mode.
  Type getType() const;  // Returns the type of the video stream (kStreaming or kFile)
  
 private:
  int32_t mbytesPerFrame;
  fl::FileHandlePtr mFileHandle;
  fl::ByteStreamPtr mByteStream;
  bool mUsingByteStream;

protected:
  virtual ~PixelStream();
};

}  // namespace fl
