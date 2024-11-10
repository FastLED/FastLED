
#pragma once

#include <stdint.h>

#include "file_system.h"
#include "ref.h"
#include "crgb.h"
#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

FASTLED_SMART_REF(FileBuffer);
FASTLED_SMART_REF(FileHandle);

class FileBuffer: public Referent {
 public:
  FileBuffer(FileHandleRef file);
  virtual ~FileBuffer();
  void rewindToStart();
  bool available() const;
  int32_t BytesLeft() const;
  int32_t FileSize() const;

  // Reads the next byte, else -1 is returned for end of buffer.
  int16_t read();
  size_t read(uint8_t* dst, size_t n);
  size_t read(CRGB* dst, size_t n) { return read((uint8_t*)dst, n * 3); }

 private:
  void ResetBuffer();
  void RefillBufferIfNecessary();
  void RefillBuffer();

  // Experimentally found to be as fast as larger values.
  static const int kBufferSize = 64;
  uint8_t mBuffer[kBufferSize];
  int16_t mCurrIdx;
  int16_t mLength;

  FileHandleRef mFile;
};

FASTLED_NAMESPACE_END
