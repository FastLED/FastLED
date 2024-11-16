#pragma once

#include "namespace.h"
#include "crgb.h"
#include "ref.h"
#include "fx/storage/filebuffer.h"
#include "fx/storage/bytestream.h"
#include "fx/frame.h"
#include "file_system.h"

FASTLED_NAMESPACE_BEGIN

FASTLED_SMART_REF(DataStream);
FASTLED_SMART_REF(ByteStream);
FASTLED_SMART_REF(FileHandle);
FASTLED_SMART_REF(FileBuffer);

// DataStream takes either a file handle or a byte stream
// and reads frames from it in order to serve data to the
// video system.
class DataStream: public Referent {
 public:

  enum Type {
    kStreaming,
    kFile,
  };

  explicit DataStream(int bytes_per_frame);

  bool begin(FileHandleRef h);
  bool beginStream(ByteStreamRef s);
  void close();
  int32_t bytesPerFrame();
  bool readPixel(CRGB* dst);  // Convenience function to read a pixel
  size_t readBytes(uint8_t* dst, size_t len);

  bool readFrame(Frame* frame);
  int32_t framesRemaining() const;
  int32_t framesDisplayed() const;
  bool available() const;
  bool atEnd() const;

  int32_t bytesRemaining() const;
  int32_t bytesRemainingInFrame() const;
  bool rewind();  // Returns false on failure, which can happen for streaming mode.
  Type getType() const;  // Returns the type of the video stream (kStreaming or kFile)
  
 private:
  void init(int bytes_per_frame);
  int32_t mbytesPerFrame;
  FileHandleRef mFileHandle;
  FileBufferRef mFileBuffer;
  ByteStreamRef mByteStream;
  bool mUsingByteStream;

protected:
  virtual ~DataStream();
};

FASTLED_NAMESPACE_END
