#pragma once

#include "namespace.h"
#include "crgb.h"
#include "fx/storage/filereader.h"
#include "fx/storage/filebuffer.h"
#include "fx/storage/bytestream.h"

FASTLED_NAMESPACE_BEGIN

// Warning, do NOT intialize VideoStream during static construction time!!
class VideoStream {
 public:
  explicit VideoStream(int bytes_per_frame);
  virtual ~VideoStream();
  bool begin(FileHandlePtr h);
  bool begin(ByteStreamPtr s);
  void Close();
  int32_t BytesPerFrame();
  bool ReadPixel(CRGB* dst);
  size_t ReadBytes(uint8_t* dst, size_t len);
  int32_t FramesRemaining() const;
  int32_t FramesDisplayed() const;

  int32_t BytesRemaining() const;
  int32_t BytesRemainingInFrame() const;
  bool Rewind();  // Returns true on failure, which can happen for streaming mode.
  
 private:
  void init(int bytes_per_frame);
  int32_t mBytesPerFrame;
  FileHandlePtr mFileHandle;
  FileBufferPtr mFileBuffer;
  ByteStreamPtr mByteStream;
  bool mUsingByteStream;
};

FASTLED_NAMESPACE_END
