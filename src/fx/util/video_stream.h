#pragma once

#include "namespace.h"
#include "crgb.h"
#include "fx/storage/filereader.h"
#include "fx/storage/filebuffer.h"

FASTLED_NAMESPACE_BEGIN

// Warning, do NOT intialize VideoStream during static construction time!!
class VideoStream {
 public:
  explicit VideoStream(int bytes_per_frame);
  virtual ~VideoStream();
  bool begin(FileHandlePtr h);
  void Close();
  int32_t BytesPerFrame();
  bool ReadPixel(CRGB* dst);
  uint16_t ReadBytes(uint8_t* dst, uint16_t len);
  int32_t FramesRemaining() const;
  int32_t FramesDisplayed() const;

  int32_t BytesRemaining() const;
  int32_t BytesRemainingInFrame() const;
  void Rewind();
  
 private:
  void init(int bytes_per_frame);
  int32_t mBytesPerFrame;
  FileHandlePtr mFileHandle;
  FileBufferPtr mFileBuffer;
};

FASTLED_NAMESPACE_END
