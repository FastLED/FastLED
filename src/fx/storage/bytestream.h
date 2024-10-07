#pragma once

#include "namespace.h"
#include "ptr.h"
#include <stddef.h>
#include <stdint.h>
#include "crgb.h"

FASTLED_NAMESPACE_BEGIN

DECLARE_SMART_PTR(ByteStream);

// An abstract class that represents a file handle.
// Devices like the SD card will return one of these.
class ByteStream : public Referent {
  public:
    virtual ~ByteStream() {}
    virtual bool available() const = 0;
    virtual size_t read(uint8_t *dst, size_t bytesToRead) = 0;
    bool read(CRGB* dst);
    virtual const char *path() const = 0;
    virtual void close() {}  // default is do nothing on close.
};


inline bool ByteStream::read(CRGB* dst) {
    if (!available()) {
        return false;
    }
    CRGB out;
    if (1 != read(&out.r, 1)) {
        return false;
    }
    if (1 != read(&out.g, 1)) {
        return false;
    }
    if (1 != read(&out.b, 1)) {
        return false;
    }
    *dst = out;
    return true;
}


FASTLED_NAMESPACE_END
