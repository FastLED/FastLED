#pragma once

#include <stdint.h>
#include <stddef.h>
#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

namespace storage
{

// An abstract class that represents a file handle.
// Devices like the SD card will return one of these.
class FileHandle {
  public:
    virtual ~FileHandle() {}
    virtual bool available() const = 0;
    virtual size_t bytesLeft() const { return size() - pos(); }
    virtual size_t size() const = 0;
    virtual size_t read(uint8_t *dst, size_t bytesToRead) = 0;
    virtual size_t pos() const = 0;
    virtual const char* path() const = 0;
    virtual void seek(size_t pos) = 0;
    virtual void close() = 0;
};

} // namespace storage

FASTLED_NAMESPACE_END
