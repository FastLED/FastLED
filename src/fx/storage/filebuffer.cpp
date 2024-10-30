

#include "filebuffer.h"

#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

FileBuffer::FileBuffer(FileHandleRef fh) {
  mFile = fh;
  ResetBuffer();
}

FileBuffer::~FileBuffer() {
  if (mIsOpen) {
    mFile->close();
  }
}

void FileBuffer::RewindToStart() {
  mFile->seek(0);
  RefillBuffer();
}

bool FileBuffer::available() const {
  return (mIsOpen) && ((mCurrIdx != mLength) || mFile->available());
}

int32_t FileBuffer::BytesLeft() const {
  if (!available()) {
    return -1;
  }
  const int32_t remaining_buffer = mLength - mCurrIdx;
  const int32_t remaining_disk = mFile->size() - mFile->pos();
  return remaining_buffer + remaining_disk;
}

int32_t FileBuffer::FileSize() const {
  if (!available()) {
    return -1;
  }
  return mFile->size();
}

int16_t FileBuffer::read() {
  if (!mIsOpen) {
    return -1;
  }

  RefillBufferIfNecessary();
  if (mCurrIdx == mLength) {
    return -1;
  }

  // main case.
  uint8_t output = mBuffer[mCurrIdx++];
  return output;
}

size_t FileBuffer::read(uint8_t* dst, size_t n) {
  size_t bytes_read = 0;
  for (size_t i = 0; i < n; i++) {
    int16_t next_byte = read();
    if (next_byte == -1) {
      break;
    }
    dst[bytes_read++] = static_cast<uint8_t>(next_byte);
  }
  return bytes_read;
}

void FileBuffer::ResetBuffer() {
  mIsOpen = false;
  mLength = -1;
  mCurrIdx = -1;
}

void FileBuffer::RefillBufferIfNecessary() {
  if (mCurrIdx == mLength) {
    RefillBuffer();
  }
}

void FileBuffer::RefillBuffer() {
  if (!mFile->available()) {
    // ERR_PRINTLN("RefillBuffer FAILED");
  } else {
    // Needs more buffer yo.
    mLength = mFile->read(mBuffer, kBufferSize);
    mCurrIdx = 0;
  }
}

FASTLED_NAMESPACE_END
