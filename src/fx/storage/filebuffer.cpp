

#include "filebuffer.h"


FileBuffer::FileBuffer(FileHandlePtr fh) {
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

bool FileBuffer::Read(uint8_t* dst) {
  int16_t val = read();
  if (val == -1) {
    return false;
  }

  *dst = (uint8_t)(val & 0xff);
  return true;
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
