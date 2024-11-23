

#include "filebuffer.h"

#include "namespace.h"

#include "fl/dbg.h"

#define DBG FASTLED_DBG

FASTLED_NAMESPACE_BEGIN

FileBuffer::FileBuffer(FileHandlePtr fh) {
  mFile = fh;
}

FileBuffer::~FileBuffer() {
}

void FileBuffer::rewindToStart() {
  mFile->seek(0);
}

bool FileBuffer::available() const {
  return mFile->available();
}

size_t FileBuffer::bytesLeft() const {
  if (!available()) {
    return 0;
  }
  return mFile->size() - mFile->pos();
}

uint32_t FileBuffer::Position() const {
  return mFile->pos();
}

bool FileBuffer::seek(uint32_t pos) {
  return mFile->seek(pos);
}

int32_t FileBuffer::FileSize() const {
  return mFile->size();
}

size_t FileBuffer::read(uint8_t* dst, size_t n) {
  return mFile->read(dst, n);
}

size_t FileBuffer::readCRGB(CRGB* dst, size_t n) {
  return read((uint8_t*)dst, n * 3);
}



FASTLED_NAMESPACE_END
