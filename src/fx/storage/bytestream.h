#pragma once

#include <stddef.h>
#include <stdint.h>

#include "namespace.h"
#include "ref.h"

#include "crgb.h"

FASTLED_NAMESPACE_BEGIN

FASTLED_SMART_REF(ByteStream);

// An abstract class that represents a file handle.
// Devices like the SD card will return one of these.
class ByteStream : public Referent {
  public:
    virtual ~ByteStream() {}
    virtual bool available(size_t) const = 0;
    virtual size_t read(uint8_t *dst, size_t bytesToRead) = 0;
    virtual const char *path() const = 0;
    virtual void close() {}  // default is do nothing on close.
};



FASTLED_NAMESPACE_END
